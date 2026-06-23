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

#ifndef HCE_DESFIRE_KEYENTRY_H
#define HCE_DESFIRE_KEYENTRY_H

#include <rt/ByteBuffer.h>

namespace hce::targets::desfire {

enum KeyType
{
   KeyType2K3DES = 0x00,
   KeyType3K3DES = 0x01,
   KeyTypeAES = 0x02,
};

struct KeyEntry
{
   unsigned int id;
   unsigned int type;
   unsigned int version;
   rt::ByteBuffer key;

   bool is3DES() const
   {
      return type == KeyType2K3DES || type == KeyType3K3DES;
   }

   bool isAES() const
   {
      return type == KeyTypeAES;
   }
};

}

#endif
