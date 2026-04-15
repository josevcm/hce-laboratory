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
#include "DesfireGetVersion.h"

namespace hce::targets {

DesfireGetVersion::DesfireGetVersion(Instance &bundle) : Command(bundle)
{
}

int DesfireGetVersion::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "getVersion [{}]", {picc.chaining});

   if (picc.chaining == 0)
   {
      picc.updateIv(request, 0);

      rt::ByteBuffer data(64);

      // hardware related information
      data.putInt(picc.hardwareVendor, 1);
      data.putInt(picc.hardwareType, 1);
      data.putInt(picc.hardwareSubtype, 1);
      data.putInt(picc.hardwareVersion >> 8, 1);
      data.putInt(picc.hardwareVersion & 0xFF, 1);
      data.putInt(picc.hardwareStorage, 1);
      data.putInt(picc.hardwareProtocol, 1);

      // software related information
      data.putInt(picc.softwareVendor, 1);
      data.putInt(picc.softwareType, 1);
      data.putInt(picc.softwareSubtype, 1);
      data.putInt(picc.softwareVersion >> 8, 1);
      data.putInt(picc.softwareVersion & 0xFF, 1);
      data.putInt(picc.softwareStorage, 1);
      data.putInt(picc.softwareProtocol, 1);

      // unique serial number, batch number, year and calendar week of production
      data.put(picc.uid, 7);
      data.putLong(picc.batchNumber, 5);
      data.putInt(picc.productionWeek, 1);
      data.putInt(picc.productionYear, 1);

      // commit buffer
      data.flip();

      // encode data
      picc.encodeData(data, data.remaining(), PlainCommunication);
   }

   // split data to send
   if (picc.chaining < 2)
      return picc.sendData(response, 7);

   return picc.sendData(response);
}

}
