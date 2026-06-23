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

#include "FileEntry.h"

#define DESFIRE_NV_BLOCK_SIZE 32

namespace hce::targets::desfire {

void FileEntry::write(const rt::ByteBuffer &value, unsigned int offset, unsigned int length)
{
   assert(fileType == StandardFile || fileType == BackupFile || fileType == LinearRecordFile || fileType == CyclicRecordFile);

   switch (fileType)
   {
      case StandardFile:
         data.set(value, offset, length);
         break;

      case LinearRecordFile:
      case CyclicRecordFile:
         // initialize new empty record
         if (!backup.isValid())
            backup = rt::ByteBuffer::zero(recordSize);

      case BackupFile:
         // set record data
         backup.set(value, offset, length);
         break;
   }

   changes |= DESFIRE_FILE_CHANGED_DATA;
}

/*
 * commit changes to file contents
 */
bool FileEntry::commit()
{
   if (!changes)
      return false;

   switch (fileType)
   {
      case BackupFile:
      {
         data = backup.copy();
         break;
      }

      case CyclicRecordFile:
      case LinearRecordFile:
      {
         // check if file is cleared
         if (backup.isEmpty())
         {
            backup.reset();
            data.clear().flip();
            break;
         }

         // make space to append new record
         if (records() < recordLimit)
            data.room(recordSize);

         // shift old records
         data.shift(rt::ByteBuffer::Right, recordSize);

         // set new record
         data.set(backup, 0);

         // clear backup buffer
         backup.reset();

         break;
      }

      case ValueFile:
      {
         // update file value
         value = backupValue;

         // commit limit if limited credit feature file is enabled
         if (isLimitedCreditEnabled())
            creditLimit = backupCreditLimit;

         // reset backup credit limit
         backupCreditLimit = 0;

         break;
      }
   }

   changes = 0;

   return true;
}

/*
 * discard changes to file
 */
bool FileEntry::rollback()
{
   if (!changes)
      return false;

   switch (fileType)
   {
      case BackupFile:
      {
         backup = data.copy();
         break;
      }

      case LinearRecordFile:
      case CyclicRecordFile:
      {
         // clear backup buffer
         backup.reset();
         break;
      }

      case ValueFile:
      {
         // restore backup value
         backupValue = value;
         backupCreditLimit = 0;
         break;
      }
   }

   changes = 0;

   return true;
}

/*
 * discard changes to file
 */
int FileEntry::usedMemory()
{
   unsigned int bytesUsed = 0;

   switch (fileType)
   {
      case ValueFile:
      case StandardFile:
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(fileSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE;
         break;

      case BackupFile:
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(fileSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE * 2;
         break;

      case CyclicRecordFile:
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(recordSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE;
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(fileSize + recordSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE;
         break;

      case LinearRecordFile:
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(recordSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE;
         bytesUsed += static_cast<int>(std::ceil(static_cast<float>(fileSize) / DESFIRE_NV_BLOCK_SIZE)) * DESFIRE_NV_BLOCK_SIZE;
   }

   return bytesUsed;
}

}
