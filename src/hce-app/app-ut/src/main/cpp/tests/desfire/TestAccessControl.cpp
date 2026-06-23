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

#include "TestAccessControl.h"

using namespace hce::cards::desfire;

static constexpr unsigned int CTRL_APP_ID = 0x2A2B2C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x02; // DES/2K3DES, 2 keys
static constexpr unsigned int FILE_R = 0x00; // read-restricted file
static constexpr unsigned int FILE_W = 0x01; // write-restricted file
static constexpr unsigned int FILE_SIZE = 16;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";
static constexpr const char *APP_KEY_NEW = "01010101010101010101010101010101";
static constexpr const char *APP_KEY_WRONG = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

void testAccessControl(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- accessControl ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto zeroKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
   auto newKey = rt::ByteBuffer::fromHex(APP_KEY_NEW);
   auto wrongKey = rt::ByteBuffer::fromHex(APP_KEY_WRONG);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Idempotent setup for physical cards: remove leftovers from previous failed runs.
   sw = card.deleteApplication(CTRL_APP_ID);
   ctx.check("setup: deleteApplication(0x2A2B2C) returns STATUS_OK or STATUS_APPLICATION_NOT_FOUND",
             sw == STATUS_OK || sw == STATUS_APPLICATION_NOT_FOUND);

   [&] {
      sw = card.createApplication(CTRL_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
      ctx.check("setup: createApplication(2-key, 0x2A2B2C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.selectApplication(CTRL_APP_ID);
      ctx.check("selectApplication(0x2A2B2C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // FILE_R: readKey=0 (restricted), writeKey=0xE (free), readWriteKey=0xF (never)
      // Important: readWriteKey also grants read/write, so it must not be 0 here.
      sw = card.createStandardFile(FILE_R, COMM_PLAIN, 0x0, 0xE, 0xF, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(readKey=0, readWriteKey=0xF) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // FILE_W: readKey=0xE (free), writeKey=0 (restricted), readWriteKey=0xF (never)
      // Important: readWriteKey also grants read/write, so it must not be 0 here.
      sw = card.createStandardFile(FILE_W, COMM_PLAIN, 0xE, 0x0, 0xF, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(writeKey=0, readWriteKey=0xF) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: readData without auth returns error");

      // Deselect by selecting master, then re-select app without authenticating
      sw = card.selectApplication(0x000000);
      ctx.check("selectApplication(master) for deauth returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.selectApplication(CTRL_APP_ID);
      ctx.check("selectApplication(0x2A2B2C) unauthenticated returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer rbuf(FILE_SIZE);
      sw = card.readData(FILE_R, 0, FILE_SIZE, COMM_PLAIN, rbuf);
      ctx.check("readData(restricted) without auth returns AUTH_ERROR or PERMISSION_DENIED", sw == STATUS_AUTHENTICATION_ERROR || sw == STATUS_PERMISSION_DENIED);

      LOG_INFO(ctx.log, "  Test B: readData with correct key succeeds");

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer rbuf2(FILE_SIZE);
      sw = card.readData(FILE_R, 0, FILE_SIZE, COMM_PLAIN, rbuf2);
      ctx.check("readData(restricted) with key0 returns STATUS_OK", sw == STATUS_OK);

      LOG_INFO(ctx.log, "  Test C: changeKey(key1) with wrong oldKey returns error");

      // Still authenticated as key0 — changing key1 requires correct old key material.
      sw = card.changeKey(1, newKey, wrongKey);
      ctx.check("changeKey(1, newKey, wrongOldKey) returns AUTH_ERROR/INTEGRITY/PERMISSION",
                sw == STATUS_AUTHENTICATION_ERROR || sw == STATUS_INTEGRITY_ERROR || sw == STATUS_PERMISSION_DENIED);

      // Verify key0 was not changed (re-auth with original key must succeed)
      sw = card.selectApplication(CTRL_APP_ID);
      ctx.check("re-selectApplication after failed changeKey returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0, original) still returns STATUS_OK after failed changeKey",
                sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
