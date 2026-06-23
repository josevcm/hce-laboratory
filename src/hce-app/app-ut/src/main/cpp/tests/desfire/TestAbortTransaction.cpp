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

#include "TestAbortTransaction.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x030405;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int VALUE_FILE_ID = 0x00;
static constexpr unsigned int DATA_FILE_ID = 0x01;
static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testAbortTransaction(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- abortTransaction (discard pending changes) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x030405) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x030405) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createValueFile(VALUE_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 10000, 1000, false);
      ctx.check("createValueFile(init=1000) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(DATA_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 64);
      ctx.check("createBackupFile(size=64) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "\n  Test 1: Abort single credit");
      {
         int value = -1;
         card.getValue(VALUE_FILE_ID, COMM_PLAIN, value);
         ctx.check("initial value is 1000", value == 1000);
      }

      sw = card.credit(VALUE_FILE_ID, 500, COMM_PLAIN);
      ctx.check("credit(500) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.abortTransaction();
      ctx.check("abortTransaction returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         int value = -1;
         card.getValue(VALUE_FILE_ID, COMM_PLAIN, value);
         ctx.check("value after abort is still 1000 (credit discarded)", value == 1000);
      }

      LOG_INFO(ctx.log, "\n  Test 2: Abort multiple operations");
      sw = card.credit(VALUE_FILE_ID, 300, COMM_PLAIN);
      ctx.check("credit(300) returns STATUS_OK", sw == STATUS_OK);

      sw = card.debit(VALUE_FILE_ID, 150, COMM_PLAIN);
      ctx.check("debit(150) returns STATUS_OK", sw == STATUS_OK);

      sw = card.credit(VALUE_FILE_ID, 200, COMM_PLAIN);
      ctx.check("credit(200) returns STATUS_OK", sw == STATUS_OK);

      sw = card.abortTransaction();
      ctx.check("abortTransaction discards all ops, returns STATUS_OK", sw == STATUS_OK);

      {
         int value = -1;
         card.getValue(VALUE_FILE_ID, COMM_PLAIN, value);
         ctx.check("value after aborting multiple ops is 1000", value == 1000);
      }

      LOG_INFO(ctx.log, "\n  Test 3: Abort data write");

      rt::ByteBuffer initialData(16);
      initialData.fill(0x11, 16);
      initialData.flip();

      sw = card.writeData(DATA_FILE_ID, 0, 16, COMM_PLAIN, initialData);
      ctx.check("writeData(initial) returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction saves initial data", sw == STATUS_OK);

      rt::ByteBuffer readInitial(16);
      card.readData(DATA_FILE_ID, 0, 16, COMM_PLAIN, readInitial);
      ctx.check("readData confirms initial write (16 bytes)", readInitial == initialData);

      rt::ByteBuffer newData(16);
      newData.fill(0x22, 16);
      newData.flip();

      sw = card.writeData(DATA_FILE_ID, 0, 16, COMM_PLAIN, newData);
      ctx.check("writeData(new, to abort) returns STATUS_OK", sw == STATUS_OK);

      sw = card.abortTransaction();
      ctx.check("abortTransaction discards write, returns STATUS_OK", sw == STATUS_OK);

      rt::ByteBuffer readAfterAbort(16);
      card.readData(DATA_FILE_ID, 0, 16, COMM_PLAIN, readAfterAbort);
      ctx.check("data after abort reverted to initial (0x11)", readAfterAbort == initialData);

      LOG_INFO(ctx.log, "\n  Test 4: Double-abort — backup/data must not alias after rollback");

      rt::ByteBuffer thirdData(16);
      thirdData.fill(0x33, 16);
      thirdData.flip();

      sw = card.writeData(DATA_FILE_ID, 0, 16, COMM_PLAIN, thirdData);
      ctx.check("writeData(0x33, second abort) returns STATUS_OK", sw == STATUS_OK);

      sw = card.abortTransaction();
      ctx.check("second abortTransaction returns STATUS_OK", sw == STATUS_OK);

      rt::ByteBuffer readAfterSecondAbort(16);
      card.readData(DATA_FILE_ID, 0, 16, COMM_PLAIN, readAfterSecondAbort);
      ctx.check("data still 0x11 after second abort (no alias corruption)", readAfterSecondAbort == initialData);

      sw = card.deleteFile(VALUE_FILE_ID);
      ctx.check("deleteFile(value) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(DATA_FILE_ID);
      ctx.check("deleteFile(data) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
