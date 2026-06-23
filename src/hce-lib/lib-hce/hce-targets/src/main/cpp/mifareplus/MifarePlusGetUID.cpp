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
#include "MifarePlusGetUID.h"

namespace hce::targets::mifareplus {

MifarePlusGetUID::MifarePlusGetUID(Instance &i) : Command(i)
{
}

// Request: [0x56][MAC_8] = 9 bytes
// MAC = CMAC(sessionMacKey, TI||0x56)[0:8]
// Response: [0x90][ENC(UID||pad)_16][respMAC_8]
// respMAC = CMAC(sessionMacKey, TI||0x90||ENC(UID||pad))[0:8]
int MifarePlusGetUID::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getUID");

   if (request.remaining() != 9)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   request.skip(1);
   const rt::ByteBuffer rxMac = request.getBuffer(8);

   if (!picc.auth.valid)
   {
      response.put(MFPLUS_STATUS_ERR_NOTAUTH);
      return -1;
   }

   rt::ByteBuffer macInput(5);
   macInput.put(picc.auth.ti).put(static_cast<unsigned char>(MFPLUS_CMD_GET_UID));
   macInput.flip();

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer expectedMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, macInput, iv, crypto::CMAC::CmacAES128).slice(0, 8);

   if (rxMac != expectedMac)
   {
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   rt::ByteBuffer plain = rt::ByteBuffer::zero(16);
   const unsigned int uidLen = picc.uid.size();
   for (unsigned int i = 0; i < uidLen && i < 16; ++i)
      plain.data()[i] = picc.uid.data()[i];

   picc.aes.init(picc.auth.sessionEncKey, 0);
   rt::ByteBuffer encIv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer encUID = picc.aes.encrypt(plain, encIv);

   rt::ByteBuffer respInput(4 + 1 + 16);
   respInput.put(picc.auth.ti)
            .put(static_cast<unsigned char>(0x90))
            .put(encUID);
   respInput.flip();

   rt::ByteBuffer respIv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer respMac = crypto::CMAC::cmac(picc.auth.sessionMacKey, respInput, respIv, crypto::CMAC::CmacAES128).slice(0, 8);

   response.put(static_cast<unsigned char>(0x90));
   response.put(encUID);
   response.put(respMac);

   return 0;
}

} // namespace hce::targets::mifareplus
