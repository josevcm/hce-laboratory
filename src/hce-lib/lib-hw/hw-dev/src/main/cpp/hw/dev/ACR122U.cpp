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

#include <hw/dev/PCSC.h>
#include <hw/dev/ACR122U.h>

#include <rt/Logger.h>

#define DEFAULT_READER_NAME "ACR122"

namespace hw {

struct ACR122U::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.ACR122U");

   PCSC pcsc;

   // Destructor to release the context
   ~Impl()
   {
      close();
   }

   int open(PCSC::Mode mode, const std::string &reader)
   {
      LOG_INFO(log, "connecting to {} ACR122U reader", {reader.empty() ? "<any>" : reader});

      // list available readers and connect to specified or first available
      const std::vector<std::string> readers = pcsc.listReaders();

      // find reader
      for (const auto &name: readers)
      {
         if (!reader.empty() && name != reader)
            continue;

         if (reader.empty() && name.find(DEFAULT_READER_NAME) == std::string::npos)
            continue;

         LOG_INFO(log, "found reader '{}', connecting...", {name});

         if (pcsc.connect(name, mode, mode == PCSC::Direct ? PCSC::None : PCSC::Any) == 0)
         {
            LOG_INFO(log, "connected to reader '{}'", {name});
            return 0;
         }
      }

      log->warn("unable to connect to {} ACR122U reader", {reader.empty() ? "<any>" : reader});

      return -1;
   }

   int close()
   {
      return pcsc.disconnect();
   }

   int directTx(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)
   {
      rt::ByteBuffer directCmd(5 + cmd.remaining());

      directCmd.put({0xFF, 0x00, 0x00, 0x00}); // CLA INS P1 P2 , direct transmit
      directCmd.put(static_cast<unsigned char>(cmd.limit())); // Lc
      directCmd.put(cmd.data(), cmd.limit()); // Data
      directCmd.flip();

      if (pcsc.control(PCSC::IOCTL_CCID_ESCAPE, directCmd, res) != 0)
      {
         log->error("error sending direct command to ACR122U reader");
         return -1;
      }

      // check if SW1 SW2 is 9000
      if (res.limit() < 2 || res[res.limit() - 2] != 0x90 || res[res.limit() - 1] != 0x00)
      {
         LOG_DEBUG(log, "error in direct command response from ACR122U reader: {x}", {res});
         return -1;
      }

      // remove status bytes
      res.push(res.limit() - 2);
      res.flip();

      return 0;
   }

   int setParameters(int value)
   {
      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xFF, 0x00, 0x51}).put(value).put(0x00); // CLA INS P1 P2=value, Lc=0
      cmd.flip();

      if (pcsc.control(PCSC::IOCTL_CCID_ESCAPE, cmd, res) != 0)
      {
         log->error("error sending operating parameters command to ACR122U reader");
         return -1;
      }

      // check if SW1 SW2 is 9000
      if (res.limit() < 2 || res.data()[res.limit() - 2] != 0x90 || res.data()[res.limit() - 1] != 0x00)
      {
         log->error("error in setting operating parameters from ACR122U reader: {x}", {res});
         return -1;
      }

      return 0;
   }
};

ACR122U::ACR122U() : impl(std::make_shared<Impl>())
{
}

// connect to card reader, if no reader specified, connect to first available
int ACR122U::open(PCSC::Mode mode, const std::string &reader)
{
   return impl->open(mode, reader);
}

// close connection to card reader
void ACR122U::close()
{
   impl->close();
}

// detect card presence
int ACR122U::transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)
{
   return impl->directTx(cmd, res, timeout);
}

// configure operation parameters
int ACR122U::setParameters(int value)
{
   return impl->setParameters(value);
}

}
