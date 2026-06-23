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

#include "TestAuthenticateAES.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0A0B0C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x81; // AES key type, 1 key

static constexpr const char *APP_KEY_AES_ZEROS = "00000000000000000000000000000000";

void testAuthenticateAES(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- authenticateAES ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto aesKey    = rt::ByteBuffer::fromHex(APP_KEY_AES_ZEROS);

   // Setup: select master, DES auth, create AES application
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(AES, 0x0A0B0C) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0A0B0C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("authenticateAES(key0, all-zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      ctx.check("authMode is AUTH_AES",  card.authMode() == AUTH_AES);
      ctx.check("authKeyId is 0",        card.authKeyId() == 0);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
