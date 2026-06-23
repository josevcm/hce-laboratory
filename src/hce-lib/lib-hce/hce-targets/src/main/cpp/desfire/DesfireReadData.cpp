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
#include "DesfireReadData.h"

namespace hce::targets::desfire {

DesfireReadData::DesfireReadData(Instance &bundle) : Command(bundle)
{
}

int DesfireReadData::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "readData [{}]", {picc.chaining});

   if (picc.chaining == 0)
   {
      // check minimum command length
      if (request.remaining() != 7)
         return DESFIRE_STATUS_LENGTH_ERROR;

      // application district of master APP must be selected
      if (!picc.isApplicationSelected())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // copy header for further CMAC processing and update IV
      picc.updateIv(request, 7);

      // decode parameters
      unsigned int fileId = request.getInt(1);
      unsigned int offset = request.getInt(3);
      unsigned int length = request.getInt(3);

      LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});
      LOG_INFO(log, "\toffset: {}", {offset});
      LOG_INFO(log, "\tlength: {}", {length});

      // check parameters
      if (fileId > DESFIRE_MAX_FILE_ID)
         return DESFIRE_STATUS_PARAMETER_ERROR;

      // check if file exists
      if (!picc.hasFile(fileId))
         return DESFIRE_STATUS_FILE_NOT_FOUND;

      // get selected file to read
      const FileEntry *file = picc.getFile(fileId);

      // check file type
      if (!file->isDataFile())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // check file limits
      if (offset >= file->length() || (offset + length) > file->length())
         return DESFIRE_STATUS_BOUNDARY_ERROR;

      // get data to send
      rt::ByteBuffer data = file->data.slice(offset, length == 0 ? file->data.remaining() - offset : length);

      // prepare transmit buffer
      if (const int status = picc.encodeData(data, length, file->commSettings, {file->readKey(), file->readWriteKey()}); status != DESFIRE_STATUS_OK)
         return status;
   }

   // send next data block from transmit buffer
   return picc.sendData(response);
}

}
