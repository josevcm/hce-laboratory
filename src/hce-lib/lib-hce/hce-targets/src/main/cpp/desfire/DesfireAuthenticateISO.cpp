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
#include "DesfireAuthenticateISO.h"

namespace hce::targets::desfire {

using namespace crc;
using namespace crypto;

DesfireAuthenticateISO::DesfireAuthenticateISO(Instance &bundle) : Command(bundle)
{
   auth.cipher = &bundle.des;
}

int DesfireAuthenticateISO::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "authenticateISO [{}]", {picc.chaining});

   // step 1, initialize cipher and generate random challenge
   if (picc.chaining == 0)
   {
      if (request.remaining() != 1)
         return DESFIRE_STATUS_LENGTH_ERROR;

      // ISO auth available in LEGACY / 2K3DES / 3K3DES mode
      if (picc.isApplicationSelected() && !picc.isApplicationCryptoMode(KeyType2K3DES) && !picc.isApplicationCryptoMode(KeyType3K3DES))
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

      // initialize IV
      auth.sessionIv = rt::ByteBuffer::zero(8);

      // initialize cipher with key
      auth.cipher->init(auth.keyEntry->key, CipherDES::Iso);

      // generate random
      random = rt::ByteBuffer::random(auth.keyEntry->key.size() == 24 ? 16 : 8);

      // encrypt rndB and add to response ek(RndB)
      response.put(auth.cipher->encrypt(this->random, auth.sessionIv));

      return DESFIRE_STATUS_ADDITIONAL_FRAME;
   }

   rt::ByteBuffer sessionKey;

   switch (auth.keyEntry->key.size())
   {
      case 8:
      case 16:
      {
         // response must be 16 bytes long ek(RndA || RndB')
         if (request.remaining() != 16)
            return DESFIRE_STATUS_LENGTH_ERROR;

         // read ek(RndA || RndB') from request and decrypt to get RndA || RndB'
         const rt::ByteBuffer dkRndARndBr = auth.cipher->decrypt(request, auth.sessionIv);

         // split RndA / RndB' and rotate
         const rt::ByteBuffer rndA = dkRndARndBr.slice(0, 8);
         const rt::ByteBuffer rndBr = dkRndARndBr.slice(8, 8);

         // rotate RndB' to get RndB and RndA to get RndA'
         const rt::ByteBuffer rndB = rt::ByteBuffer::rotateBytes(rndBr, rt::ByteBuffer::Right);
         const rt::ByteBuffer rndAr = rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left);

         // check if RndB matches
         if (rndB != random)
            return DESFIRE_STATUS_AUTHENTICATION_ERROR;

         // at this point, authentication has successfully, build session key
         sessionKey = rt::ByteBuffer::zero(16);
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));

         // for 2K3DES, use second half of the key
         if (auth.keyEntry->key.size() == 16)
         {
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
         }
         else
         {
            sessionKey.put(rndA.slice(0, 4));
            sessionKey.put(rndB.slice(0, 4));
         }

         // encrypt rndAr and add to response ek(RndAr)
         response.put(auth.cipher->encrypt(rndAr, auth.sessionIv));

         break;
      }

      case 24:
      {
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

         // at this point, authentication has successfully, build session key
         sessionKey = rt::ByteBuffer::zero(24);
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
         sessionKey.put(rndA.slice(6, 4));
         sessionKey.put(rndB.slice(6, 4));
         sessionKey.put(rndA.slice(12, 4));
         sessionKey.put(rndB.slice(12, 4));

         // encrypt rndAr and add to response ek(RndAr)
         response.put(auth.cipher->encrypt(rndAr, auth.sessionIv));

         break;
      }

      default:
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }

   sessionKey.flip();

   LOG_INFO(log, "\tsessionKey: 0x{x}", {sessionKey.copy()});

   // initialize cipher for session
   auth.mode = ISOAuthentication;
   auth.cipher->init(sessionKey, CipherDES::Iso);
   auth.sessionIv = rt::ByteBuffer::zero(8);
   auth.sessionKey = sessionKey;

   // initialize auth status
   picc.setAuthentication(auth);

   return DESFIRE_STATUS_OK;
}

}
