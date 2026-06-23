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

// Security: ISO authentication with 2K3DES and 3K3DES session key construction
// Tests that authenticateISO correctly constructs the session key for both DES
// and 3K3DES applications, enabling secure file operations on ISO-enabled cards.
void testSessionKey2K3DES(hce::cards::desfire::Desfire &card, TestContext &ctx);


