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

#include <aes.h>

#include <hce/crypto/CipherAES.h>

namespace hce {

CipherAES::CipherAES() : block(16)
{
}

void CipherAES::init(const rt::ByteBuffer &key, int ignored)
{
   assert(key.size() == 16 || key.size() == 24 || key.size() == 32);

   mode = key.size();

   switch (mode)
   {
      case 16:
         aes_128_init(&aes128, key.data());
         break;
      case 24:
         aes_192_init(&aes192, key.data());
         break;
      case 32:
         aes_256_init(&aes256, key.data());
         break;
   }
}

rt::ByteBuffer CipherAES::encrypt(const rt::ByteBuffer &input)
{
   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   return encrypt(input, iv);
}

rt::ByteBuffer CipherAES::decrypt(const rt::ByteBuffer &input)
{
   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   return decrypt(input, iv);
}

rt::ByteBuffer CipherAES::encrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv)
{
   assert(input.remaining() % 16 == 0);
   assert(iv.size() == 16);

   rt::ByteBuffer tmp = input;
   rt::ByteBuffer output = rt::ByteBuffer(input.remaining());

   while (tmp.remaining() >= 16)
   {
      tmp.get(block);

      // apply IV before encryption
      block = block ^ iv;

      // encrypt block with CBC send mode
      if (mode == 16)
         aes_128_encrypt(&aes128, block.data());
      else if (mode == 24)
         aes_192_encrypt(&aes192, block.data());
      else
         aes_256_encrypt(&aes256, block.data());

      // store encrypted block
      output.put(block);

      // update IV with last crypt block
      iv = block.copy();
   }

   output.flip();

   return output;
}

rt::ByteBuffer CipherAES::decrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv)
{
   assert(input.remaining() % 16 == 0);
   assert(iv.size() == 16);

   rt::ByteBuffer tmp = input;
   rt::ByteBuffer output = rt::ByteBuffer(input.remaining());
   rt::ByteBuffer niv(16);

   while (tmp.remaining() >= 16)
   {
      tmp.get(block);

      // new IV is current block
      niv = block.copy();

      if (mode == 16)
         aes_128_decrypt(&aes128, block.data());
      else if (mode == 24)
         aes_192_decrypt(&aes192, block.data());
      else
         aes_256_decrypt(&aes256, block.data());

      // apply IV after decryption
      block = block ^ iv;

      // store decrypted block
      output.put(block);

      // update IV with last crypt block
      iv = niv;
   }

   output.flip();

   return output;
}

}
