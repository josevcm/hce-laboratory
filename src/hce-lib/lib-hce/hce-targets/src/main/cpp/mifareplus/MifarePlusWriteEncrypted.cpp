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
#include "MifarePlusWriteEncrypted.h"

namespace hce::targets::mifareplus {

MifarePlusWriteEncrypted::MifarePlusWriteEncrypted(Instance &i) : Command(i)
{
}

int MifarePlusWriteEncrypted::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "writeEncrypted");

   // [INS][blockAddr][EK(data)_16][MAC_8] = 26 bytes
   if (request.remaining() != 26)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int blockAddr = request.get();
   const rt::ByteBuffer encData = request.getBuffer(16);
   const rt::ByteBuffer rxMac = request.getBuffer(8);

   LOG_INFO(log, "\tblock: {}", {blockAddr});

   if (blockAddr == 0)
   {
      response.put(MFPLUS_STATUS_ERR_PARAM);
      return -1;
   }

   if (!picc.isValidBlock(blockAddr))
   {
      response.put(MFPLUS_STATUS_ERR_BOUNDARY);
      return -1;
   }

   if (!picc.isAuthenticated(blockAddr))
   {
      response.put(MFPLUS_STATUS_ERR_NOTAUTH);
      return -1;
   }

   // verify MAC over encrypted data: CMAC(sessionMacKey, TI||blockAddr||EK(data))[0..7]
   rt::ByteBuffer macInput(4 + 1 + 16);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(blockAddr))
           .put(encData);
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer expectedMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   if (rxMac != expectedMac)
   {
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   // decrypt with SES_AUTH_ENC key
   picc.aes.init(picc.auth.sessionEncKey, 0);
   rt::ByteBuffer plain = picc.aes.decrypt(encData, picc.auth.iv);

   picc.blocks[blockAddr] = plain.copy();
   picc.dirty = true;

   LOG_INFO(log, "\tblock {} written (encrypted)", {blockAddr});

   // response MAC: CMAC(sessionMacKey, TI||0x90)[0..7]
   rt::ByteBuffer respInput(5);
   respInput.put(picc.auth.ti).put(static_cast<unsigned char>(0x90));
   respInput.flip();

   rt::ByteBuffer respIv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer respMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, respInput, respIv, crypto::CMAC::CmacAES128).slice(0, 8);

   response.put(static_cast<unsigned char>(0x90));
   response.put(respMac);

   return 0;
}

} // namespace hce::targets::mifareplus
