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
#include "DesfireListFiles.h"

namespace hce::targets {

DesfireListFiles::DesfireListFiles(Instance &bundle) : Command(bundle)
{
}

int DesfireListFiles::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "listFiles");

   if (picc.chaining == 0)
   {
      // application district of master APP must be selected
      if (!picc.isApplicationSelected())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // and authenticated with master key
      if (!picc.isFreeDirectoryListing() && !picc.isAuthenticatedWithMasterKey())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // copy header for further CMAC processing and update IV
      picc.updateIv(request, 0);

      auto data = rt::ByteBuffer(picc.application->files.size());

      for (const auto fileId: picc.listFiles())
      {
         data.put(fileId);
         LOG_INFO(log, "\tfileId: 0x{02x}", {fileId});
      }

      data.flip();

      // prepare data to send directly in plain mode
      picc.encodeData(data, data.remaining(), PlainCommunication, {0x0E});
   }

   // send next block
   return picc.sendData(response);
}

}
