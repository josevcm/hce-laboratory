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

#pragma once

#include <hce/cards/desfire/Desfire.h>
#include <TestContext.h>

// Resets the card to factory-virgin state.
// Tries Legacy (DES) auth first, then AES. Works for EV1 (DES) and EV2+ (AES) cards.
// After format: only master app remains, default master key, no user apps or files.
void testFormatCard(hce::cards::desfire::Desfire &card, TestContext &ctx);
