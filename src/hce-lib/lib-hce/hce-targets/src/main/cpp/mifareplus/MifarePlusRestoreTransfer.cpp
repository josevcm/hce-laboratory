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
#include "MifarePlusRestoreTransfer.h"

namespace hce::targets::mifareplus {

MifarePlusRestoreTransfer::MifarePlusRestoreTransfer(Instance &i) : Command(i)
{
}

// Request: [0x37][srcAddr][dstAddr][MAC_8] = 11 bytes
// MAC = CMAC(sessionMacKey, TI||srcAddr||dstAddr)[0:8]
// Response: [0x90][respMAC_8]
// respMAC = CMAC(sessionMacKey, TI||0x90)[0:8]
int MifarePlusRestoreTransfer::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "restoreTransfer");

   if (request.remaining() != 11)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int srcAddr = request.get();
   const unsigned int dstAddr = request.get();
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

   rt::ByteBuffer macInput(4 + 1 + 1);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(srcAddr))
           .put(static_cast<unsigned char>(dstAddr));
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer expectedMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   if (rxMac != expectedMac)
   {
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   picc.blocks[dstAddr] = picc.blocks[srcAddr].copy();
   picc.dirty = true;

   LOG_INFO(log, "\trestore block {} copied to block {}", {srcAddr, dstAddr});

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
