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

#ifndef HCE_CIPHER_H
#define HCE_CIPHER_H

#include <rt/ByteBuffer.h>

namespace hce::crypto {

class Cipher
{
   public:

      virtual ~Cipher() = default;

      virtual void init(const rt::ByteBuffer &key, int mode) = 0;

      virtual rt::ByteBuffer encrypt(const rt::ByteBuffer &input) = 0;

      virtual rt::ByteBuffer decrypt(const rt::ByteBuffer &input) = 0;

      virtual rt::ByteBuffer encrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv) = 0;

      virtual rt::ByteBuffer decrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv) = 0;
};

}

#endif //HCE_CIPHER_H
