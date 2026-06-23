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
#include "TestMFPRead.h"

using namespace hce::cards::mifareplus;

void testMFPRead(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP read (plain, single + multi-block) ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // Authenticate sector 0 with KeyA
   int sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("setup: authenticate(sector=0, KEY_A) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // --- Write known data to block 1 so we can verify the read ---
   auto writeData = rt::ByteBuffer::fromHex("0102030405060708090A0B0C0D0E0F10");
   sw = card.writeBlock(1, writeData);
   ctx.check("setup: writeBlock(1) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // --- Read single block ---
   rt::ByteBuffer data(16);
   sw = card.readBlock(1, 1, data);
   ctx.check("readBlock(1, count=1) returns STATUS_OK", sw == STATUS_OK);
   data.flip();
   ctx.check("readBlock(1) returns the written data", data == writeData);

   // --- Write data to blocks 1 and 2 for multi-block read ---
   auto data2 = rt::ByteBuffer::fromHex("AABBCCDDEEFF00112233445566778899");
   sw = card.writeBlock(2, data2);
   ctx.check("setup: writeBlock(2) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Re-authenticate before multi-block read (session may have progressed)
   sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("re-authenticate for multi-block read returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // --- Read 2 blocks ---
   rt::ByteBuffer twoBlocks(32);
   sw = card.readBlock(1, 2, twoBlocks);
   ctx.check("readBlock(1, count=2) returns STATUS_OK", sw == STATUS_OK);
   twoBlocks.flip();

   if (sw == STATUS_OK)
   {
      rt::ByteBuffer first = twoBlocks.slice(0, 16);
      rt::ByteBuffer second = twoBlocks.slice(16, 16);
      ctx.check("multi-block read: first block matches written data", first == writeData);
      ctx.check("multi-block read: second block matches written data", second == data2);
   }

   // --- Out of range block returns error ---
   sw = card.authenticate(0, KEY_A, zeroKey);
   rt::ByteBuffer dummy(16);
   sw = card.readBlock(255, 1, dummy);
   ctx.check("readBlock on out-of-range address returns error", sw != STATUS_OK);
}
