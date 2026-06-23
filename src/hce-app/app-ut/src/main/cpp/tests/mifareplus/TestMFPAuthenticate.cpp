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
#include "TestMFPAuthenticate.h"

using namespace hce::cards::mifareplus;

void testMFPAuthenticate(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP authenticate (first + following) ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // --- First authentication on sector 0 with KeyA ---
   int sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("authenticate(sector=0, KEY_A, zeros) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("isAuthenticated() is true after auth", card.isAuthenticated());
   ctx.check("authSector() is 0 after auth on sector 0", card.authSector() == 0);

   // --- First authentication on sector 0 with KeyB ---
   sw = card.authenticate(0, KEY_B, zeroKey);
   ctx.check("authenticate(sector=0, KEY_B, zeros) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("isAuthenticated() is true after KeyB auth", card.isAuthenticated());

   // --- Following authentication on sector 1 after existing session ---
   sw = card.authenticate(0, KEY_A, zeroKey);
   ctx.check("re-authenticate sector 0 returns STATUS_OK", sw == STATUS_OK);

   sw = card.authenticateFollowing(1, KEY_A, zeroKey);
   ctx.check("authenticateFollowing(sector=1, KEY_A, zeros) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("authSector() updated to 1 after following auth", card.authSector() == 1);

   // --- Reset authentication ---
   sw = card.resetAuthentication();
   ctx.check("resetAuthentication() returns STATUS_OK", sw == STATUS_OK);
   ctx.check("isAuthenticated() is false after reset", !card.isAuthenticated());

   // --- Following auth without prior session fails ---
   sw = card.authenticateFollowing(0, KEY_A, zeroKey);
   ctx.check("authenticateFollowing without prior auth returns STATUS_ERR_NOTAUTH", sw == STATUS_ERR_NOTAUTH);
}
