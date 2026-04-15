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

#include <hce/crypto/CipherDES.h>

#include "Instance.h"
#include "DesfireAuthenticate.h"

namespace hce::targets {

DesfireAuthenticate::DesfireAuthenticate(Instance &bundle) : Command(bundle)
{
   auth.cipher = &bundle.des;
}

int DesfireAuthenticate::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "authenticate [{}]", {picc.chaining});

   // step 1, initialize cipher and generate random challenge
   if (picc.chaining == 0)
   {
      if (request.remaining() != 1)
         return DESFIRE_STATUS_LENGTH_ERROR;

      // legacy auth only available in 2K3DES mode
      if (picc.isApplicationSelected() && !picc.isApplicationCryptoMode(KeyType2K3DES))
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
      if (!auth.keyEntry->is3DES())
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // invalidate current authentication state
      picc.invalidateAuth();

      // initialize cipher with key
      auth.cipher->init(auth.keyEntry->key, CipherDES::Legacy);

      // initialize rndB with random data
      random = rt::ByteBuffer::random(8);

      // encrypt rndB and add to response ek(RndB)
      response.put(auth.cipher->encrypt(this->random));

      return DESFIRE_STATUS_ADDITIONAL_FRAME;
   }

   // at this point authentication is invalidated!
   picc.invalidateAuth();

   // response must be 16 bytes long ek(RndA || RndB')
   if (request.remaining() != 16)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // read ek(RndA || RndB') from request and decrypt to get RndA || RndB'
   const rt::ByteBuffer dkRndARndBr = auth.cipher->decrypt(request);

   // split RndA / RndB' and rotate
   const rt::ByteBuffer rndA = dkRndARndBr.slice(0, 8);
   const rt::ByteBuffer rndBr = dkRndARndBr.slice(8, 8);

   // rotate RndB' to get RndB and RndA to get RndA'
   const rt::ByteBuffer rndB = rt::ByteBuffer::rotateBytes(rndBr, rt::ByteBuffer::Right);
   const rt::ByteBuffer rndAr = rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left);

   // check if RndB matches
   if (rndB != random)
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // encrypt rndAr and add to response ek(RndAr)
   response.put(auth.cipher->encrypt(rndAr));

   // at this point, authentication has successfully, build session key
   rt::ByteBuffer sessionKey(16);

   sessionKey.put(rndA.slice(0, 4));
   sessionKey.put(rndB.slice(0, 4));

   switch (auth.keyEntry->key.size())
   {
      // single DES, 64 bit session key
      case 8:
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
         break;

      // 2 key DES, 128 bit session key
      case 16:

         // if both halves of the key are the same, use singled DES session key
         if (auth.keyEntry->key.slice(0, 8) == auth.keyEntry->key.slice(8, 8))
         {
            sessionKey.put(rndA.slice(0, 4));
            sessionKey.put(rndB.slice(0, 4));
         }
         else
         {
            sessionKey.put(rndA.slice(4, 4));
            sessionKey.put(rndB.slice(4, 4));
         }

         break;

      // 3 key DES, 192 bit session key
      case 24:
         sessionKey.put(rndA.slice(6, 4));
         sessionKey.put(rndB.slice(6, 4));
         sessionKey.put(rndA.slice(12, 4));
         sessionKey.put(rndB.slice(12, 4));
         break;

      default:
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }

   sessionKey.flip();

   LOG_INFO(log, "\tsessionKey: 0x{x}", {sessionKey.copy()});

   // initialize cipher for session
   auth.mode = LegacyAuthentication;
   auth.cipher->init(sessionKey, CipherDES::Legacy);
   auth.sessionIv = rt::ByteBuffer::zero(0);
   auth.sessionKey = sessionKey;

   // initialize auth status
   picc.setAuthentication(auth);

   return DESFIRE_STATUS_OK;
}

}
