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

#include "TestChangeKey.h"

using namespace hce::cards::desfire;

// 2K3DES test app: keySettings2 = 0x01 (2K3DES mode, 1 key)
static constexpr unsigned int TEST_APP_ID = 0x0A0B0C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

// Default 2K3DES key: 16-byte zeros

static constexpr const char *KEY_2K3DES_ZEROS = "00000000000000000000000000000000";
// New key to change to: 16-byte 0x01
static constexpr const char *KEY_2K3DES_NEW = "01010101010101010101010101010101";

void testChangeKey(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- changeKey ---");

   // --- Setup: create test app ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0A0B0C, 2K3DES) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      auto zeroKey = rt::ByteBuffer::fromHex(KEY_2K3DES_ZEROS);
      auto newKey = rt::ByteBuffer::fromHex(KEY_2K3DES_NEW);

      // --- Select test app and authenticate with default key ---
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0A0B0C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Change key 0 to new value (same key = changing the auth key) ---
      sw = card.changeKey(0, newKey, zeroKey);
      ctx.check("changeKey(0, newKey) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Verify: re-select and re-auth with new key ---
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(TEST_APP_ID) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, newKey);
      ctx.check("authenticateLegacy with new key returns STATUS_OK", sw == STATUS_OK);
      ctx.check("authMode is AUTH_LEGACY after re-auth", card.authMode() == AUTH_LEGACY);
      if (sw != STATUS_OK) return;

      // --- Restore: change key back to zeros ---
      sw = card.changeKey(0, zeroKey, newKey);
      ctx.check("changeKey(0, zeros) restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Verify restore ---
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(TEST_APP_ID) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy with zeros after restore returns STATUS_OK", sw == STATUS_OK);
   }();

   // --- Teardown: delete test app ---
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
