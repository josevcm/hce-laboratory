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

// Transaction: CMAC on plain responses with AES session
// Spec §7.3.1: in ISO/AES sessions even "plain" responses must carry an 8-byte CMAC trailer.
// On a real card: all steps pass.
// On the emulator (unfixed): readData / getFileSettings fail because the PICC omits
// the CMAC and the client-side MAC validation returns STATUS_INTEGRITY_ERROR.
void testCmacPlain(hce::cards::desfire::Desfire &card, TestContext &ctx);


