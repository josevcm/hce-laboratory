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
#include "DesfireIsoUpdateBinary.h"

namespace hce::targets {

DesfireIsoUpdateBinary::DesfireIsoUpdateBinary(Instance &bundle) : Command(bundle)
{
}

int DesfireIsoUpdateBinary::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoUpdateBinary");

   // skip CLA and INS
   request.skip(2);

   // read parameters
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int lc = request.get();

   // invalid APDU format, must be CLA | INS | P1 | P2 | LC | DATA
   if (request.remaining() != lc)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   // if bit 7 = 1, access by short FID not implemented
   if (p1 & 0x80)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   // if P1 bit 7 = 0, remain 15 bits of P1/P2 are file offset
   const unsigned int offset = p1 << 8 | p2;

   LOG_INFO(log, "\tisoId: 0x{02x}", {picc.elementaryFile->isoId});
   LOG_INFO(log, "\tfileId: 0x{02x}", {picc.elementaryFile->fileId});
   LOG_INFO(log, "\toffset: {}", {offset});
   LOG_INFO(log, "\tlength: {}", {lc});
   LOG_INFO(log, "\tdata: {x}", {request});

   // elementary file must be selected before...
   if (picc.elementaryFile == nullptr)
      return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

   // get file
   FileEntry *file = picc.getFile(picc.elementaryFile->fileId);

   // check file type
   if (!file->isDataFile())
      return DESFIRE_ISO_STATUS_ACCESS_NOT_ALLOWED;

   // check file limits
   if (offset >= file->length() || (offset + lc) > file->length())
      return DESFIRE_ISO_STATUS_NOT_ENOUGH_DATA;

   // write data to file
   file->write(request, offset, lc);

   // ISO UPDATE perform automatic commit
   picc.commitData();

   return DESFIRE_ISO_STATUS_OK;
}

}
