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
#include "DesfireGetIsoFileIDs.h"

namespace hce::targets {

DesfireGetIsoFileIDs::DesfireGetIsoFileIDs(Instance &bundle) : Command(bundle)
{
}

int DesfireGetIsoFileIDs::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getIsoFileIDs");

   if (picc.chaining == 0)
   {
      // application must be selected
      if (!picc.isApplicationSelected())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // require master key authentication when directory listing is not free
      if (!picc.isFreeDirectoryListing() && !picc.isAuthenticatedWithMasterKey())
         return DESFIRE_STATUS_PERMISSION_DENIED;

      // copy header for further CMAC processing and update IV
      picc.updateIv(request, 0);

      auto data = rt::ByteBuffer(picc.application->files.size() * 2);

      for (const auto fileId: picc.listFiles())
      {
         const FileEntry *file = picc.getFile(fileId);

         if (file->isoId == 0)
            continue;

         data.put(file->isoId & 0xFF);
         data.put((file->isoId >> 8) & 0xFF);

         LOG_INFO(log, "\tisoId: 0x{04x}", {file->isoId});
      }

      data.flip();

      picc.encodeData(data, data.remaining(), PlainCommunication, {0x0E});
   }

   return picc.sendData(response);
}

}
