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

#include <hce/crc/CRC.h>

#include "Instance.h"
#include "DesfireChangeKeySettings.h"

namespace hce::targets::desfire {

DesfireChangeKeySettings::DesfireChangeKeySettings(Instance &bundle) : Command(bundle)
{
}

int DesfireChangeKeySettings::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "changeKeySettings");

   // check configuration changeable flag
   if (!picc.isAllowChangeConfig())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // read 0 bytes header required for later decodeData (this command has no plain parameters)
   picc.readHeader(request, 0);

   // receive command data
   if (const int status = picc.decodeData(request, 1, CryptCommunication, {DESFIRE_MASTER_KEY_ID}); status != DESFIRE_STATUS_OK)
      return status;

   // get new keySettings
   unsigned int keySettings = picc.buffer.getInt(1);

   LOG_INFO(log, "\tkeySettings: 0x{02x}", {keySettings});

   // update key settings
   picc.setKeySettings(keySettings);

   // send successful response
   return picc.sendAck(response);
}

}
