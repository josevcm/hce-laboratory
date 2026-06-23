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
#include "TestMFPMultiSector.h"

using namespace hce::cards::mifareplus;

void testMFPMultiSector(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP multi-sector (following auth + cross-sector access) ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // --- Write distinct data to sector 0 and sector 1 ---
   auto dataSec0 = rt::ByteBuffer::fromHex("AABBCCDDEEFF00112233445566778899");
   auto dataSec1 = rt::ByteBuffer::fromHex("0102030405060708090A0B0C0D0E0F10");

   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate sector 0 returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.writeBlock(1, dataSec0);
      ctx.check("setup: write block 1 (sector 0) returns STATUS_OK", sw == STATUS_OK);

      sw = card.authenticateFollowing(1, KEY_A, zeroKey);
      ctx.check("authenticateFollowing(sector=1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.writeBlock(4, dataSec1);
      ctx.check("setup: write block 4 (sector 1) returns STATUS_OK", sw == STATUS_OK);
   }();

   // --- Read sector 0, then move to sector 1 using following auth ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate sector 0 for read test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer readSec0(16);
      sw = card.readBlock(1, 1, readSec0);
      ctx.check("readBlock(1) in sector 0 returns STATUS_OK", sw == STATUS_OK);
      readSec0.flip();
      ctx.check("sector 0 block 1 data matches", readSec0 == dataSec0);

      sw = card.authenticateFollowing(1, KEY_A, zeroKey);
      ctx.check("authenticateFollowing sector 1 returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      ctx.check("authSector() is 1 after following auth", card.authSector() == 1);

      rt::ByteBuffer readSec1(16);
      sw = card.readBlock(4, 1, readSec1);
      ctx.check("readBlock(4) in sector 1 returns STATUS_OK", sw == STATUS_OK);
      readSec1.flip();
      ctx.check("sector 1 block 4 data matches", readSec1 == dataSec1);
   }();

   // --- Accessing sector 0 after following auth to sector 1 is rejected ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate sector 0 for cross-sector test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateFollowing(1, KEY_A, zeroKey);
      ctx.check("following auth to sector 1 returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer dummy(16);
      sw = card.readBlock(1, 1, dummy);
      ctx.check("read from sector 0 after auth to sector 1 returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);
   }();

   // Restore auth
   card.authenticate(0, KEY_A, zeroKey);
}
