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
#include "DesfireAuthenticateAES.h"

namespace hce::targets {

DesfireAuthenticateAES::DesfireAuthenticateAES(Instance &bundle) : Command(bundle)
{
   auth.cipher = &bundle.aes;
}

int DesfireAuthenticateAES::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "authenticateAES [{}]", {picc.chaining});

   // step 1, initialize cipher and generate random challenge
   if (picc.chaining == 0)
   {
      if (request.remaining() != 1)
         return DESFIRE_STATUS_LENGTH_ERROR;

      // AES auth only available in AES crypto mode
      if (picc.isApplicationSelected() && !picc.isApplicationCryptoMode(KeyTypeAES))
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // get key number
      unsigned int keyNo = request.getInt(1);

      LOG_INFO(log, "\tkeyNo: 0x{02x}", {keyNo});

      // check if key entry exists
      if (!picc.hasKeyEntry(keyNo))
         return DESFIRE_STATUS_NO_SUCH_KEY;

      // get key entry
      auth.keyEntry = picc.getKeyEntry(keyNo);

      // check if key type is 3DES
      if (!auth.keyEntry->isAES())
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // invalidate current authentication state
      picc.invalidateAuth();

      // initialize IV
      auth.sessionIv = rt::ByteBuffer::zero(16);

      // initialize cipher with key
      auth.cipher->init(auth.keyEntry->key, 0);

      // generate RndB
      random = rt::ByteBuffer::random(16);

      // encrypt rndB and add to response ek(RndB)
      response.put(auth.cipher->encrypt(this->random, auth.sessionIv));

      return DESFIRE_STATUS_ADDITIONAL_FRAME;
   }

   rt::ByteBuffer sessionKey(16);

   // response must be 32 bytes long ek(RndA || RndB')
   if (request.remaining() != 32)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // read ek(RndA || RndB') from request and decrypt to get RndA || RndB'
   const rt::ByteBuffer dkRndARndBr = auth.cipher->decrypt(request, auth.sessionIv);

   // split RndA / RndB' and rotate
   const rt::ByteBuffer rndA = dkRndARndBr.slice(0, 16);
   const rt::ByteBuffer rndBr = dkRndARndBr.slice(16, 16);

   // rotate RndB' to get RndB and RndA to get RndA'
   const rt::ByteBuffer rndB = rt::ByteBuffer::rotateBytes(rndBr, rt::ByteBuffer::Right);
   const rt::ByteBuffer rndAr = rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left);

   // check if RndB matches
   if (rndB != random)
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // encrypt rndAr and add to response ek(RndAr)
   response.put(auth.cipher->encrypt(rndAr, auth.sessionIv));

   // at this point, authentication has successfully, build session key
   sessionKey.put(rndA.slice(0, 4));
   sessionKey.put(rndB.slice(0, 4));
   sessionKey.put(rndA.slice(12, 4));
   sessionKey.put(rndB.slice(12, 4));

   sessionKey.flip();

   LOG_INFO(log, "\tsessionKey: 0x{x}", {sessionKey.copy()});

   // initialize cipher for session
   auth.mode = AESAuthentication;
   auth.cipher->init(sessionKey, 0);
   auth.sessionIv = rt::ByteBuffer::zero(16);
   auth.sessionKey = sessionKey;

   // initialize auth status
   picc.setAuthentication(auth);

   return DESFIRE_STATUS_OK;
}

}
