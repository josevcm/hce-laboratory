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
#include "DesfireCreateApplication.h"

namespace hce::targets::desfire {

DesfireCreateApplication::DesfireCreateApplication(Instance &bundle) : Command(bundle)
{
}

int DesfireCreateApplication::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "createApplication");

   // command length
   const unsigned int length = request.remaining();

   // check minimum command length
   if (length < 5)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // master application must be selected
   if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // check if master key auth is required for create/delete applications
   if (!picc.isFreeCreateDelete() && !picc.isAuthenticatedWithMasterKey())
      return DESFIRE_STATUS_AUTHENTICATION_ERROR;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, length);

   // decode parameters
   unsigned int aid = request.getInt(3);
   unsigned int keySettings1 = request.getInt(1);
   unsigned int keySettings2 = request.getInt(1);

   // extra parameters for ISO
   const bool isoEnabled = (keySettings2 & 0x20) != 0;
   unsigned int isoId = 0;
   rt::ByteBuffer isoName = {};

   // get ISO parameters
   if (isoEnabled)
   {
      if (length < 7)
         return DESFIRE_STATUS_LENGTH_ERROR;

      isoId = request.getInt(2);

      if (length > 7)
         isoName = request.getBuffer(request.remaining());
   }

   LOG_INFO(log, "\taid: 0x{06x}", {aid});
   LOG_INFO(log, "\tkeySettings1: 0x{02x}", {keySettings1});
   LOG_INFO(log, "\tkeySettings2: 0x{02x}", {keySettings2});

   if (length > 5)
      LOG_INFO(log, "\tisoId: 0x{02x}", {isoId});

   if (length > 7)
      LOG_INFO(log, "\tisoName: 0x{x}", {isoName});

   // PICC application cannot be created
   if (aid == DESFIRE_MASTER_APP_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check maximum applications limit (master app + 28 user apps)
   if (picc.applications.size() == 29)
      return DESFIRE_STATUS_COUNT_ERROR;

   // check keys limits
   if ((keySettings2 & 0x0f) > 14 || (keySettings2 & 0x0f) == 0)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if application already exists
   if (picc.hasApplication(aid))
      return DESFIRE_STATUS_DUPLICATE_ERROR;

   // create new application with default keys
   Application app {
      .aid = aid,
      .cryptoMode = keySettings2 >> 6,
      .keySettings = keySettings1,
      .maximumKeys = keySettings2 & 0x0F,
      .isoEnabled = isoEnabled,
      .isoId = isoId,
      .isoName = isoName
   };

   // add default keys
   for (unsigned int i = 0; i < app.maximumKeys; ++i)
   {
      unsigned int version = 0x00;

      switch (app.cryptoMode)
      {
         case KeyType2K3DES:
         {
            rt::ByteBuffer key = DESFIRE_MASTER_KEY_DEFAULT_2K3DES;

            if (picc.defaultKey.remaining() >= 16)
            {
               key = picc.defaultKey.slice(0, 16);
               version = picc.defaultKeyVersion;
            }

            app.keys.emplace(i, KeyEntry {.id = i, .type = KeyType2K3DES, .version = version, .key = key});

            break;
         }

         case KeyType3K3DES:
         {
            rt::ByteBuffer key = DESFIRE_MASTER_KEY_DEFAULT_3K3DES;

            if (picc.defaultKey.remaining() >= 24)
            {
               key = picc.defaultKey.slice(0, 24);
               version = picc.defaultKeyVersion;
            }

            app.keys.emplace(i, KeyEntry {.id = i, .type = KeyType3K3DES, .version = version, .key = key});

            break;
         }

         case KeyTypeAES:
         {
            rt::ByteBuffer key = DESFIRE_MASTER_KEY_DEFAULT_AES;

            if (picc.defaultKey.remaining() >= 16)
            {
               key = picc.defaultKey.slice(0, 16);
               version = picc.defaultKeyVersion;
            }

            app.keys.emplace(i, KeyEntry {.id = i, .type = KeyTypeAES, .version = version, .key = key});

            break;
         }

         default:
            return DESFIRE_STATUS_APPL_INTEGRITY_ERROR;
      }
   }

   // add new application to the PICC
   picc.addApplication(app);

   // send successful response
   return picc.sendAck(response);
}

}
