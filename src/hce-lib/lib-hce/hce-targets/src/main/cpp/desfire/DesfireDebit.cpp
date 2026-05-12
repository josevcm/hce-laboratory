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
#include "DesfireDebit.h"

namespace hce::targets {

DesfireDebit::DesfireDebit(Instance &bundle) : Command(bundle)
{
}

int DesfireDebit::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "debit");

   // check minimum command length
   if (request.remaining() < 5)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // application district of master APP must be selected
   if (!picc.isApplicationSelected())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // copy header for further CMAC processing
   picc.readHeader(request, 1);

   // decode parameters
   const unsigned int fileId = request.getInt(1);

   // check parameters
   if (fileId > DESFIRE_MAX_FILE_ID)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check if file exists
   if (!picc.hasFile(fileId))
      return DESFIRE_STATUS_FILE_NOT_FOUND;

   // get selected file to read
   FileEntry *file = picc.getFile(fileId);

   // check file type
   if (!file->isValueFile())
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // receive file data
   if (const int status = picc.decodeData(request, 4, file->commSettings, {file->readKey(), file->writeKey(), file->readWriteKey()}); status != DESFIRE_STATUS_OK)
      return status;

   // update file
   return debit(file, picc.buffer, response);
}

int DesfireDebit::debit(FileEntry *file, rt::ByteBuffer &data, rt::ByteBuffer &response) const
{
   // read debit value
   const int debit = static_cast<int>(data.getInt(4));

   // debit value must be always positive
   if (debit < 0)
      return DESFIRE_STATUS_PARAMETER_ERROR;

   // check lower limit
   if (file->backupValue - debit < file->lowerLimit)
      return DESFIRE_STATUS_BOUNDARY_ERROR;

   // update file value
   file->debit(debit);

   // send successful response
   return picc.sendAck(response);
}

}
