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

#include "TestAuthenticate.h"

using namespace hce::cards::desfire;

void testAuthenticate(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- authenticateLegacy ---");

   card.selectApplication(0x000000);

   auto key = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   int sw = card.authenticateLegacy(0, key);
   ctx.check("authenticateLegacy(key0, all-zeros) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("authMode is AUTH_LEGACY", card.authMode() == AUTH_LEGACY);
   ctx.check("authKeyId is 0", card.authKeyId() == 0);
}
