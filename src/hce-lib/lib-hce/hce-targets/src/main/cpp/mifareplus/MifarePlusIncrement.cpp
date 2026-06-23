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
#include "MifarePlusIncrement.h"

namespace hce::targets::mifareplus {

MifarePlusIncrement::MifarePlusIncrement(Instance &i) : Command(i)
{
}

// Request: [0xC0][blockAddr][EK(value_4bytes + 12_zeros)][MAC_8] = 26 bytes
// MAC = CMAC(sessionMacKey, TI||blockAddr||EK(value))[0:8]
// Response: [0x90][respMAC_8]
// respMAC = CMAC(sessionMacKey, TI||0x90)[0:8]
int MifarePlusIncrement::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "increment");

   if (request.remaining() != 26)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int blockAddr = request.get();
   const rt::ByteBuffer encValue = request.getBuffer(16);
   const rt::ByteBuffer rxMac = request.getBuffer(8);

   LOG_INFO(log, "\tblock: {}", {blockAddr});

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

   if (!picc.isValueBlock(blockAddr))
   {
      response.put(MFPLUS_STATUS_ERR_PARAM);
      return -1;
   }

   rt::ByteBuffer macInput(4 + 1 + 16);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(blockAddr))
           .put(encValue);
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer expectedMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   if (rxMac != expectedMac)
   {
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   picc.aes.init(picc.auth.sessionEncKey, 0);
   rt::ByteBuffer plain = picc.aes.decrypt(encValue, picc.auth.iv);

   const unsigned char *p = plain.data();
   const int32_t delta = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
   const int32_t current = picc.getBlockValue(blockAddr);
   const unsigned char addr = picc.blocks[blockAddr].data()[12];

   picc.auth.transferBuffer = picc.blocks[blockAddr].copy();
   picc.auth.transferValid = true;
   picc.auth.transferSector = picc.sectorOf(blockAddr);

   rt::ByteBuffer tmp = rt::ByteBuffer::zero(16);
   unsigned char *td = tmp.data();
   const int32_t result = current + delta;
   td[0] = static_cast<unsigned char>(result & 0xFF);
   td[1] = static_cast<unsigned char>((result >> 8) & 0xFF);
   td[2] = static_cast<unsigned char>((result >> 16) & 0xFF);
   td[3] = static_cast<unsigned char>((result >> 24) & 0xFF);
   td[4] = ~td[0]; td[5] = ~td[1]; td[6] = ~td[2]; td[7] = ~td[3];
   td[8] = td[0];  td[9] = td[1];  td[10] = td[2]; td[11] = td[3];
   td[12] = addr; td[13] = ~addr; td[14] = addr; td[15] = ~addr;
   picc.auth.transferBuffer = tmp.copy();

   LOG_INFO(log, "\tincrement block {} by {} -> {}", {blockAddr, delta, result});

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
