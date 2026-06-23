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
#include "DesfireChangeFileSettings.h"

namespace hce::targets::desfire {

DesfireChangeFileSettings::DesfireChangeFileSettings(Instance &bundle) : Command(bundle)
{
}

int DesfireChangeFileSettings::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "changeFileSettings");

   // check minimum command length
   if (request.remaining() < 1)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application distinct of master app must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // copy header for further CMAC processing and update IV
   picc.readHeader(request, 1);

   // read file to change
   unsigned int fileId = request.getInt(1);

   LOG_INFO(log, "\tfileId: {}", {fileId});

   // check if file exists
   if (!picc.hasFile(fileId))
      return DESFIRE_STATUS_FILE_NOT_FOUND;

   // get selected file to read
   FileEntry *file = picc.getFile(fileId);

   // receive command data
   if (const int status = picc.decodeData(request, 3, CryptCommunication, {file->changeRightsKey()}); status != DESFIRE_STATUS_OK)
      return status;

   // read new settings
   unsigned int commSettings = picc.buffer.getInt(1);
   unsigned int accessRights = picc.buffer.getInt(2);

   LOG_INFO(log, "\tcommSettings: 0x{02x}", {commSettings});
   LOG_INFO(log, "\taccessRights: 0x{04x}, readKey: 0x{02x}, writeKey: 0x{02x}, readWriteKey: 0x{02x}, changeKey: 0x{02x}", {accessRights, ((accessRights >> 12) & 0x0f), ((accessRights >> 8) & 0x0f), ((accessRights >> 4) & 0x0f), (accessRights & 0x0f)});

   file->commSettings = commSettings;
   file->accessRights = accessRights;

   // set picc as dirty state
   picc.dirty = true;

   // send successful response
   return picc.sendAck(response);
}

}
