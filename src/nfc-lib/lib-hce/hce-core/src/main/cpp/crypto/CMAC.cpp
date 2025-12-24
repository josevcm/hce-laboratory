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

#include <hce/crypto/CipherAES.h>
#include <hce/crypto/CipherDES.h>

#include <hce/crypto/CMAC.h>

namespace hce {

rt::ByteBuffer CMAC::cmac(const rt::ByteBuffer &key, const rt::ByteBuffer &input, const rt::ByteBuffer &iv, Mode mode)
{
   assert(mode == CmacAES128 || mode == CmacAES128Trunc || mode == CmacTDES);

   static CipherAES cipherAES;
   static CipherDES cipherDES;

   int size = 0;
   int mask = 0;
   Cipher *cipher = nullptr;

   switch (mode)
   {
      case CmacAES128:
      case CmacAES128Trunc:
      {
         size = 16;
         mask = 0x87;
         cipher = &cipherAES;
         break;
      }
      case CmacTDES:
      {
         size = 8;
         mask = 0x1b;
         cipher = &cipherDES;
         break;
      }
      default:
         assert(false);
   }

   // initialize cipher with key and default Mode
   cipher->init(key, 0);

   /*
    * sub k0, k1, k2 generation
    */
   rt::ByteBuffer k0 = cipher->encrypt(rt::ByteBuffer::zero(size));

   // K1
   rt::ByteBuffer k1 = rt::ByteBuffer::shiftBits(k0, rt::ByteBuffer::Left);

   if ((k0[0] & 0x80) != 0)
      k1[size - 1] ^= mask;

   // K2
   rt::ByteBuffer k2 = rt::ByteBuffer::shiftBits(k1, rt::ByteBuffer::Left);

   if ((k1[0] & 0x80) != 0)
      k2[size - 1] ^= mask;

   /*
    * Prepare input data, padding an XOR last block
    */
   unsigned int padding = input.remaining() % size == 0 ? 0 : size - input.remaining() % size;

   rt::ByteBuffer data = rt::ByteBuffer::zero(input.remaining() + padding);

   // add input data
   data.put(input);

   if (padding > 0)
   {
      data.put(0x80).push(padding - 1, true);

      for (int i = 0; i < size; i++)
         data[data.size() - size + i] ^= k2[i];
   }
   else
   {
      for (int i = 0; i < size; i++)
         data[data.size() - size + i] ^= k1[i];
   }

   data.flip();

   rt::ByteBuffer tmpIv = iv;
   rt::ByteBuffer crypt = cipher->encrypt(data, tmpIv);

   // CMAC is last block of crypted data
   crypt = crypt.slice(crypt.remaining() - size, size);

   // for truncated mode, return only left half of CMAC
   if (mode == CmacAES128Trunc)
   {
      rt::ByteBuffer result(size / 2);

      for (int i = 0; i < result.capacity(); ++i)
         result[i] = crypt[i * 2 + 1];

      return result;
   }

   return crypt;
}

}
