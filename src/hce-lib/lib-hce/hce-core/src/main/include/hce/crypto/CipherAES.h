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

#ifndef HCE_CIPHERAES_H
#define HCE_CIPHERAES_H

#include <aes.h>

#include <rt/Logger.h>

#include <hce/crypto/Cipher.h>

namespace hce {

class CipherAES : public Cipher
{
   rt::Logger *log = rt::Logger::getLogger("hce.CipherAES");

   public:

      CipherAES();

      void init(const rt::ByteBuffer &key, int ignored) override;

      rt::ByteBuffer encrypt(const rt::ByteBuffer &input) override;

      rt::ByteBuffer decrypt(const rt::ByteBuffer &input) override;

      rt::ByteBuffer encrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv) override;

      rt::ByteBuffer decrypt(const rt::ByteBuffer &input, rt::ByteBuffer &iv) override;

   private:

      int mode = 0;

      aes_128_context_t aes128 = {};
      aes_192_context_t aes192 = {};
      aes_256_context_t aes256 = {};

      rt::ByteBuffer block;
};

}
#endif //HCE_CIPHERAES_H
