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

#include "TestValueFileLimits.h"

using namespace hce::cards::desfire;

static constexpr unsigned int VFL_APP_ID = 0x1FEEEE;
static constexpr unsigned int VFL_KS1 = 0x0F;
static constexpr unsigned int VFL_KS2 = 0x01; // DES/2K3DES, 1 key

void testValueFileLimits(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- valueFileLimits: CreateValueFile range validation ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex("00000000000000000000000000000000");
   int sw = 0;

   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(VFL_APP_ID, VFL_KS1, VFL_KS2);
   ctx.check("setup: createApplication returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(VFL_APP_ID);
      ctx.check("selectApplication(testApp) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Test A: upperLimit < lowerLimit -> PARAMETER_ERROR
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 100, 50, 75, false);
      ctx.check("createValueFile(upper < lower) returns PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth after invalid create A returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Test B: initialValue < lowerLimit -> PARAMETER_ERROR
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 1000, -1, false);
      ctx.check("createValueFile(initial < lower) returns PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth after invalid create B returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Test C: initialValue > upperLimit -> PARAMETER_ERROR
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 100, 200, false);
      ctx.check("createValueFile(initial > upper) returns PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth before valid create returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Test D: valid params -> STATUS_OK
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 1000, 500, false);
      ctx.check("createValueFile(valid: 0..1000 init=500) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth before getValue returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      int value = -1;
      sw = card.getValue(0x00, COMM_PLAIN, value);
      ctx.check("getValue after createValueFile returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getValue returns configured initial value 500", value == 500);

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth for edge case returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Test E: lowerLimit == upperLimit == initialValue -> STATUS_OK
      sw = card.createValueFile(0x01, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 42, 42, 42, false);
      ctx.check("createValueFile(lower==upper==initial==42) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-auth before getValue(edge) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      int edge = -1;
      sw = card.getValue(0x01, COMM_PLAIN, edge);
      ctx.check("getValue(boundary edge) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getValue(boundary edge) returns 42", edge == 42);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}


