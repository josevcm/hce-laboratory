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
#include "DesfireClearRecordFile.h"

namespace hce::targets::desfire {

DesfireClearRecordFile::DesfireClearRecordFile(Instance &bundle) : Command(bundle)
{
}

int DesfireClearRecordFile::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "clearRecordFile");

   // check minimum command length
   if (request.remaining() < 1)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application distinct of master app must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, 1);

   // decode parameters
   unsigned int fileId = request.getInt(1);

   LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});

   // check parameters
   if (fileId > DESFIRE_MAX_FILE_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if file exists
   if (!picc.hasFile(fileId))
      return DESFIRE_STATUS_FILE_NOT_FOUND;

   // get selected file to read
   FileEntry *file = picc.getFile(fileId);

   // check file type
   if (!file->isRecordFile())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // check current authentication status can clear file
   if (!picc.hasReadWriteAccess(file))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // mark file as cleared by setting backup as empty (but valid)
   file->backup = rt::ByteBuffer::zero(file->recordSize);

   // after flip(): remaining=0, which commit() detects as "clear all records"
   file->backup.flip();

   // mark file as dirty so commitTransaction() processes the clear
   file->changes |= DESFIRE_FILE_CHANGED_DATA;

   // send successful response
   return picc.sendAck(response);
}

}
