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

#ifndef HCE_DESFIRE_FILE_ENTRY_H
#define HCE_DESFIRE_FILE_ENTRY_H

#include <rt/ByteBuffer.h>

// --- Value File features bits ---
#define DESFIRE_VALUE_FILE_LIMITED_CREDIT_FEATURE     0x01
#define DESFIRE_VALUE_FILE_FREE_ACCESS_FEATURE        0x02

// --- File change bits ---
#define DESFIRE_FILE_CHANGED_DATA   0x01
#define DESFIRE_FILE_CHANGED_DEBIT  0x02
#define DESFIRE_FILE_CHANGED_CREDIT 0x04

namespace hce::targets {
enum FileType
{
   StandardFile = 0x00,
   BackupFile = 0x01,
   ValueFile = 0x02,
   LinearRecordFile = 0x03,
   CyclicRecordFile = 0x04
};

enum CommSettings
{
   PlainCommunication = 0x00,
   MACedCommunication = 0x01,
   CryptCommunication = 0x03,
};

struct FileEntry
{
   // common file attributes
   unsigned int fileId;
   unsigned int fileType;
   unsigned int fileSize;
   unsigned int isoId;
   unsigned int commSettings;
   unsigned int accessRights;

   // record file attributes
   unsigned int recordSize;
   unsigned int recordLimit;

   // value file attributes
   int value;
   int lowerLimit;
   int upperLimit;
   int creditLimit;
   int backupValue;
   int backupCreditLimit;
   unsigned int features;

   // data file attributes
   rt::ByteBuffer data;
   rt::ByteBuffer backup;

   // file change count
   unsigned int changes;

   bool isDataFile() const
   {
      return fileType == StandardFile || fileType == BackupFile;
   }

   bool isValueFile() const
   {
      return fileType == ValueFile;
   }

   bool isRecordFile() const
   {
      return fileType == LinearRecordFile || fileType == CyclicRecordFile;
   }

   bool isLinearRecordFile() const
   {
      return fileType == LinearRecordFile;
   }

   bool isCyclicRecordFile() const
   {
      return fileType == CyclicRecordFile;
   }

   /*
    * check if limited credit feature is enabled
    */
   bool isLimitedCreditEnabled() const
   {
      return features & DESFIRE_VALUE_FILE_LIMITED_CREDIT_FEATURE;
   }

   /*
    * check if free read value feature is enabled
    */
   bool isFreeReadAccessEnabled() const
   {
      return features & DESFIRE_VALUE_FILE_FREE_ACCESS_FEATURE;
   }

   /*
    * check if file is dirty (changes made since las commit)
    */
   bool isChanged() const
   {
      return changes;
   }

   /*
    * check if file has configured with PLAIN communication mode
    */
   bool isPlainComm() const
   {
      return commSettings == PlainCommunication;
   }

   /*
    * check if file has configured with MACED communication mode
    */
   bool isMACedComm() const
   {
      return commSettings == MACedCommunication;
   }

   /*
    * check if file has configured with FULL ENCIPHERED communication mode
    */
   bool isCryptoComm() const
   {
      return commSettings == CryptCommunication;
   }

   /*
    * get read key
    */
   unsigned int readKey() const
   {
      return accessRights >> 12 & 0x0F;
   }

   /*
    * get write key
    */
   unsigned int writeKey() const
   {
      return accessRights >> 8 & 0x0F;
   }

   /*
    * get readWrite key
    */
   unsigned int readWriteKey() const
   {
      return accessRights >> 4 & 0x0F;
   }

   /*
    * get change key
    */
   unsigned int changeKey() const
   {
      return accessRights & 0x0F;
   }

   /*
    * return total file size in bytes
    */
   unsigned int length() const
   {
      return data.remaining();
   }

   /*
    * return record count
    */
   unsigned int records() const
   {
      assert(fileType == LinearRecordFile || fileType == CyclicRecordFile);
      return data.remaining() / recordSize;
   }

   /*
    * return record starting from newest
    */
   rt::ByteBuffer record(const unsigned int n) const
   {
      assert(fileType == LinearRecordFile || fileType == CyclicRecordFile);
      assert(recordSize * n <= data.remaining());
      return data.slice(recordSize * n, recordSize);
   }

   void credit(const int v)
   {
      backupValue += v;
      changes |= DESFIRE_FILE_CHANGED_CREDIT;
   }

   void debit(const int v)
   {
      backupValue -= v;
      backupCreditLimit += v;
      changes |= DESFIRE_FILE_CHANGED_DEBIT;
   }

   void limitedCredit(const int v)
   {
      assert(v <= backupCreditLimit);
      backupValue += v;
      backupCreditLimit = 0;
      changes |= DESFIRE_FILE_CHANGED_CREDIT;
   }

   int allowedCreditLimit() const
   {
      return changes & DESFIRE_FILE_CHANGED_DEBIT ? backupCreditLimit : creditLimit;
   }

   /*
    * write data to file or record
    */
   void write(const rt::ByteBuffer &value, unsigned int offset, unsigned int length);

   /*
    * commit changes to file contents
    */
   bool commit();

   /*
    * discard changes to file
    */
   bool rollback();

   /*
    * compute file memory usage
    */
   int usedMemory();
};

}

#endif
