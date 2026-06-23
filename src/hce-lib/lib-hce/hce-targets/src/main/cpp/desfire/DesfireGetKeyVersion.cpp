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
#include "DesfireGetKeyVersion.h"

namespace hce::targets::desfire {

DesfireGetKeyVersion::DesfireGetKeyVersion(Instance &bundle) : Command(bundle)
{
}

int DesfireGetKeyVersion::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getKeyVersion");

   if (request.remaining() != 1)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, 1);

   // get key number
   unsigned int keyNo = request.getInt(1);

   LOG_INFO(log, "\tkeyNo: 0x{02x}", {keyNo});

   // check if key entry exists
   if (!picc.hasKeyEntry(keyNo))
      return DESFIRE_STATUS_NO_SUCH_KEY;

   // get key entry
   KeyEntry *keyEntry = picc.getKeyEntry(keyNo);

   LOG_INFO(log, "\tkeyVersion: 0x{02x}", {keyEntry->version});

   // add version to response
   response.putInt(keyEntry->version, 1);

   // send successful response
   return picc.sendAck(response);
}

}
