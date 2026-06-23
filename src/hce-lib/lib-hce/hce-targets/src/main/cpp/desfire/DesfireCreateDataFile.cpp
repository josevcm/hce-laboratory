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
#include "DesfireCreateDataFile.h"

namespace hce::targets::desfire {

DesfireCreateDataFile::DesfireCreateDataFile(Instance &bundle, unsigned int type) : Command(bundle), fileType(type)
{
}

int DesfireCreateDataFile::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, fileType == StandardFile ? "createStandardFile" : "createBackupFile");

   // command length
   const unsigned int length = request.remaining();

   // check minimum command length
   if (length != 7 && length != 9)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application district of master APP must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // check if master key auth is required for create/delete files
   if (!picc.isFreeCreateDelete() && !picc.isAuthenticatedWithMasterKey())
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   if (picc.isApplicationIsoEnabled())
   {
      if (length != 9)
         return DESFIRE_STATUS_LENGTH_ERROR;
   }
   else
   {
      if (length != 7)
         return DESFIRE_STATUS_LENGTH_ERROR;
   }

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, length);

   // decode parameters
   unsigned int isoId = 0;
   unsigned int fileId = request.getInt(1);

   if (picc.isApplicationIsoEnabled())
      isoId = request.getInt(2);

   unsigned int commSettings = request.getInt(1);
   unsigned int accessRights = request.getInt(2);
   unsigned int fileSize = request.getInt(3);

   LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});
   LOG_INFO(log, "\tfileSize: {}", {fileSize});
   LOG_INFO(log, "\tcommSettings: 0x{02x}", {commSettings});
   LOG_INFO(log, "\taccessRights: 0x{04x}, readKey: 0x{02x}, writeKey: 0x{02x}, readWriteKey: 0x{02x}, changeKey: 0x{02x}", {accessRights, ((accessRights >> 12) & 0x0f), ((accessRights >> 8) & 0x0f), ((accessRights >> 4) & 0x0f), (accessRights & 0x0f)});

   if (picc.isApplicationIsoEnabled())
      LOG_INFO(log, "\tisoId: 0x{04x}", {isoId});

   // fileId must be in range 0..31
   if (fileId > DESFIRE_MAX_FILE_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // fileSize must be greater than 0
   if (fileSize == 0)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if file already exists
   if (picc.hasFile(fileId))
      return DESFIRE_STATUS_DUPLICATE_ERROR;

   const FileEntry file {
      .fileId = fileId,
      .fileType = fileType,
      .fileSize = fileSize,
      .isoId = isoId,
      .commSettings = commSettings,
      .accessRights = accessRights,
      .data = rt::ByteBuffer::zero(fileSize),
      .backup = rt::ByteBuffer::zero(fileSize)
   };

   // add file to application
   picc.addFile(file);

   // send successful response
   return picc.sendAck(response);
}

}
