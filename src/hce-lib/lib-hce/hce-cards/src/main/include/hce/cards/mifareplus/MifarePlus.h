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
#ifndef HCE_CARDS_MIFAREPLUS_H
#define HCE_CARDS_MIFAREPLUS_H

#include <cstdint>
#include <functional>
#include <memory>

#include <rt/ByteBuffer.h>
#include <rt/Logger.h>

namespace hce::cards::mifareplus {

static constexpr int STATUS_OK         = 0x90;
static constexpr int STATUS_ERR_AUTH   = 0x06;
static constexpr int STATUS_ERR_LENGTH = 0x07;
static constexpr int STATUS_ERR_PARAM  = 0x0B;
static constexpr int STATUS_ERR_NOTAUTH  = 0x0D;
static constexpr int STATUS_ERR_BOUNDARY = 0x0E;
static constexpr int STATUS_ERR_CMD    = 0x0F;

enum KeyType { KEY_A = 0, KEY_B = 1 };

using Transport = std::function<int(const rt::ByteBuffer &request, rt::ByteBuffer &response)>;

class MifarePlus
{
   public:

      explicit MifarePlus(Transport transport);

      ~MifarePlus();

      int authenticate(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key);

      int authenticateFollowing(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key);

      int resetAuthentication();

      int readBlock(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data);

      int readBlockEncrypted(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data);

      int writeBlock(unsigned int blockAddr, const rt::ByteBuffer &data);

      int writeBlockEncrypted(unsigned int blockAddr, const rt::ByteBuffer &data);

      int increment(unsigned int blockAddr, int32_t value);

      int decrement(unsigned int blockAddr, int32_t value);

      int restore(unsigned int blockAddr);

      int transfer(unsigned int blockAddr);

      int incrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value);

      int decrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value);

      int restoreTransfer(unsigned int srcAddr, unsigned int dstAddr);

      int getUID(rt::ByteBuffer &uid);

      bool isAuthenticated() const;

      unsigned int authSector() const;

   private:

      struct Impl;

      std::unique_ptr<Impl> impl;
};

} // namespace hce::cards::mifareplus

#endif // HCE_CARDS_MIFAREPLUS_H
