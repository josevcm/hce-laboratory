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
#include "DesfireIsoAuthentication.h"

namespace hce::targets {

enum Status
{
   GET_CHALLENGE = 0,
   EXTERNAL_AUTH = 1,
   INTERNAL_AUTH = 2,
   AUTHENTICATED = 3
};

DesfireIsoAuthentication::DesfireIsoAuthentication(Instance &bundle) : Command(bundle)
{
}

int DesfireIsoAuthentication::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   // read CLA and INS
   const unsigned int cla = request.get();
   const unsigned int ins = request.get();

   // check command
   switch (ins)
   {
      case DESFIRE_CMD_ISO_GET_CHALLENGE:
         return getChallenge(request, response);

      case DESFIRE_CMD_ISO_EXTERNAL_AUTHENTICATE:
         return externalAuth(request, response);

      case DESFIRE_CMD_ISO_INTERNAL_AUTHENTICATE:
         return internalAuth(request, response);
   }

   return DESFIRE_ISO_STATUS_INS_NOT_SUPPORTED;
}

int DesfireIsoAuthentication::getChallenge(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoGetChallenge");

   // read P1 | P2 | LE
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int le = request.get();

   if (p1 != 0 || p2 != 0)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   if (le != 8 && le != 16)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_LE;

   // generate randon challenge
   rpicc1 = rt::ByteBuffer::random(le);

   // add to response
   response.put(rpicc1);

   // set status 1, challenge generated!
   status = EXTERNAL_AUTH;

   return DESFIRE_ISO_STATUS_OK;
}

int DesfireIsoAuthentication::externalAuth(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoExternalAuth");

   // read P1 | P2 | LC
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int lc = request.get();

   if (status != EXTERNAL_AUTH)
      return DESFIRE_ISO_STATUS_NO_DIAGNOSTIC;

   if (request.remaining() != lc)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   // response must be ek(RPCD1 || RPICC1) bytes long
   if (request.remaining() != rpicc1.size() * 2)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   LOG_INFO(log, "\tp1(algorithm): 0x{02x}", {p1});
   LOG_INFO(log, "\tp2(secret): 0x{02x}", {p2});

   // get key number
   const int keyNo = p2 & 0x1f;

   // only 0x00 to 0x0D keys are valid for authentication
   if (keyNo > 0x0d)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   // PICC access requested
   if (!(p2 & 0x80))
   {
      // PICC masper app must be selected
      if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // for PICC access, only key 0 is valid
      if (keyNo != 0)
         return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;
   }

   // check if key entry exists
   if (!picc.hasKeyEntry(keyNo))
      return DESFIRE_STATUS_NO_SUCH_KEY;

   // get key entry
   auth.keyEntry = picc.getKeyEntry(keyNo);

   // initialize cipher with key algorithm (ignore P01)
   if (auth.keyEntry->is3DES())
      auth.cipher = &picc.des;
   else
      auth.cipher = &picc.aes;

   // initialize cipher with key
   auth.cipher->init(auth.keyEntry->key, 0);

   // initialize IV
   auth.sessionIv = rt::ByteBuffer::zero(auth.keyEntry->is3DES() ? 8 : 16);

   // read ek(RPCD1 || RPICC1) from request and decrypt to get RndA || RndB'
   const rt::ByteBuffer dkRpcd1Rpicc1 = auth.cipher->decrypt(request, auth.sessionIv);

   // split rpcd1 / rpicc1
   const rt::ByteBuffer r1 = dkRpcd1Rpicc1.slice(0, rpicc1.size());
   const rt::ByteBuffer r2 = dkRpcd1Rpicc1.slice(8, rpicc1.size());

   // check if rpicc1 matches
   if (r2 != rpicc1)
      return DESFIRE_ISO_STATUS_ACCESS_NOT_ALLOWED;

   // store status
   algorithm = p1;
   secret = p2;
   rpcd1 = r1;
   status = INTERNAL_AUTH;

   return DESFIRE_ISO_STATUS_OK;
}

int DesfireIsoAuthentication::internalAuth(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "isoInternalAuth");

   // read P1 | P2 | LC
   const unsigned int p1 = request.get();
   const unsigned int p2 = request.get();
   const unsigned int lc = request.get();

   if (status != INTERNAL_AUTH)
      return DESFIRE_ISO_STATUS_NO_DIAGNOSTIC;

   if (p1 != algorithm && p2 != secret)
      return DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2;

   // data must be RPCD2 + LE field
   if (request.remaining() != (lc + 1))
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   // LE must be ek(RPICC2 | RPCD2) bytes
   if (request.pop() != rpicc1.size() * 2)
      return DESFIRE_ISO_STATUS_WRONG_LENGTH;

   LOG_INFO(log, "\tp1(algorithm): 0x{02x}", {p1});
   LOG_INFO(log, "\tp2(secret): 0x{02x}", {p2});

   // generate randon challenge
   rpicc2 = rt::ByteBuffer::random(rpicc1.size());

   // read RPCD2 from request
   rpcd2 = request.getBuffer(rpcd1.size());

   // generate ek(RPICC2 | RPCD2)
   const rt::ByteBuffer ekRpicc2Rpcd2 = auth.cipher->encrypt(rpicc2.concat(rpcd2), auth.sessionIv);

   // add cryptogram to response
   response.put(ekRpicc2Rpcd2);

   // up to 24 bytes session key
   rt::ByteBuffer sessionKey = rt::ByteBuffer::zero(24);

   // generate session key, same as native auth
   if (auth.keyEntry->is3DES())
   {
      sessionKey.put(rpcd1.slice(0, 4));
      sessionKey.put(rpicc2.slice(0, 4));

      switch (auth.keyEntry->key.size())
      {
         case 8:
            sessionKey.put(rpcd1.slice(0, 4));
            sessionKey.put(rpicc2.slice(0, 4));
            break;

         case 16:

            if (auth.keyEntry->key.size() == 16)
            {
               // if both halves of the key are the same, use singled DES session key
               if (auth.keyEntry->key.slice(0, 8) == auth.keyEntry->key.slice(8, 8))
               {
                  sessionKey.put(rpcd1.slice(0, 4));
                  sessionKey.put(rpicc2.slice(0, 4));
               }
               else
               {
                  sessionKey.put(rpcd1.slice(4, 4));
                  sessionKey.put(rpicc2.slice(4, 4));
               }
            }

            break;

         case 24:
         {
            sessionKey.put(rpcd1.slice(6, 4));
            sessionKey.put(rpicc2.slice(6, 4));
            sessionKey.put(rpcd1.slice(12, 4));
            sessionKey.put(rpicc2.slice(12, 4));
         }
      }

      auth.mode = ISOAuthentication;
      auth.sessionIv = rt::ByteBuffer::zero(8);
   }

   // AES
   else
   {
      sessionKey.put(rpcd1.slice(0, 4));
      sessionKey.put(rpicc2.slice(0, 4));
      sessionKey.put(rpcd1.slice(12, 4));
      sessionKey.put(rpicc2.slice(12, 4));

      auth.mode = AESAuthentication;
      auth.sessionIv = rt::ByteBuffer::zero(16);
   }

   sessionKey.flip();

   // initialize cipher for session
   auth.sessionKey = sessionKey;
   auth.cipher->init(sessionKey, 0);

   // initialize auth status
   picc.setAuthentication(auth);

   // store status
   status = AUTHENTICATED;

   return DESFIRE_ISO_STATUS_OK;
}

}
