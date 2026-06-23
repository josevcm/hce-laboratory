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
#include <hce/crypto/CMAC.h>
#include "Instance.h"
#include "MifarePlusRead.h"

namespace hce::targets::mifareplus {

MifarePlusRead::MifarePlusRead(Instance &i) : Command(i)
{
}

int MifarePlusRead::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "read");

   if (request.remaining() != 3)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int blockAddr = request.get();
   const unsigned int numBlocks = request.get();

   LOG_INFO(log, "\tblock: {} count: {}", {blockAddr, numBlocks});

   if (numBlocks < 1 || numBlocks > 3)
   {
      response.put(MFPLUS_STATUS_ERR_PARAM);
      return -1;
   }

   for (unsigned int i = 0; i < numBlocks; ++i)
   {
      if (!picc.isValidBlock(blockAddr + i))
      {
         response.put(MFPLUS_STATUS_ERR_BOUNDARY);
         return -1;
      }
      if (!picc.isAuthenticated(blockAddr + i))
      {
         response.put(MFPLUS_STATUS_ERR_NOTAUTH);
         return -1;
      }
   }

   rt::ByteBuffer data(numBlocks * 16);
   for (unsigned int i = 0; i < numBlocks; ++i)
      data.put(picc.blocks[blockAddr + i]);
   data.flip();

   rt::ByteBuffer macInput(4 + 1 + 1 + numBlocks * 16);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(blockAddr))
           .put(static_cast<unsigned char>(numBlocks))
           .put(data);
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer mac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   response.put(static_cast<unsigned char>(0x90));
   response.put(data);
   response.put(mac);

   return 0;
}

} // namespace hce::targets::mifareplus
