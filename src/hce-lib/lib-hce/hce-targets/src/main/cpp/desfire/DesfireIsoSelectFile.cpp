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
#include "DesfireIsoSelectFile.h"

namespace hce::targets {

DesfireIsoSelectFile::DesfireIsoSelectFile(Instance &bundle) : Command(bundle)
{
}

int DesfireIsoSelectFile::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoSelectFile");

   // skip CLA and INS
   request.skip(2);

   // read parameters
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int lc = request.get();

   if (p1 != 0x00 && p1 != 0x02 && p1 != 0x04)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   if (p2 != 0x00 && p2 != 0x0c)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   if (request.remaining() < lc)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   // check parameters
   if (p1 == 0x00 || p1 == 0x02)
   {
      const unsigned int id = lc == 2 ? request.getInt(2, rt::ByteBuffer::BigEndian) : DESFIRE_ISO_MASTERFILE_ID;

      LOG_INFO(log, "\tfileId: 0x{04x}", {id});

      // P1=0x00, select MF, DF or EF
      if (p1 == 0x00)
      {
         if (!picc.isoHasFile(id))
            return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

         picc.isoSelectFile(id);

         return DESFIRE_ISO_STATUS_OK;
      }

      // P1=0x02, select elementary file
      if (!picc.isoHasElementaryFile(id))
         return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

      picc.isoSelectElementaryFile(id);

      return DESFIRE_ISO_STATUS_OK;
   }

   // select by dfname
   rt::ByteBuffer name = request.getBuffer(lc);

   LOG_INFO(log, "\tname: 0x{x}", {name});

   if (!picc.isoHasDirectoryFile(name))
      return DESFIRE_ISO_STATUS_FILE_NOT_FOUND;

   picc.isoSelectDirectoryFile(name);

   return DESFIRE_ISO_STATUS_OK;
}

}
