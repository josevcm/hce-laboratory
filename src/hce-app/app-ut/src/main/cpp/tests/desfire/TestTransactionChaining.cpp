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

#include <rt/ByteBuffer.h>

#include "TestTransactionChaining.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x040506;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int LARGE_DATA_FILE_ID = 0x00;
static constexpr unsigned int LARGE_RECORD_FILE_ID = 0x01;
static constexpr const char *APP_KEY_ZEROS   = "00000000000000000000000000000000";

void testTransactionChaining(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- transactionChaining (APDU chaining > 60 bytes) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x040506) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x040506) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(LARGE_DATA_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 256);
      ctx.check("createBackupFile(size=256) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createLinearRecordFile(LARGE_RECORD_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 64, 4);
      ctx.check("createLinearRecordFile(recSize=64, maxRec=4) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "\n  Test 1: Write large data (256 bytes)");
      rt::ByteBuffer largeData(256);
      for (unsigned int i = 0; i < 256; i++)
         largeData.putInt((i % 256), 1);
      largeData.flip();

      sw = card.writeData(LARGE_DATA_FILE_ID, 0, 256, COMM_PLAIN, largeData);
      ctx.check("writeData(256 bytes) with chaining returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction after chained write returns STATUS_OK", sw == STATUS_OK);

      rt::ByteBuffer readLargeData(256);
      sw = card.readData(LARGE_DATA_FILE_ID, 0, 256, COMM_PLAIN, readLargeData);
      ctx.check("readData(256 bytes) returns STATUS_OK", sw == STATUS_OK);

      if (sw == STATUS_OK)
         ctx.check("read 256 bytes match written", readLargeData.remaining() == 256);

      LOG_INFO(ctx.log, "\n  Test 2: Write large record (64 bytes)");

      rt::ByteBuffer largeRecord(64);
      for (unsigned int i = 0; i < 64; i++)
         largeRecord.putInt(0xAA + (i % 16), 1);

      largeRecord.flip();

      sw = card.writeRecord(LARGE_RECORD_FILE_ID, 0, 64, COMM_PLAIN, largeRecord);
      ctx.check("writeRecord(64 bytes) with chaining returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction after record write returns STATUS_OK", sw == STATUS_OK);

      rt::ByteBuffer readLargeRecord(64);
      sw = card.readRecords(LARGE_RECORD_FILE_ID, 0, 1, COMM_PLAIN, readLargeRecord);
      ctx.check("readRecords after chained write returns STATUS_OK", sw == STATUS_OK);

      if (sw == STATUS_OK)
         ctx.check("read record matches (64 bytes)", readLargeRecord.remaining() == 64);

      LOG_INFO(ctx.log, "\n  Test 3: Multiple chained records");

      rt::ByteBuffer largeRecord2(64);

      for (unsigned int i = 0; i < 64; i++)
         largeRecord2.putInt(0x55 + (i % 16), 1);

      largeRecord2.flip();

      sw = card.writeRecord(LARGE_RECORD_FILE_ID, 0, 64, COMM_PLAIN, largeRecord2);
      ctx.check("writeRecord(rec2, 64 bytes) returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction persists chained writes, returns STATUS_OK", sw == STATUS_OK);

      rt::ByteBuffer largeRecord3(64);

      for (unsigned int i = 0; i < 64; i++)
         largeRecord3.putInt(0x99 + (i % 16), 1);

      largeRecord3.flip();

      sw = card.writeRecord(LARGE_RECORD_FILE_ID, 0, 64, COMM_PLAIN, largeRecord3);
      ctx.check("writeRecord(rec3, 64 bytes) returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction persists chained writes, returns STATUS_OK", sw == STATUS_OK);

      {
         FileSettings fs {};
         card.getFileSettings(LARGE_RECORD_FILE_ID, fs);
         ctx.check("currentRecords is 3 after three writes", fs.currentRecords == 3);
      }

      sw = card.deleteFile(LARGE_DATA_FILE_ID);
      ctx.check("deleteFile(large_data) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(LARGE_RECORD_FILE_ID);
      ctx.check("deleteFile(large_record) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}

