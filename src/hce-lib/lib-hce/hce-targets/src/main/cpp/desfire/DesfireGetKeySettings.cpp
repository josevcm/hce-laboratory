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
#include "DesfireGetKeySettings.h"

namespace hce::targets {

DesfireGetKeySettings::DesfireGetKeySettings(Instance &bundle) : Command(bundle)
{
}

int DesfireGetKeySettings::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getKeySettings");

   // update sessionIv
   picc.updateIv(request, 0);

   // require master key authentication when directory listing is not free
   if (!picc.isFreeDirectoryListing() && !picc.isAuthenticatedWithMasterKey())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // prepare data
   const unsigned char keySettings1 = picc.application->keySettings;
   const unsigned char keySettings2 = picc.application->cryptoMode << 6 | picc.application->maximumKeys;

   // add key settings
   response.put(keySettings1);
   response.put(keySettings2);

   // send successful response
   return picc.sendAck(response);
}

}
