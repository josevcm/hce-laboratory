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
#include "DesfireWriteData.h"

namespace hce::targets {

DesfireWriteData::DesfireWriteData(Instance &bundle) : Command(bundle)
{
}

int DesfireWriteData::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "writeData [{}]", {picc.chaining});

   if (picc.chaining == 0)
   {
      // check minimum command length
      if (request.remaining() < 7)
         return DESFIRE_STATUS_LENGTH_ERROR;

      // application district of master APP must be selected
      if (!picc.isApplicationSelected())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // copy header for further CMAC processing and update IV
      picc.readHeader(request, 7);

      // decode parameters
      fileId = request.getInt(1);
      offset = request.getInt(3);
      length = request.getInt(3);

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
      file = picc.getFile(fileId);

      // check file type
      if (!file->isDataFile())
         return DESFIRE_STATUS_PERMISSION_DENIED;
   }

   // receive file data
   if (const int status = picc.decodeData(request, length, file->commSettings, {file->writeKey(), file->readWriteKey()}); status != DESFIRE_STATUS_OK)
      return status;

   // now update data file...
   LOG_INFO(log, "\tdata[{}]: {x}", {picc.buffer.size(), picc.buffer});

   // write data to file
   file->write(picc.buffer, offset, length);

   // send successful response
   return picc.sendAck(response);
}

}
