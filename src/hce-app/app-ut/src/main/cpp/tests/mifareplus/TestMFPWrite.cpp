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
#include "TestMFPWrite.h"

using namespace hce::cards::mifareplus;

void testMFPWrite(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP write (plain, with MAC) ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // --- Write block 1 and read back ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate(sector=0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto writeData = rt::ByteBuffer::fromHex("0102030405060708090A0B0C0D0E0F10");
      sw = card.writeBlock(1, writeData);
      ctx.check("writeBlock(1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-authenticate and read back
      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer readBack(16);
      sw = card.readBlock(1, 1, readBack);
      ctx.check("readBlock after write returns STATUS_OK", sw == STATUS_OK);
      readBack.flip();
      ctx.check("read-back data matches written data", readBack == writeData);
   }();

   // --- Write to block 0 (manufacturer block) is rejected ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate for block0 test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto data = rt::ByteBuffer::zero(16);
      sw = card.writeBlock(0, data);
      ctx.check("writeBlock(0) to manufacturer block returns error", sw != STATUS_OK);
   }();

   // --- Write outside authenticated sector fails ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate sector 0 for boundary test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto data = rt::ByteBuffer::zero(16);
      sw = card.writeBlock(4, data);
      ctx.check("writeBlock to unauthenticated sector returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);
   }();

   // Restore clean auth state
   card.authenticate(0, KEY_A, zeroKey);
}
