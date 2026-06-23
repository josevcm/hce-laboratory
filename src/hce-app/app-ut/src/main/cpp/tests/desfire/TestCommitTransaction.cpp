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

#include "TestCommitTransaction.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x020304;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int VALUE_FILE_ID = 0x00;
static constexpr unsigned int DATA_FILE_ID = 0x01;
static constexpr unsigned int RECORD_FILE_ID = 0x02;
static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testCommitTransaction(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- commitTransaction (value/data/record files) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x020304) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x020304) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createValueFile(VALUE_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 5000, 1000, false);
      ctx.check("createValueFile(id=0, init=1000) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(DATA_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 32);
      ctx.check("createBackupFile(id=1, size=32) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createLinearRecordFile(RECORD_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 8, 4);
      ctx.check("createLinearRecordFile(id=2) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "\n  Test 1: Multiple value operations");

      {
         int value = -1;
         card.getValue(VALUE_FILE_ID, COMM_PLAIN, value);
         ctx.check("initial value is 1000", value == 1000);
      }

      sw = card.credit(VALUE_FILE_ID, 500, COMM_PLAIN);
      ctx.check("credit(500) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.debit(VALUE_FILE_ID, 200, COMM_PLAIN);
      ctx.check("debit(200) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.credit(VALUE_FILE_ID, 100, COMM_PLAIN);
      ctx.check("credit(100) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction persists all ops, returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         int value = -1;
         card.getValue(VALUE_FILE_ID, COMM_PLAIN, value);
         ctx.check("final value is 1400 (1000+500-200+100)", value == 1400);
      }

      LOG_INFO(ctx.log, "\n  Test 2: Write data with commit");
      rt::ByteBuffer dataToWrite(16);
      dataToWrite.fill(0xAA, 16);
      dataToWrite.flip();

      sw = card.writeData(DATA_FILE_ID, 0, 16, COMM_PLAIN, dataToWrite);
      ctx.check("writeData(16 bytes) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after writeData returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_NO_CHANGES) return;

      {
         rt::ByteBuffer readBack(16);
         sw = card.readData(DATA_FILE_ID, 0, 16, COMM_PLAIN, readBack);
         ctx.check("readData after commit returns STATUS_OK", sw == STATUS_OK);
         ctx.check("read data matches written data (16 bytes)", readBack == dataToWrite);
      }

      LOG_INFO(ctx.log, "\n  Test 3: Write records with commit");
      rt::ByteBuffer record1(8);
      record1.fill(0x55, 8);
      record1.flip();

      sw = card.writeRecord(RECORD_FILE_ID, 0, 8, COMM_PLAIN, record1);
      ctx.check("writeRecord(rec1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         FileSettings fs {};
         card.getFileSettings(RECORD_FILE_ID, fs);
         ctx.check("currentRecords is 1 before commit", fs.currentRecords == 1);
      }

      sw = card.commitTransaction();
      ctx.check("commitTransaction after first record returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         FileSettings fs {};
         card.getFileSettings(RECORD_FILE_ID, fs);
         ctx.check("currentRecords is 2 after one commit", fs.currentRecords == 2);
      }

      rt::ByteBuffer record2(8);
      record2.fill(0x66, 8);
      record2.flip();

      sw = card.writeRecord(RECORD_FILE_ID, 0, 8, COMM_PLAIN, record2);
      ctx.check("writeRecord(rec2) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         FileSettings fs {};
         card.getFileSettings(RECORD_FILE_ID, fs);
         ctx.check("currentRecords is 2 before commit", fs.currentRecords == 2);
      }

      sw = card.commitTransaction();
      ctx.check("commitTransaction after second record returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         FileSettings fs {};
         card.getFileSettings(RECORD_FILE_ID, fs);
         ctx.check("currentRecords is 2 after two commits", fs.currentRecords == 2);
      }

      sw = card.deleteFile(VALUE_FILE_ID);
      ctx.check("deleteFile(value) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(DATA_FILE_ID);
      ctx.check("deleteFile(data) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(RECORD_FILE_ID);
      ctx.check("deleteFile(record) returns STATUS_OK", sw == STATUS_OK);
   }();

   LOG_DEBUG(ctx.log, "  teardown, recover card status");

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
