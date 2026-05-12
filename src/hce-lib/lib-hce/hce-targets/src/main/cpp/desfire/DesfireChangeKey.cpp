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

#include <hce/crc/CRC.h>

#include "Instance.h"
#include "DesfireChangeKey.h"

namespace hce::targets {

DesfireChangeKey::DesfireChangeKey(Instance &bundle) : Command(bundle)
{
}

int DesfireChangeKey::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "changeKey");

   // check minimum command length
   if (request.remaining() < 1)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // check if authenticated
   if (!picc.isAuthenticated())
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // decode parameters
   const unsigned int keyId = request.getInt(1);

   // get key entry to change
   unsigned int keyNo = keyId & 0x3F;
   unsigned int keyType = keyId >> 6;

   LOG_INFO(log, "\tkeyId: 0x{02x}", {keyId});
   LOG_INFO(log, "\tkeyNo: 0x{02x}", {keyNo});
   LOG_INFO(log, "\tkeyType: 0x{02x}", {keyType});

   // check key type (upper 2 bits of keyNo)
   if (keyType != KeyType2K3DES && keyType != KeyType3K3DES && keyType != KeyTypeAES)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // key type change is ONLY allowed for AID = 0 (master app)
   if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID) && keyType != 0)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if key to be changed exists
   if (!picc.hasKeyEntry(keyNo))
      return DESFIRE_STATUS_NO_SUCH_KEY;

   // check if change is allowed for current authentication status
   if (!picc.isKeyChangeable(keyNo))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // get key entry
   const auto keyEntry = picc.getKeyEntry(keyNo);

   // key entry to change
   switch (picc.auth->mode)
   {
      case LegacyAuthentication:
         return changeKeyEV0(keyEntry, keyType, request, response);

      // ISOAuthentication or AESAuthentication
      default:
         return changeKeyEV1(keyEntry, keyType, request, response);
   }
}

int DesfireChangeKey::changeKeyEV0(KeyEntry *keyEntry, unsigned int keyType, rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   if (request.remaining() != 24)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // buffer for encrypted data
   const rt::ByteBuffer cryptogram = request.getBuffer(24);

   // decrypt data
   rt::ByteBuffer plain = picc.auth->cipher->decrypt(cryptogram);

   // new key data
   rt::ByteBuffer newKey;

   // change key type only apply to card master KEY
   unsigned int newKeyType = picc.isApplicationSelected(DESFIRE_MASTER_APP_ID) ? keyType : keyEntry->type;

   // new key version
   unsigned int newKeyVersion = 0;

   // case 1: key used for authentication is different from the key to be changed
   if (picc.auth->keyEntry->id != keyEntry->id)
   {
      // read XOR of new key and old key
      const rt::ByteBuffer newKeyXorOldKey = plain.getBuffer(16);

      // verify CRC of changed key
      if (const unsigned short crc = plain.getInt(2); crc != CRC::iso14443A(newKeyXorOldKey))
         return DESFIRE_STATUS_INTEGRITY_ERROR;

      // calculate new key value by xoring with old key
      newKey = newKeyXorOldKey ^ keyEntry->key;

      // verify CRC of new key
      if (const unsigned short crc = plain.getInt(2); crc != CRC::iso14443A(newKey))
         return DESFIRE_STATUS_INTEGRITY_ERROR;
   }
   else
   {
      // read new key
      newKey = plain.getBuffer(16);

      // verify CRC of new key
      if (const unsigned short crc = plain.getInt(2); crc != CRC::iso14443A(newKey))
         return DESFIRE_STATUS_INTEGRITY_ERROR;

      // change current key invalidate authentication state
      picc.auth = nullptr;
   }

   // update key entry
   keyEntry->key = newKey;
   keyEntry->type = newKeyType;
   keyEntry->version = newKeyVersion;

   LOG_INFO(log, "\tnewKey: 0x{x}", {newKey});
   LOG_INFO(log, "\tnewKeyType: 0x{02x}", {newKeyType});
   LOG_INFO(log, "\tnewKeyVersion: 0x{02x}", {newKeyVersion});

   // set picc as dirty state
   picc.dirty = true;

   return DESFIRE_STATUS_OK;
}

int DesfireChangeKey::changeKeyEV1(KeyEntry *keyEntry, unsigned int keyType, rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   // check length
   if (request.remaining() % 8 || (picc.auth->mode == AESAuthentication && request.remaining() % 16))
      return DESFIRE_STATUS_LENGTH_ERROR;

   // buffer for encrypted data
   rt::ByteBuffer cryptogram(request.remaining());

   // read encrypted data
   request.get(cryptogram);

   // decrypt data
   rt::ByteBuffer plain = picc.auth->cipher->decrypt(cryptogram, picc.auth->sessionIv);

   // new key data
   rt::ByteBuffer newKey;

   // change key type only apply to card master KEY
   unsigned int newKeyType = picc.isApplicationSelected(DESFIRE_MASTER_APP_ID) ? keyType : keyEntry->type;

   // new key version
   unsigned int newKeyVersion = 0;

   // case 1: key used for authentication is DIFFERENT from the key to be changed
   if (picc.auth->keyEntry->id != keyEntry->id)
   {
      rt::ByteBuffer newKeyXorOldKey(keyEntry->key.size());
      rt::ByteBuffer newKeyCrcBuffer(keyEntry->key.size() + 3);

      // read XOR of new key and old key
      plain.get(newKeyXorOldKey);

      // crc is calculated with CMD + keyId + newKey + [newKeyVersion for AES]
      newKeyCrcBuffer.put(DESFIRE_CMD_CHANGE_KEY).put(keyEntry->id).put(newKeyXorOldKey);

      // read version for AES keys and add to crcBuffer
      if (newKeyType == KeyTypeAES)
      {
         newKeyVersion = plain.getInt(1);
         newKeyCrcBuffer.put(newKeyVersion);
      }

      // prepare crcBuffer to process
      newKeyCrcBuffer.flip();

      // verify CRC of changed key
      if (const unsigned int crc = plain.getInt(4); crc != CRC::ccitt32(newKeyCrcBuffer))
         return DESFIRE_STATUS_INTEGRITY_ERROR;

      // calculate new key value by xoring with old key
      newKey = newKeyXorOldKey ^ keyEntry->key;

      // verify CRC of new key
      if (const unsigned int crc = plain.getInt(4); crc != CRC::ccitt32(newKey))
         return DESFIRE_STATUS_INTEGRITY_ERROR;
   }
   // case 2: key used for authentication is the SAME as key to be changed
   else
   {
      // buffer for new key (24 byte for 3K3DES and 16 byte for 2K3DES / AES)
      newKey = rt::ByteBuffer(newKeyType == KeyType3K3DES ? 24 : 16);

      // buffer for key crc
      rt::ByteBuffer newKeyCrcBuffer(newKey.size() + 3);

      // read new key
      plain.get(newKey);

      // crc is calculated with CMD + newKeyId + newKey + [keyVersion]
      newKeyCrcBuffer.put(DESFIRE_CMD_CHANGE_KEY).put(keyType << 6 | keyEntry->id).put(newKey);

      // read version for AES keys type (change key type only apply for master card key)
      if (newKeyType == KeyTypeAES)
      {
         newKeyVersion = plain.getInt(1);
         newKeyCrcBuffer.put(newKeyVersion);
      }

      // prepare crcBuffer to process
      newKeyCrcBuffer.flip();

      // verify CRC of new key
      if (const unsigned int crc = plain.getInt(4); crc != CRC::ccitt32(newKeyCrcBuffer))
         return DESFIRE_STATUS_INTEGRITY_ERROR;

      // change current key invalidate authentication state
      picc.invalidateAuth();
   }

   // update key entry
   keyEntry->key = newKey;
   keyEntry->type = newKeyType;
   keyEntry->version = newKeyVersion;

   LOG_INFO(log, "\tnewKey: 0x{x}", {newKey});
   LOG_INFO(log, "\tnewKeyType: 0x{02x}", {newKeyType});
   LOG_INFO(log, "\tnewKeyVersion: 0x{02x}", {newKeyVersion});

   picc.dirty = true;
   // send successful response
   return picc.sendAck(response);
}

}
