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
#include "DesfireListApplications.h"

namespace hce::targets {

DesfireListApplications::DesfireListApplications(Instance &bundle) : Command(bundle)
{
}

int DesfireListApplications::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "listApplications");

   if (picc.chaining == 0)
   {
      // PICC application must be selected
      if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // and authenticated with master key
      if (!picc.isFreeDirectoryListing() && !picc.isAuthenticatedWithMasterKey())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // copy header for further CMAC processing and update IV
      picc.updateIv(request, 0);

      rt::ByteBuffer data(picc.applications.size() * 3);

      for (const auto aid: picc.listApplications())
      {
         if (aid != 0)
         {
            data.putInt(aid, 3);
            LOG_INFO(log, "\taid: 0x{06x}", {aid});
         }
      }

      data.flip();

      // prepare data to send directly in plain mode
      picc.encodeData(data, data.remaining(), PlainCommunication);
   }

   // send next block
   return picc.sendData(response);
}

}
