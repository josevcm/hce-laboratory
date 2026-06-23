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
#include "DesfireCommitTransaction.h"

namespace hce::targets::desfire {

DesfireCommitTransaction::DesfireCommitTransaction(Instance &bundle) : Command(bundle)
{
}

int DesfireCommitTransaction::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "commit");

   // application distinct of master app must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // if no data to commit, finish with error
   if (!picc.commitData())
      return DESFIRE_STATUS_NO_CHANGES;

   // copy header for further CMAC processing and update IV
   picc.updateIv(request, 0);

   // send successful response
   return picc.sendAck(response);
}

}
