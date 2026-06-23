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
#include "TestMFPWriteEncrypted.h"

using namespace hce::cards::mifareplus;

void testMFPWriteEncrypted(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP writeEncrypted ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // --- Write encrypted and read back plain ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate(sector=0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto expected = rt::ByteBuffer::fromHex("CAFEBABE0102030405060708090A0B0C");
      sw = card.writeBlockEncrypted(1, expected);
      ctx.check("writeBlockEncrypted(1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-authenticate and read back plain
      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate before plain read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer readBack(16);
      sw = card.readBlock(1, 1, readBack);
      ctx.check("plain read after encrypted write returns STATUS_OK", sw == STATUS_OK);
      readBack.flip();
      ctx.check("data written encrypted matches plain read-back", readBack == expected);
   }();

   // --- Write encrypted and read back encrypted ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate for encrypted read-back returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto expected = rt::ByteBuffer::fromHex("DEADC0DEAABBCCDDEEFF001122334455");
      sw = card.writeBlockEncrypted(2, expected);
      ctx.check("writeBlockEncrypted(2) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate for encrypted read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer readBack(16);
      sw = card.readBlockEncrypted(2, 1, readBack);
      ctx.check("readBlockEncrypted after writeBlockEncrypted returns STATUS_OK", sw == STATUS_OK);
      readBack.flip();
      ctx.check("encrypted write + encrypted read yields original plaintext", readBack == expected);
   }();

   // Restore auth state
   card.authenticate(0, KEY_A, zeroKey);
}
