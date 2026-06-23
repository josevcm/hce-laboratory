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
#include "TestMFPAuthErrors.h"

using namespace hce::cards::mifareplus;

void testMFPAuthErrors(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP authentication errors ---");

   auto zeroKey = rt::ByteBuffer::zero(16);
   auto wrongKey = rt::ByteBuffer::fromHex("AABBCCDDEEFF00112233445566778899");

   // --- Wrong key rejected ---
   int sw = card.authenticate(0, KEY_A, wrongKey);
   ctx.check("authenticate with wrong key returns STATUS_ERR_AUTH", sw == STATUS_ERR_AUTH);
   ctx.check("isAuthenticated() is false after failed auth", !card.isAuthenticated());

   // --- Invalid sector rejected ---
   sw = card.authenticate(99, KEY_A, zeroKey);
   ctx.check("authenticate on out-of-range sector returns error", sw != STATUS_OK);

   // --- Read without authentication returns error ---
   rt::ByteBuffer data(16);
   sw = card.readBlock(1, 1, data);
   ctx.check("readBlock without auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   // --- Write without authentication returns error ---
   auto blockData = rt::ByteBuffer::zero(16);
   sw = card.writeBlock(1, blockData);
   ctx.check("writeBlock without auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   // --- Value ops without authentication return error ---
   sw = card.increment(1, 10);
   ctx.check("increment without auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   sw = card.restore(1);
   ctx.check("restore without auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   sw = card.transfer(1);
   ctx.check("transfer without auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   // --- Restore state ---
   card.authenticate(0, KEY_A, zeroKey);
}
