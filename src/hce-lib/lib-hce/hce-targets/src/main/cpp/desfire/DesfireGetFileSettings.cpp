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
#include "DesfireGetFileSettings.h"

namespace hce::targets {

DesfireGetFileSettings::DesfireGetFileSettings(Instance &bundle) : Command(bundle)
{
}

int DesfireGetFileSettings::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getFileSettings");

   // check minimum command length
   if (request.remaining() < 1)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application district of master APP must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, 1);

   // decode parameters
   const unsigned int fileId = request.getInt(1);

   LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});

   // check parameters
   if (fileId > DESFIRE_MAX_FILE_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if file exists
   if (!picc.hasFile(fileId))
      return DESFIRE_STATUS_FILE_NOT_FOUND;

   // get selected file to read
   const FileEntry *file = picc.getFile(fileId);

   response.putInt(file->fileType, 1);
   response.putInt(file->commSettings, 1);
   response.putInt(file->accessRights, 2);

   LOG_INFO(log, "\tfileType: {}", {file->fileType});
   LOG_INFO(log, "\tcommSettings: {2x}", {file->commSettings});
   LOG_INFO(log, "\taccessRights: {4x}", {file->accessRights});

   switch (file->fileType)
   {
      case ValueFile:

         response.putInt(file->lowerLimit, 4);
         response.putInt(file->upperLimit, 4);
         response.putInt(file->creditLimit, 4);
         response.putInt(file->features, 1);

         LOG_INFO(log, "\tlowerLimit: {}", {file->lowerLimit});
         LOG_INFO(log, "\tupperLimit: {}", {file->upperLimit});
         LOG_INFO(log, "\tcreditLimit: {}", {file->creditLimit});
         LOG_INFO(log, "\tfeatures: {}", {file->features});
         break;

      case StandardFile:
      case BackupFile:

         response.putInt(file->fileSize, 3);

         LOG_INFO(log, "\tfileSize: {}", {file->fileSize});
         break;

      case CyclicRecordFile:
      case LinearRecordFile:

         response.putInt(file->recordSize, 3);
         response.putInt(file->recordLimit, 3);
         response.putInt(file->records(), 3);

         LOG_INFO(log, "\trecordSize: {}", {file->recordSize});
         LOG_INFO(log, "\trecordLimit: {}", {file->recordLimit});
         LOG_INFO(log, "\trecordCount: {}", {file->records()});
         break;
   }

   // send successful response
   return picc.sendAck(response);
}

}
