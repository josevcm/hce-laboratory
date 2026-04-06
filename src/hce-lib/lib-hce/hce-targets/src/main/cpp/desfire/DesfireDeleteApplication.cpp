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
#include "DesfireDeleteApplication.h"

namespace hce::targets {

DesfireDeleteApplication::DesfireDeleteApplication(Instance &bundle) : Command(bundle)
{
}

int DesfireDeleteApplication::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "deleteApplication");

   // check minimum command length
   if (request.remaining() < 3)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request,3);

   // decode parameters
   unsigned int aid = request.getInt(3);

   LOG_INFO(log, "\taid: 0x{06x}", {aid});

   // check if delete applications is allowed
   if (picc.isFreeCreateDelete())
   {
      // for free delete, a PICC application or target application must be selected
      if (!picc.isApplicationSelected(aid) && !picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // and authenticated with master key
      if (!picc.isAuthenticatedWithMasterKey())
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }
   else
   {
      // delete application requires a PICC to be selected
      if (!picc.isApplicationSelected(DESFIRE_MASTER_APP_ID))
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;

      // and authenticated with master key
      if (!picc.isAuthenticatedWithMasterKey())
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }

   // PICC application cannot be deleted
   if (aid == DESFIRE_MASTER_APP_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // if application to be deleted is selected, deselect and set master
   if (picc.isApplicationSelected(aid))
      picc.selectApplication(DESFIRE_MASTER_APP_ID);

   // delete application
   picc.deleteApplication(aid);

   // send successful response
   return picc.sendAck(response);
}

}
