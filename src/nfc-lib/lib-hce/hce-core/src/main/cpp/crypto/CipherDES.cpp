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

#include <cassert>

#include <cppdes/des.h>
#include <cppdes/des3.h>

#include <hce/crypto/CipherDES.h>

namespace hce {

CipherDES::CipherDES()
{
}

void CipherDES::init(const rt::ByteBuffer &key, int mode)
{
   assert(key.size() == 8 || key.size() == 16 || key.size() == 24);

   ui64 k1 = 0;
   ui64 k2 = 0;
   ui64 k3 = 0;

   switch (rt::ByteBuffer tmp = key; tmp.remaining())
   {
      case 8:
         k1 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         k2 = k1;
         k3 = k1;
         break;
      case 16:
         k1 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         k2 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         k3 = k1;
         break;
      case 24:
         k1 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         k2 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         k3 = tmp.getLong(8, rt::ByteBuffer::BigEndian);
         break;
      default:
         break;
   }

   this->mode = mode;
   this->des3 = std::make_shared<DES3>(k1, k2, k3);
}

rt::ByteBuffer CipherDES::encrypt(const rt::ByteBuffer &input)
{
   rt::ByteBuffer iv = rt::ByteBuffer::zero(8);
   return encrypt(input, iv);
}

rt::ByteBuffer CipherDES::decrypt(const rt::ByteBuffer &input)
{
   rt::ByteBuffer iv = rt::ByteBuffer::zero(8);
   return decrypt(input, iv);
}

rt::ByteBuffer CipherDES::encrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv)
{
   assert(input.remaining() % 8 == 0);
   assert(iv.size() == 8);

   ui64 iv64 = iv.getLong(8, rt::ByteBuffer::BigEndian);

   rt::ByteBuffer tmp = input;
   rt::ByteBuffer output = rt::ByteBuffer(input.remaining());

   while (tmp.remaining() >= 8)
   {
      ui64 block = tmp.getLong(8, rt::ByteBuffer::BigEndian);

      // encrypt block with CBC send mode
      ui64 crypt = des3->encrypt(block ^ iv64);

      // store encrypted block
      output.putLong(crypt, 8, rt::ByteBuffer::BigEndian);

      // update IV with last crypt block
      iv64 = crypt;
   }

   // update IV
   iv.clear();
   iv.putLong(iv64, 8, rt::ByteBuffer::BigEndian);
   iv.flip();

   output.flip();

   return output;
}

rt::ByteBuffer CipherDES::decrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv)
{
   assert(input.remaining() % 8 == 0);
   assert(iv.size() == 8);

   ui64 iv64 = iv.getLong(8, rt::ByteBuffer::BigEndian);

   rt::ByteBuffer tmp = input;
   rt::ByteBuffer output = rt::ByteBuffer(input.remaining());

   while (tmp.remaining() >= 8)
   {
      ui64 value;
      ui64 block = tmp.getLong(8, rt::ByteBuffer::BigEndian);

      // encrypt block with CBC send mode
      if (mode == Legacy)
         value = des3->encrypt(block) ^ iv64;
      else
         value = des3->decrypt(block) ^ iv64;

      // store encrypted block
      output.putLong(value, 8, rt::ByteBuffer::BigEndian);

      // update IV with current input block
      iv64 = block;
   }

   // update IV
   iv.clear();
   iv.putLong(iv64, 8, rt::ByteBuffer::BigEndian);
   iv.flip();

   output.flip();

   return output;
}

}
