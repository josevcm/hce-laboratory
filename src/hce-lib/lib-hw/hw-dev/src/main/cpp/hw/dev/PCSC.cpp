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

#include <winscard.h>

#include <stdexcept>

#include <hw/dev/PCSC.h>

#include <rt/Logger.h>

namespace hw {

struct PCSC::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.PCSC");

   SCARDCONTEXT hContext = 0;
   SCARDHANDLE hCard = 0;
   DWORD activeProtocol = 0;

   std::string connectedDevice;

   Impl()
   {
      if (LONG rv = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext); rv != SCARD_S_SUCCESS)
      {
         std::string result = std::to_string(rv);
         log->error("error creating context: {}", {result});
         throw std::runtime_error("error creating context: " + result);
      }
   }

   // Destructor to release the context
   ~Impl()
   {
      SCardReleaseContext(hContext);
   }

   // List available readers
   std::vector<std::string> listReaders() const
   {
      std::vector<std::string> readersList;

      char readers[65536];
      DWORD readersLen = sizeof(readers);

      if (const LONG rv = SCardListReaders(hContext, nullptr, readers, &readersLen); rv == SCARD_S_SUCCESS)
      {
         for (char *reader = readers; *reader != '\0'; reader += strlen(reader) + 1)
         {
            readersList.emplace_back(reader);
         }
      }

      return readersList;
   }

   int connect(const std::string &reader, Mode mode, Protocol protocol)
   {
      int sharedMode = 0;
      int preferredProtocols = 0;

      switch (mode)
      {
         case Direct:
            sharedMode = SCARD_SHARE_DIRECT;
            break;

         case Exclusive:
            sharedMode = SCARD_SHARE_EXCLUSIVE;
            break;

         case Shared:
            sharedMode = SCARD_SHARE_SHARED;
            break;

         default:
            log->error("invalid mode: {}", {mode});
            return -1;
      }

      if (protocol & T0)
         preferredProtocols |= SCARD_PROTOCOL_T0;

      if (protocol & T1)
         preferredProtocols |= SCARD_PROTOCOL_T1;

      if (const LONG rv = SCardConnect(hContext, reader.c_str(), sharedMode, preferredProtocols, &hCard, &activeProtocol); rv != SCARD_S_SUCCESS)
      {
         log->warn("error {} connecting to reader '{}'", {std::to_string(rv), reader});
         return -1;
      }

      connectedDevice = reader;

      return 0;
   }

   int disconnect()
   {
      if (!hCard)
         return -1;

      if (const LONG rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD); rv != SCARD_S_SUCCESS)
         log->warn("error {} disconnecting from reader {}", {std::to_string(rv), connectedDevice});

      hCard = 0;
      connectedDevice = "";

      return 0;
   }

   int cardTransmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &resp)
   {
      if (!hCard)
      {
         log->error("device not connected");
         return -1;
      }

      DWORD received = resp.capacity();

      const SCARD_IO_REQUEST pioSendPci = activeProtocol == SCARD_PROTOCOL_T0 ? *SCARD_PCI_T0 : *SCARD_PCI_T1;

      LOG_DEBUG(log, "TX >> {x}", {cmd});

      if (const LONG rv = SCardTransmit(hCard, &pioSendPci, cmd.data(), cmd.limit(), nullptr, resp.ptr(), &received); rv != SCARD_S_SUCCESS)
      {
         log->warn("error {} transmitting data command to reader '{}'", {std::to_string(rv), connectedDevice});
         return -1;
      }

      resp.push(received);
      resp.flip();

      LOG_DEBUG(log, "RX << {x}", {resp});

      return 0;
   }

   int cardControl(int controlCode, const rt::ByteBuffer &cmd, rt::ByteBuffer &resp)
   {
      if (!hCard)
      {
         log->error("device not connected");
         return -1;
      }

      DWORD received;

      LOG_DEBUG(log, "CONTROL {x} >> {x}", {controlCode, cmd});

      if (const LONG rv = SCardControl(hCard, controlCode, cmd.data(), cmd.remaining(), resp.ptr(), resp.capacity(), &received); rv != SCARD_S_SUCCESS)
      {
         log->warn("error {} transmitting control command to reader '{}'", {std::to_string(rv), connectedDevice});
         return -1;
      }

      resp.push(received);
      resp.flip();

      LOG_DEBUG(log, "CONTROL {x} << {x}", {controlCode, resp});

      return 0;
   }
};

PCSC::PCSC() : impl(std::make_shared<Impl>())
{
}

std::vector<std::string> PCSC::listReaders() const
{
   return impl->listReaders();
}

int PCSC::connect(const std::string &reader, Mode mode, Protocol protocol)
{
   return impl->connect(reader, mode, protocol);
}

int PCSC::disconnect()
{
   return impl->disconnect();
}

int PCSC::transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &resp)
{
   return impl->cardTransmit(cmd, resp);
}

int PCSC::control(int controlCode, const rt::ByteBuffer &cmd, rt::ByteBuffer &resp)
{
   return impl->cardControl(controlCode, cmd, resp);
}

}
