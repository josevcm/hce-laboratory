/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

  HCE-LABORATORY is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HCE-LABORATORY is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with HCE-LABORATORY. If not, see <http://www.gnu.org/licenses/>.

*/

#include <windows.h>

#include <rt/Logger.h>

#include <hw/proto/HSU.h>

#define DEFAULT_TIMEOUT 50 // milliseconds

namespace hw {

struct HSU::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.HSU");

   std::string device;

   HANDLE handle = INVALID_HANDLE_VALUE;

   enum Mode
   {
      Normal,
      LowPower,
      PowerDown
   } powerMode = LowPower;

   const rt::ByteBuffer ackFrame {0x00, 0x00, 0xff, 0x00, 0xff, 0x00};
   const rt::ByteBuffer nackFrame {0x00, 0x00, 0xff, 0xff, 0x00, 0x00};
   const rt::ByteBuffer errorFrame {0x00, 0x00, 0xff, 0x01, 0xff, 0x7f, 0x81, 0x00};
   const rt::ByteBuffer wakeUpFrame {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

   ~Impl()
   {
      close();
   }

   int open(const std::string &name, const std::string &config)
   {
      close();

      const HANDLE hComm = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

      if (hComm == INVALID_HANDLE_VALUE)
      {
         log->error("serial port {} cannot be opened, error: {}", {name, static_cast<unsigned int>(GetLastError())});
         return -1;
      }

      DCB dcbSerialParams = {};
      dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

      if (!BuildCommDCBA(config.c_str(), &dcbSerialParams))
      {
         log->error("unable to build serial port {} parameters, error: {}", {name, static_cast<unsigned int>(GetLastError())});
         close();
         return -2;
      }

      if (!SetCommState(hComm, &dcbSerialParams))
      {
         log->error("unable to set serial port {} parameters, error: {}", {name, static_cast<unsigned int>(GetLastError())});
         close();
         return -3;
      }

      COMMTIMEOUTS timeouts = {};
      timeouts.ReadIntervalTimeout = 30;
      timeouts.ReadTotalTimeoutConstant = 30;
      timeouts.ReadTotalTimeoutMultiplier = 0;
      timeouts.WriteTotalTimeoutConstant = 30;
      timeouts.WriteTotalTimeoutMultiplier = 0;

      if (!SetCommTimeouts(hComm, &timeouts))
      {
         log->error("setting serial port {} timeouts, error: ", {name, static_cast<unsigned int>(GetLastError())});
         close();
         return -4;
      }

      PurgeComm(hComm, PURGE_RXABORT | PURGE_RXCLEAR);

      this->device = name;
      this->handle = hComm;

      log->info("serial port {} opened at {}", {name, config});

      return 0;
   }

   void close()
   {
      if (handle != INVALID_HANDLE_VALUE)
      {
         CloseHandle(handle);
         handle = INVALID_HANDLE_VALUE;
         log->info("serial port {} closed", {device});
      }
   }

   int transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)
   {
      if (handle == INVALID_HANDLE_VALUE)
         return -1;

      // purge read buffer before write
      purge();

      // send command and receive ack
      if (const int r = send(cmd, timeout); r != 0)
         return r;

      if (const int r = recv(res, timeout); r != 0)
         return r;

      return 0;
   }

   int send(const rt::ByteBuffer &cmd, int timeout)
   {
      // check if we need to wake up the PN532
      if (powerMode != Normal)
      {
         if (write(wakeUpFrame, 100) <= 0)
         {
            log->error("serial port {} write error", {device});
            return -1;
         }

         timeout += 1000;

         powerMode = Normal;
      }

      // ATB frames
      rt::ByteBuffer tx(255);
      rt::ByteBuffer ack(6);

      // compute data checksum
      const auto dcs = cmd.reduce<unsigned char>(0, [](unsigned char a, unsigned char d) { return a + d; });

      // prepare command frame
      tx.put({0x00, 0x00, 0xff}); // preamble + start code
      tx.put(cmd.remaining()); // length includes TFI + PD0 (CC+1)
      tx.put(256 - cmd.remaining()); // length checksum (LCS)
      tx.put(cmd.ptr(), cmd.remaining());
      tx.put(256 - dcs); // DCS
      tx.put({0x00}); // postamble
      tx.flip();

      if (write(tx, 100) <= 0)
      {
         log->error("serial port {} write error", {device});
         return -1;
      }

      switch (cmd[1])
      {
         // PowerDown
         case 0x16:
            powerMode = PowerDown;
            break;

         // TgInitAsTarget, PN532 automatically goes into PowerDown mode
         case 0x8c:
            powerMode = PowerDown;
            break;
      }

      // read ack frame
      if (const int r = read(ack, timeout); r <= 0)
         return r;

      // PN532 automatically wakeup when TgInitAsTarget detect external RF
      if (cmd[1] == 0x8c)
         powerMode = Normal;

      // check ack frame
      if (ack.limit() < 6)
      {
         log->error("invalid ack response length {}", {ack.limit()});
         return -1;
      }

      return ack == ackFrame ? 0 : -1;
   }

   int recv(rt::ByteBuffer &res, int timeout)
   {
      // ATB frame header
      rt::ByteBuffer header(5);
      rt::ByteBuffer postamble(1);

      // read header
      if (const int r = read(header, timeout); r <= 0)
         return r;

      // check header
      if (header[0] != 0x00 || header[1] != 0x00 || header[2] != 0xff)
      {
         log->error("invalid frame preamble or start code: {}", {header});
         return -1;
      }

      while (true)
      {
         int length = 0;

         // extended frame
         if (header[3] == 0xff && header[4] == 0xff)
         {
            // reset buffer and reserve space to read 3 bytes
            header.flip().room(3);

            // read extended length bytes
            if (const int r = read(header, 100); r <= 0)
            {
               log->error("serial port {} read extended header error: {}", {device, r});
               return r;
            }

            if (header[0] + header[1] + header[2] != 256)
            {
               log->error("length checksum mismatch: {}", {header});
               break;
            }

            length = (header[0] << 8) + header[1];
         }

         // normal frame
         else
         {
            if (header[3] + header[4] != 256)
            {
               log->error("length checksum mismatch: {}", {header});
               break;
            }

            length = header[3];
         }

         // read remaining frame (TFI + PDn + DCS)
         rt::ByteBuffer body(length + 1);

         // read header
         if (const int r = read(body, 100); r <= 0)
         {
            log->error("serial port {} read data error: {}", {device, r});
            return -1;
         }

         // calculate data checksum and check
         const auto dcs = body.reduce<unsigned char>(0, [](unsigned char a, unsigned char d) { return a + d; });

         if (dcs != 0)
         {
            log->error("data checksum mismatch: {}", {body});
            break;
         }

         // copy response to output buffer
         res.put(body.ptr(), body.remaining() - 1);
         res.flip();

         // read postamble
         if (const int r = read(postamble, 100); r <= 0)
         {
            log->error("serial port {} read postamble error: {}", {device, r});
            return -1;
         }

         if (postamble[0] != 0x00)
         {
            log->error("invalid frame postamble: {}", {postamble});
            break;
         }

         return 0;
      }

      purge();

      return -1;
   }

   int read(rt::ByteBuffer &data, int timeout) const
   {
      COMMTIMEOUTS timeouts;
      timeouts.ReadIntervalTimeout = 0;
      timeouts.ReadTotalTimeoutMultiplier = 0;
      timeouts.ReadTotalTimeoutConstant = timeout;
      timeouts.WriteTotalTimeoutMultiplier = 0;
      timeouts.WriteTotalTimeoutConstant = timeout;

      if (!SetCommTimeouts(handle, &timeouts))
      {
         log->error("unable to apply new timeout settings, error: {}", {static_cast<unsigned int>(GetLastError())});
         return -1;
      }

      DWORD totalReaded = 0;
      DWORD bytesReaded = 0;

      do
      {
         bytesReaded = 0;

         const BOOL ok = ReadFile(handle, data.ptr(), data.remaining(), &bytesReaded, nullptr);

         if (!ok)
         {
            log->error("unable to read from serial port {}, error: {}", {device, static_cast<unsigned int>(GetLastError())});
            return -1;
         }

         // add readed bytes
         totalReaded += bytesReaded;

         // timeout with no data
         if (totalReaded == 0)
            return -2;

         // move buffer position
         data.push(bytesReaded);
      }
      while (bytesReaded > 0 && data.remaining() > 0);

      data.flip();

      LOG_DEBUG(log, "RX << {x}", {data});

      return static_cast<int>(totalReaded);
   }

   int write(const rt::ByteBuffer &data, int timeout) const
   {
      DWORD bytesWritten = 0;

      LOG_DEBUG(log, "TX >> {x}", {data});

      COMMTIMEOUTS timeouts;
      timeouts.ReadIntervalTimeout = 0;
      timeouts.ReadTotalTimeoutMultiplier = 0;
      timeouts.ReadTotalTimeoutConstant = timeout;
      timeouts.WriteTotalTimeoutMultiplier = 0;
      timeouts.WriteTotalTimeoutConstant = timeout;

      if (!SetCommTimeouts(handle, &timeouts))
      {
         log->error("unable to apply new timeout settings.");
         return -1;
      }

      // write data
      if (const BOOL ok = WriteFile(handle, data.data(), data.size(), &bytesWritten, nullptr); !ok)
      {
         log->error("unable to write data to serial port.");
         return -2;
      }

      return static_cast<int>(bytesWritten);
   }

   void purge() const
   {
      PurgeComm(handle, PURGE_RXABORT | PURGE_RXCLEAR);
   }
};

HSU::HSU() : impl(std::make_shared<Impl>())
{
}

int HSU::open(const std::string &device, const std::string &config)
{
   return impl->open(device, config);
}

void HSU::close()
{
   return impl->close();
}

int HSU::transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)
{
   return impl->transmit(cmd, res, timeout);
}

}
