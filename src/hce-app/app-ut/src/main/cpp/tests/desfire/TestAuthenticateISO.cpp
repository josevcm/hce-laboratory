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

#include "TestAuthenticateISO.h"

using namespace hce::cards::desfire;

void testAuthenticateISO(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- authenticateISO ---");

   int sw = card.selectApplication(0x000000);
   ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // DES master key (8-byte zeros) — EV1 card default
   auto keyDes = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   sw = card.authenticateISO(0, keyDes);
   ctx.check("authenticateISO(key0, DES-zeros) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("authMode is AUTH_ISO", card.authMode() == AUTH_ISO);
   ctx.check("authKeyId is 0", card.authKeyId() == 0);
}
