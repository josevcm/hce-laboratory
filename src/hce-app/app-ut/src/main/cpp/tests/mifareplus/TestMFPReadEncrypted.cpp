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
#include "TestMFPReadEncrypted.h"

using namespace hce::cards::mifareplus;

void testMFPReadEncrypted(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP readEncrypted ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // Authenticate and write known data
   int sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("setup: authenticate(sector=0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   auto expected = rt::ByteBuffer::fromHex("DEADBEEFCAFE0102030405060708090A");
   sw = card.writeBlock(1, expected);
   ctx.check("setup: writeBlock(1) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Re-authenticate to get fresh session before encrypted read
   sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("re-authenticate returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // --- Read encrypted: decrypted data must match original ---
   rt::ByteBuffer data(16);
   sw = card.readBlockEncrypted(1, 1, data);
   ctx.check("readBlockEncrypted(1, count=1) returns STATUS_OK", sw == STATUS_OK);
   data.flip();
   ctx.check("readBlockEncrypted returns the original plaintext", data == expected);

   // --- Multi-block encrypted read ---
   sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("re-authenticate for multi-block encrypted read returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   auto expected2 = rt::ByteBuffer::fromHex("1122334455667788AABBCCDDEEFF0099");
   sw = card.writeBlock(2, expected2);
   ctx.check("setup: writeBlock(2) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("re-authenticate returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   rt::ByteBuffer twoBlocks(32);
   sw = card.readBlockEncrypted(1, 2, twoBlocks);
   ctx.check("readBlockEncrypted(1, count=2) returns STATUS_OK", sw == STATUS_OK);
   twoBlocks.flip();

   if (sw == STATUS_OK)
   {
      rt::ByteBuffer first = twoBlocks.slice(0, 16);
      rt::ByteBuffer second = twoBlocks.slice(16, 16);
      ctx.check("encrypted multi-block read: block 1 data matches", first == expected);
      ctx.check("encrypted multi-block read: block 2 data matches", second == expected2);
   }
}
