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

#include "TestAuthenticationErrors.h"

using namespace hce::cards::desfire;

static constexpr unsigned int AES_APP_ID    = 0x1B1C1D;
static constexpr unsigned int K3DES_APP_ID  = 0x1C1D1E;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int AES_KS2       = 0x81; // AES, 1 key
static constexpr unsigned int K3DES_KS2     = 0x41; // 3K3DES, 1 key

static constexpr const char *WRONG_MASTER_KEY = "0102030405060708";
static constexpr const char *AES_KEY_ZEROS    = "00000000000000000000000000000000";
static constexpr const char *WRONG_AES_KEY    = "01020304050607080910111213141516";
static constexpr const char *K3DES_KEY_ZEROS  = "000000000000000000000000000000000000000000000000";

void testAuthenticationErrors(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- authenticationErrors ---");

   auto masterKey    = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto wrongMaster  = rt::ByteBuffer::fromHex(WRONG_MASTER_KEY);
   auto aesKey       = rt::ByteBuffer::fromHex(AES_KEY_ZEROS);
   auto wrongAesKey  = rt::ByteBuffer::fromHex(WRONG_AES_KEY);
   auto k3desKey     = rt::ByteBuffer::fromHex(K3DES_KEY_ZEROS);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(AES_APP_ID, KEY_SETTINGS_1, AES_KS2);
   ctx.check("setup: createApplication(AES, 0x1B1C1D) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(K3DES_APP_ID, KEY_SETTINGS_1, K3DES_KS2);
   ctx.check("setup: createApplication(3K3DES, 0x1C1D1E) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      LOG_INFO(ctx.log, "  Test A: wrong key returns AUTHENTICATION_ERROR");

      // A1: wrong DES key on PICC master
      sw = card.selectApplication(0x000000);
      ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, wrongMaster);
      ctx.check("authenticateLegacy(master, wrong key) returns STATUS_AUTHENTICATION_ERROR", sw == STATUS_AUTHENTICATION_ERROR);
      ctx.check("authMode is AUTH_NONE after failed auth", card.authMode() == AUTH_NONE);

      // A2: wrong AES key on AES app
      sw = card.selectApplication(AES_APP_ID);
      ctx.check("selectApplication(AES app) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, wrongAesKey);
      ctx.check("authenticateAES(AES app, wrong key) returns STATUS_AUTHENTICATION_ERROR", sw == STATUS_AUTHENTICATION_ERROR);
      ctx.check("authMode is AUTH_NONE after failed AES auth", card.authMode() == AUTH_NONE);

      LOG_INFO(ctx.log, "  Test B: auth invalidated by selectApplication");

      sw = card.selectApplication(0x000000);
      ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, masterKey);
      ctx.check("authenticateLegacy(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      ctx.check("authMode is AUTH_LEGACY before selectApplication", card.authMode() == AUTH_LEGACY);

      sw = card.selectApplication(AES_APP_ID);
      ctx.check("selectApplication(AES app) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("authMode is AUTH_NONE after selectApplication", card.authMode() == AUTH_NONE);
      ctx.check("authKeyId is -1 after selectApplication", card.authKeyId() == -1);

      LOG_INFO(ctx.log, "  Test C: authenticateLegacy rejected for 3K3DES app");

      sw = card.selectApplication(K3DES_APP_ID);
      ctx.check("selectApplication(3K3DES app) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // authenticateLegacy must be rejected (only authenticateISO is valid for 3K3DES)
      sw = card.authenticateLegacy(0, k3desKey);
      ctx.check("authenticateLegacy on 3K3DES app returns ILLEGAL_COMMAND or AUTH_ERROR",
                sw == STATUS_ILLEGAL_COMMAND || sw == STATUS_AUTHENTICATION_ERROR);

      // authenticateISO must succeed for 3K3DES
      sw = card.authenticateISO(0, k3desKey);
      ctx.check("authenticateISO on 3K3DES app returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
