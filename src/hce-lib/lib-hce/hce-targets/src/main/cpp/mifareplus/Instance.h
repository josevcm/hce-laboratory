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

#ifndef HCE_MIFAREPLUS_INSTANCE_H
#define HCE_MIFAREPLUS_INSTANCE_H

#include <rt/Logger.h>

namespace hce::targets {

struct Instance
{
   rt::Logger *log = rt::Logger::getLogger("hce.mifareplus.Instance");

   // UID
   unsigned char uid[7] {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
};

}

#endif //HCE_MIFAREPLUS_INSTANCE_H
