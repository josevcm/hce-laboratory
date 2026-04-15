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
#include "DesfireCreateRecordFile.h"

namespace hce::targets {

DesfireCreateRecordFile::DesfireCreateRecordFile(Instance &bundle, unsigned int type) : Command(bundle), fileType(type)
{
}

int DesfireCreateRecordFile::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, fileType == LinearRecordFile ? "createLinearRecordFile" : "createCyclicRecordFile");

   // command length
   const unsigned int length = request.remaining();

   // check parameters length
   if (length != 10 && length != 12)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application district of master APP must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // check if master key auth is required for create/delete files
   if (!picc.isFreeCreateDelete() && !picc.isAuthenticatedWithMasterKey())
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // check if ISO parameters are present
   if (picc.isApplicationIsoEnabled())
   {
      if (length != 12)
         return DESFIRE_STATUS_LENGTH_ERROR;
   }
   else
   {
      if (length != 10)
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
   unsigned int recordSize = request.getInt(3);
   unsigned int recordLimit = request.getInt(3);

   LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});
   LOG_INFO(log, "\tcommSettings: 0x{02x}", {commSettings});
   LOG_INFO(log, "\taccessRights: 0x{04x}, readKey: 0x{02x}, writeKey: 0x{02x}, readWriteKey: 0x{02x}, changeKey: 0x{02x}}", {accessRights, ((accessRights >> 12) & 0x0f), ((accessRights >> 8) & 0x0f), ((accessRights >> 4) & 0x0f), (accessRights & 0x0f)});
   LOG_INFO(log, "\trecordSize: {} ", {recordSize});
   LOG_INFO(log, "\trecordLimit: {}", {recordLimit});

   if (picc.isApplicationIsoEnabled())
      LOG_INFO(log, "\tisoId: 0x{04x}", {isoId});

   // fileId must be in range 0..31
   if (fileId > DESFIRE_MAX_FILE_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // minimum 1 byte per record
   if (recordSize < 1)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // minimum 1 record for linear or 2 for cyclic
   if (recordLimit < (fileType == LinearRecordFile ? 1 : 2))
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if file already exists
   if (picc.hasFile(fileId))
      return DESFIRE_STATUS_DUPLICATE_ERROR;

   // cyclic files have -1 usable record
   if (fileType == CyclicRecordFile)
      --recordLimit;

   // start with no records
   rt::ByteBuffer data = rt::ByteBuffer::zero(recordSize * recordLimit);
   data.flip();

   const FileEntry file {
      .fileId = fileId,
      .fileType = fileType,
      .fileSize = recordSize * recordLimit,
      .isoId = isoId,
      .commSettings = commSettings,
      .accessRights = accessRights,
      .recordSize = recordSize,
      .recordLimit = recordLimit,
      .data = data,
      .backup = {}
   };

   // add file to application
   picc.addFile(file);

   // send successful response
   return picc.sendAck(response);
}

}
