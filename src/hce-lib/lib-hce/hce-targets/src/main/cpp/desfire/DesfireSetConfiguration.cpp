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

#include "Instance.h"
#include "DesfireSetConfiguration.h"

namespace hce::targets {

DesfireSetConfiguration::DesfireSetConfiguration(Instance &bundle) : Command(bundle)
{
}

int DesfireSetConfiguration::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "setConfiguration");

   // must be at PICC level
   if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // require master key authentication
   if (!picc.isAuthenticatedWithMasterKey())
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // read option byte into CMAC header, then consume it from request
   picc.readHeader(request, 1);

   const unsigned int option = request.getInt(1);

   LOG_INFO(log, "\toption: 0x{02x}", {option});

   switch (option)
   {
      // configuration byte: bit0=formatDisabled, bit1=randomId (write-once)
      case 0x00:
      {
         if (const int status = picc.decodeData(request, 1, CryptCommunication, {DESFIRE_MASTER_KEY_ID}); status != DESFIRE_STATUS_OK)
            return status;

         const unsigned int config = picc.buffer.getInt(1);

         LOG_INFO(log, "\tconfig: 0x{02x}", {config});

         if (config & 0x01)
            picc.formatDisabled = true;

         if (config & 0x02)
            picc.randomId = true;

         break;
      }

      // default key and version: 24 bytes key + 1 byte version
      case 0x01:
      {
         if (const int status = picc.decodeData(request, 25, CryptCommunication, {DESFIRE_MASTER_KEY_ID}); status != DESFIRE_STATUS_OK)
            return status;

         picc.defaultKey = picc.buffer.getBuffer(24);
         picc.defaultKeyVersion = picc.buffer.getInt(1);

         LOG_INFO(log, "\tdefaultKeyVersion: 0x{02x}", {picc.defaultKeyVersion});

         break;
      }

      // user-defined ATS: TL T0 TA TB TC + historical bytes (max 20 bytes)
      case 0x02:
      {
         const unsigned int cipherLen = request.remaining();

         if (const int status = picc.decodeData(request, cipherLen, CryptCommunication, {DESFIRE_MASTER_KEY_ID}); status != DESFIRE_STATUS_OK)
            return status;

         const unsigned int atsLen = picc.buffer.remaining();

         if (atsLen > 20)
            return DESFIRE_STATUS_PARAMETER_ERROR;

         picc.customAts = picc.buffer.getBuffer(atsLen);

         LOG_INFO(log, "\tcustomAts: 0x{x}", {picc.customAts});

         break;
      }

      default:
         return DESFIRE_STATUS_PARAMETER_ERROR;
   }

   picc.dirty = true;
   return picc.sendAck(response);
}

}
