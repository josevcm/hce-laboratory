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
#include "DesfireSelectApplication.h"

namespace hce::targets {

DesfireSelectApplication::DesfireSelectApplication(Instance &bundle) : Command(bundle)
{
}

int DesfireSelectApplication::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "selectApplication");

   if (request.remaining() != 3)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // read AID
   const unsigned int aid = request.getInt(3);

   LOG_INFO(log, "\taid: 0x{06x}", {aid});

   // check if application exists
   if (!picc.hasApplication(aid))
      return DESFIRE_STATUS_APPLICATION_NOT_FOUND;

   // mark application as selected
   picc.selectApplication(aid);

   // invalidate authentication state
   picc.invalidateAuth();

   // send successful response
   return picc.sendAck(response);
}

}
