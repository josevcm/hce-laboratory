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

#ifndef HCE_CRC_H
#define HCE_CRC_H

#include <rt/ByteBuffer.h>

namespace hce {

struct CRC
{
   static unsigned short iso14443A(const rt::ByteBuffer &data);

   static unsigned  short iso14443A(const rt::ByteBuffer &data, unsigned int length);

   static unsigned short ccitt16(const rt::ByteBuffer &data, unsigned int length, unsigned short init, bool revin);

   static unsigned short ccitt16(const unsigned char *data, unsigned int from, unsigned int to, unsigned short init, bool revin);

   static unsigned int ccitt32(const rt::ByteBuffer &data);

   static unsigned int ccitt32(const rt::ByteBuffer &data, unsigned int length);

   static unsigned int ccitt32(const rt::ByteBuffer &data, unsigned int length, unsigned int init);

   static unsigned int ccitt32(const unsigned char *data, unsigned int from, unsigned int to, unsigned int init);
};

}
#endif //HCE_CRC_H
