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

#include  <rt/Logger.h>

#include <hw/ic/PN532.h>

#include <utility>

namespace hw {

struct PN532::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.PN532");

   TransmitFunction transmitFunction;

   Impl(TransmitFunction transmitFunction) : transmitFunction(std::move(transmitFunction))
   {
   }

   int getFirmwareVersion(FwVersion &version)
   {
      LOG_DEBUG(log, "getFirmwareVersion");

      rt::ByteBuffer cmd({0xD4, 0x02});
      rt::ByteBuffer res(256);

      if (transmit(cmd, res, 500) != 0)
         return -1;

      if (res.limit() != 6)
      {
         log->error("invalid get firmware version response length {}", {res.limit()});
         return -1;
      }

      version.ic = res.data()[2];
      version.ver = res.data()[3];
      version.rev = res.data()[4];
      version.support = res.data()[5];

      return 0;
   }

   int getGeneralStatus(GeneralStatus &status)
   {
      LOG_DEBUG(log, "getGeneralStatus");

      rt::ByteBuffer cmd({0xD4, 0x04});
      rt::ByteBuffer res(256);

      if (transmit(cmd, res, 500) != 0)
         return -1;

      if (res.limit() < 6)
      {
         log->error("invalid get general status response length {}", {res.limit()});
         return -1;
      }

      int offset = 2;

      status.err = res.ptr()[offset++];
      status.field = res.ptr()[offset++];
      status.nbTg = res.ptr()[offset++];

      if (status.nbTg > 0)
      {
         status.tg1Id = res.ptr()[offset++];
         status.tg1BrRx = res.ptr()[offset++];
         status.tg1BrTx = res.ptr()[offset++];
         status.tg1Type = res.ptr()[offset++];
      }

      if (status.nbTg > 1)
      {
         status.tg2Id = res.ptr()[offset++];
         status.tg2BrRx = res.ptr()[offset++];
         status.tg2BrTx = res.ptr()[offset++];
         status.tg2Type = res.ptr()[offset++];
      }

      status.sam = res.ptr()[offset++];

      return 0;
   }

   int setParameters(int value)
   {
      LOG_DEBUG(log, "setParameters 0x{02x}", {value});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x12}).put(value).flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() != 2)
      {
         log->error("invalid set parameters response length {}", {res.limit()});
         return -1;
      }

      return 0;
   }

   int setSAMConfiguration(int mode, int timeout, int irq)
   {
      LOG_DEBUG(log, "setSAMConfiguration, mode=0x{02x}, timeout={02x}, irq={02x}", {mode, timeout, irq});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x14});
      cmd.put(mode);
      cmd.put(timeout);
      cmd.put(irq);
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() != 2)
      {
         log->error("invalid set SAM configuration response length {}", {res.limit()});
         return -1;
      }

      return 0;
   }

   int powerDown(int wakeUpEnable, int triggerIrq)
   {
      LOG_DEBUG(log, "powerDown");

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x16});
      cmd.put(wakeUpEnable);
      cmd.put(triggerIrq);
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() != 2)
      {
         log->error("invalid powerDown response length {}", {res.limit()});
         return -1;
      }

      return 0;
   }

   int tgInitAsTarget(int &mode, rt::ByteBuffer &init)
   {
      LOG_DEBUG(log, "tgInitAsTarget");

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x8C, 0x04}); // PICC Only=0x04, DEP Only=0x02, Passive Only=0x01
      cmd.put({0x04, 0x00, 0x12, 0x34, 0x56, 0x20}); // MifareParams
      cmd.put({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}); // FELICAParams
      cmd.put({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}); // NFCID3t
      cmd.put({0x00}); // LEN_Gt
      cmd.put({0x00}); // LEN_Tk
      cmd.flip();

      if (const int r = transmit(cmd, res, 5000); r < 0)
         return r;

      if (res.limit() < 2)
      {
         log->error("invalid tgInitAsTarget response length {}", {res.limit()});
         return -1;
      }

      mode = res.data()[2];

      init.put(res.data() + 3, res.limit() - 3);
      init.flip();

      return 0;
   }

   int tgResponseToInitiator(rt::ByteBuffer &data, int &status)
   {
      LOG_DEBUG(log, "tgResponseToInitiator: {x}", {data});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x90}).put(data.data(), data.limit());
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() < 3)
      {
         log->error("invalid tgResponseToInitiator response length {}", {res.limit()});
         return -1;
      }

      status = res.data()[2];

      return 0;
   }

   int tgGetData(rt::ByteBuffer &data, int &status)
   {
      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x86}).flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() < 3)
      {
         log->error("invalid tgGetData response length {}", {res.limit()});
         return -1;
      }

      status = res.data()[2];

      data.put(res.data() + 3, res.limit() - 3);
      data.flip();

#ifdef PN532_DEBUG
      LOG_DEBUG(log, "tgGetData: {x}", {data});
#endif

      return 0;
   }

   int tgSetData(const rt::ByteBuffer &data, int &status)
   {
      LOG_DEBUG(log, "tgSetData: {x}", {data});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x8E}).put(data.data(), data.limit());
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() < 3)
      {
         log->error("invalid tgGetData response length {}", {res.limit()});
         return -1;
      }

      status = res.data()[2];

      return 0;
   }

   int readRegister(int reg, int &value)
   {
      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x06});
      cmd.put(reg >> 8 & 0xff).put(reg & 0xff);
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() != 3)
      {
         log->error("invalid read register response length {}", {res.limit()});
         return -1;
      }

      value = res.data()[2];

      LOG_DEBUG(log, "read register 0x{04x}: 0x{02x}", {reg, value});

      return 0;
   }

   int writeRegister(int reg, int value)
   {
      LOG_DEBUG(log, "write register 0x{04x}: 0x{02x}", {reg, value});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer res(256);

      cmd.put({0xD4, 0x08});
      cmd.put(reg >> 8 & 0xff).put(reg & 0xff).put(value);
      cmd.flip();

      if (transmit(cmd, res, 0) != 0)
         return -1;

      if (res.limit() != 2)
      {
         log->error("invalid write register response length {}", {res.limit()});
         return -1;
      }

      return 0;
   }

   int transmit(rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout) const
   {
      if (!transmitFunction)
      {
         log->error("no transmit function defined");
         return -1;
      }

      LOG_DEBUG(log, "TX >> {x}", {cmd});

      switch (transmitFunction(cmd, res, timeout))
      {
         case -1:
            LOG_DEBUG(log, "error transmitting command to PN532");
            return -1;

         case -2:
            LOG_DEBUG(log, "RX << TIMEOUT");
            return -2;
      }

      LOG_DEBUG(log, "RX << {x}", {res});

      return 0;
   }

};

PN532::PN532(TransmitFunction transmitFunction) : impl(std::make_shared<Impl>(std::move(transmitFunction)))
{
}

int PN532::diagnose()
{
   impl->log->warn("diagnose command not implemented yet");

   return -1;
}

int PN532::getFirmwareVersion(FwVersion &version)
{
   return impl->getFirmwareVersion(version);
}

int PN532::getGeneralStatus(GeneralStatus &status)
{
   return impl->getGeneralStatus(status);
}

int PN532::readRegister(int reg, int &value)
{
   return impl->readRegister(reg, value);
}

int PN532::writeRegister(int reg, int value)
{
   return impl->writeRegister(reg, value);
}

int PN532::readGPIO()
{
   impl->log->warn("readGPIO command not implemented yet");

   return -1;
}

int PN532::writeGPIO()
{
   impl->log->warn("writeGPIO command not implemented yet");

   return -1;
}

int PN532::setSerialBaudRate()
{
   impl->log->warn("setSerialBaudRate command not implemented yet");

   return -1;
}

int PN532::setParameters(int value)
{
   return impl->setParameters(value);
}

int PN532::setSAMConfiguration(int mode, int timeout, int irq)
{
   return impl->setSAMConfiguration(mode, timeout, irq);
}

int PN532::powerDown(int wakeUpEnable, int triggerIrq)
{
   return impl->powerDown(wakeUpEnable, triggerIrq);
}

int PN532::tgInitAsTarget(int &mode, rt::ByteBuffer &init)
{
   return impl->tgInitAsTarget(mode, init);
}

int PN532::tgResponseToInitiator(rt::ByteBuffer &data, int &status)
{
   return impl->tgResponseToInitiator(data, status);
}

int PN532::tgGetData(rt::ByteBuffer &data, int &status)
{
   return impl->tgGetData(data, status);
}

int PN532::tgSetData(const rt::ByteBuffer &data, int &status)
{
   return impl->tgSetData(data, status);
}

}
