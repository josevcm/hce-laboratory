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
#include "TestMFPGetUID.h"

using namespace hce::cards::mifareplus;

void testMFPGetUID(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP getUID ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // Authenticate first
   int sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("authenticate(sector=0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Get UID — server decrypts response before returning
   rt::ByteBuffer uid(16);
   sw = card.getUID(uid);
   ctx.check("getUID() returns STATUS_OK", sw == STATUS_OK);

   if (sw == STATUS_OK)
   {
      uid.flip();
      // Server pads the 7-byte UID to 16 bytes before encrypting; client
      // decrypts and returns the full 16-byte plaintext block.
      ctx.check("getUID() returns 16-byte decrypted block", uid.remaining() == 16);
      ctx.check("UID block is non-zero (first bytes are UID)", uid != rt::ByteBuffer::zero(16));
   }

   // --- getUID without authentication returns error ---
   card.resetAuthentication();

   rt::ByteBuffer uid2(16);
   sw = card.getUID(uid2);
   ctx.check("getUID without authentication returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);

   // Restore auth
   card.authenticate(0, KEY_A, zeroKey);
}
