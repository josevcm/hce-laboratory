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
#include "DesfireIsoReadBinary.h"

namespace hce::targets::desfire {

DesfireIsoReadBinary::DesfireIsoReadBinary(Instance &bundle) : Command(bundle)
{
}

int DesfireIsoReadBinary::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoReadBinary");

   // skip CLA and INS
   request.skip(2);

   // read parameters
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int le = request.get();

   // invalid APDU format, must be CLA | INS | P1 | P2 | LE
   if (request.remaining() > 0)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   const FileEntry *file;
   unsigned int offset;

   if (p1 & 0x80)
   {
      // P1 bit7=1: P1[4:0] = Short File ID, P2 = offset (implicit, non-persistent selection)
      const unsigned int sfi = p1 & 0x1F;
      offset = p2;

      file = picc.getFileByShortFID(sfi);

      if (file == nullptr)
         return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

      LOG_INFO(log, "\tsfi: 0x{02x}", {sfi});
   }
   else
   {
      // P1 bit7=0: P1:P2 = 15-bit offset within current selected EF
      if (picc.elementaryFile == nullptr)
         return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

      offset = p1 << 8 | p2;
      file = picc.getFile(picc.elementaryFile->fileId);

      LOG_INFO(log, "\tisoId: 0x{02x}", {picc.elementaryFile->isoId});
      LOG_INFO(log, "\tfileId: 0x{02x}", {picc.elementaryFile->fileId});
   }

   LOG_INFO(log, "\toffset: {}", {offset});
   LOG_INFO(log, "\tlength: {}", {le});

   // check file type
   if (!file->isDataFile())
      return DESFIRE_ISO_STATUS_ACCESS_NOT_ALLOWED;

   // check file limits
   if (offset >= file->length() || (offset + le) > file->length())
      return DESFIRE_ISO_STATUS_NOT_ENOUGH_DATA;

   // get data to send
   rt::ByteBuffer data = file->data.slice(offset, le == 0 ? file->data.remaining() - offset : le);

   // prepare transmit buffer
   if (const int status = picc.encodeData(data, le, file->commSettings, {file->readKey(), file->readWriteKey()}); status != DESFIRE_STATUS_OK)
      return status;

   // add response to output buffer
   picc.sendData(response);

   return DESFIRE_ISO_STATUS_OK;
}

}
