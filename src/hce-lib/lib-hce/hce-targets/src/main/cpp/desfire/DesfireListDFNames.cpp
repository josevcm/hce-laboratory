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
#include "DesfireListDFNames.h"

namespace hce::targets {

DesfireListDFNames::DesfireListDFNames(Instance &bundle) : Command(bundle)
{
}

int DesfireListDFNames::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "listDFNames");

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

      // get list of all user DF names, exclude MasterFile
      directoryList = picc.isoListDirectoryFiles();

      // data buffer for build response
      rt::ByteBuffer data(1024);

      // prepare data
      for (auto &df: directoryList)
      {
         LOG_INFO(log, "\taid: 0x{06x}", {df.appId});
         LOG_INFO(log, "\tfid: 0x{04x}", {df.isoId});
         LOG_INFO(log, "\tname: 0x{x}", {df.name});

         data.putInt(df.appId, 3, rt::ByteBuffer::LittleEndian);
         data.putInt(df.isoId, 2, rt::ByteBuffer::BigEndian);
         data.put(df.name);
      }

      data.flip();

      // prepare data to send directly in plain mode
      picc.encodeData(data, data.remaining(), PlainCommunication);
   }

   // return each name in separate frame
   if (picc.chaining < directoryList.size())
   {
      const DirectoryFile &df = directoryList[picc.chaining];

      // add data to response buffer
      return picc.sendData(response, 3 + 2 + df.name.remaining());
   }

   // send next block
   return picc.sendData(response);
}

}
