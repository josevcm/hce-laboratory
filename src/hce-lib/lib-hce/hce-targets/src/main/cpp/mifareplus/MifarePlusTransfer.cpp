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
#include "MifarePlusTransfer.h"

namespace hce::targets::mifareplus {

MifarePlusTransfer::MifarePlusTransfer(Instance &i) : Command(i)
{
}

// Request: [0xB0][blockAddr][MAC_8] = 10 bytes
// MAC = CMAC(sessionMacKey, TI||blockAddr)[0:8]
// Response: [0x90][respMAC_8]
// respMAC = CMAC(sessionMacKey, TI||0x90)[0:8]
int MifarePlusTransfer::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "transfer");

   if (request.remaining() != 10)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const unsigned int blockAddr = request.get();
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

   if (!picc.auth.transferValid)
   {
      response.put(MFPLUS_STATUS_ERR_CMD);
      return -1;
   }

   rt::ByteBuffer macInput(4 + 1);
   macInput.put(picc.auth.ti)
           .put(static_cast<unsigned char>(blockAddr));
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer expectedMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   if (rxMac != expectedMac)
   {
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   picc.blocks[blockAddr] = picc.auth.transferBuffer.copy();
   picc.auth.transferValid = false;
   picc.dirty = true;

   LOG_INFO(log, "\ttransfer register written to block {}", {blockAddr});

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
