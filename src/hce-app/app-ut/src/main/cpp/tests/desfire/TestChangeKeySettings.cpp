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

#include "TestChangeKeySettings.h"

using namespace hce::cards::desfire;

void testChangeKeySettings(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- changeKeySettings ---");

   // Select master app and authenticate with Legacy DES
   int sw = card.selectApplication(0x000000);
   ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   const auto keyDes = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   sw = card.authenticateLegacy(0, keyDes);
   ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Read original settings
   unsigned int origSettings = 0, numKeys = 0;
   sw = card.getKeySettings(origSettings, numKeys);
   ctx.check("getKeySettings (baseline) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   LOG_INFO(ctx.log, "  Original keySettings: 0x{02x}", {origSettings});

   // Change to a different value (preserve bit 0 = allow config change)
   // Toggle bit 1 (free access to master key) while keeping others
   unsigned int newSettings = origSettings ^ 0x02;
   sw = card.changeKeySettings(newSettings);
   ctx.check("changeKeySettings returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return; // settings unchanged: nothing to restore

   [&] {
      // Re-authenticate to verify
      sw = card.selectApplication(0x000000);
      ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, keyDes);
      ctx.check("re-auth after changeKeySettings returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      unsigned int readSettings = 0;
      sw = card.getKeySettings(readSettings, numKeys);
      ctx.check("getKeySettings after change returns STATUS_OK", sw == STATUS_OK);
      ctx.check("keySettings changed to expected value", readSettings == newSettings);
      LOG_INFO(ctx.log, "  New keySettings: 0x{02x}", {readSettings});
   }();

   // Teardown: always restore original settings
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, keyDes);
   sw = card.changeKeySettings(origSettings);
   ctx.check("changeKeySettings (restore) returns STATUS_OK", sw == STATUS_OK);

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, keyDes);
   unsigned int readSettings = 0;
   sw = card.getKeySettings(readSettings, numKeys);
   ctx.check("keySettings restored to original", readSettings == origSettings);
   LOG_INFO(ctx.log, "  Restored keySettings: 0x{02x}", {readSettings});
}
