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
#include "MifarePlusDecrementTransfer.h"

namespace hce::targets::mifareplus {

MifarePlusDecrementTransfer::MifarePlusDecrementTransfer(Instance &i) : Command(i)
{
}

// Request: [0x36][srcAddr][dstAddr][EK(value_4bytes + 12_zeros)][MAC_8] = 27 bytes
// MAC = CMAC(sessionMacKey, TI||srcAddr||dstAddr||EK(value))[0:8]
// Response: [0x90][respMAC_8]
// respMAC = CMAC(sessionMacKey, TI||0x90)[0:8]
int MifarePlusDecrementTransfer::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "decrementTransfer");

   if (request.remaining() != 27)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int srcAddr = request.get();
   const unsigned int dstAddr = request.get();
   const rt::ByteBuffer encValue = request.getBuffer(16);
   const rt::ByteBuffer rxMac = request.getBuffer(8);

   LOG_INFO(log, "\tsrc: {} dst: {}", {srcAddr, dstAddr});

   if (!picc.isValidBlock(srcAddr) || !picc.isValidBlock(dstAddr))
   {
      response.put(MFPLUS_STATUS_ERR_BOUNDARY);
      return -1;
   }

   if (!picc.isAuthenticated(srcAddr) || !picc.isAuthenticated(dstAddr))
   {
      response.put(MFPLUS_STATUS_ERR_NOTAUTH);
      return -1;
   }

   if (!picc.isValueBlock(srcAddr))
   {
      response.put(MFPLUS_STATUS_ERR_PARAM);
      return -1;
   }

   rt::ByteBuffer macInput(4 + 1 + 1 + 16);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(srcAddr))
           .put(static_cast<unsigned char>(dstAddr))
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
   const int32_t current = picc.getBlockValue(srcAddr);
   const unsigned char addr = picc.blocks[dstAddr].data()[12];
   const int32_t result = current - delta;

   picc.setBlockValue(dstAddr, result, addr);
   picc.dirty = true;

   LOG_INFO(log, "\tdecrement block {} by {} -> {} written to block {}", {srcAddr, delta, result, dstAddr});

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
