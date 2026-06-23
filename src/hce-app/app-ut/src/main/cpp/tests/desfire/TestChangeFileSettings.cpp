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

#include "TestChangeFileSettings.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0D0E0F;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;
static constexpr unsigned int FILE_SIZE      = 32;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testChangeFileSettings(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- changeFileSettings ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0D0E0F) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0D0E0F) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create standard file with full free access, plain comm
      sw = card.createStandardFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(id=0, plain, free) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify original settings
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings (before change) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("original commMode is COMM_PLAIN", fs.commSettings == COMM_PLAIN);
         LOG_INFO(ctx.log, "  before: commMode={} accessRights=0x{04x}", {fs.commSettings, fs.accessRights});
      }

      // Change comm mode to COMM_MACING, restrict write to key 0
      sw = card.changeFileSettings(FILE_ID, COMM_MACING, 0xE, 0x0, 0xE, 0xE);
      LOG_INFO(ctx.log, "  changeFileSettings sw: 0x{04x}", {sw});
      ctx.check("changeFileSettings(macing, write=key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify updated settings
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings (after change) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("commMode changed to COMM_MACING", fs.commSettings == COMM_MACING);
         LOG_INFO(ctx.log, "  after:  commMode={} accessRights=0x{04x}", {fs.commSettings, fs.accessRights});
      }

      // Restore original settings (plain, full free access)
      sw = card.changeFileSettings(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE);
      ctx.check("changeFileSettings (restore plain, free) returns STATUS_OK", sw == STATUS_OK);

      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("commMode restored to COMM_PLAIN", fs.commSettings == COMM_PLAIN);
      }

      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile(0) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
