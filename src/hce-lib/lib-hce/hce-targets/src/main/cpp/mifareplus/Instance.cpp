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
#include "Instance.h"

namespace hce::targets::mifareplus {

unsigned int Instance::blockCount() const
{
   if (sectorCount <= 32)
      return sectorCount * 4;

   return 32 * 4 + (sectorCount - 32) * 16;
}

unsigned int Instance::firstBlock(unsigned int sector) const
{
   if (sector < 32)
      return sector * 4;

   return 32 * 4 + (sector - 32) * 16;
}

unsigned int Instance::trailerBlock(unsigned int sector) const
{
   if (sector < 32)
      return sector * 4 + 3;
   return 32 * 4 + (sector - 32) * 16 + 15;
}

unsigned int Instance::sectorOf(unsigned int block) const
{
   if (block < 128)
      return block / 4;

   return 32 + (block - 128) / 16;
}

bool Instance::isSectorTrailer(unsigned int block) const
{
   return trailerBlock(sectorOf(block)) == block;
}

bool Instance::isValidBlock(unsigned int block) const
{
   return block < blockCount();
}

bool Instance::isAuthenticated(unsigned int block) const
{
   return auth.valid && sectorOf(block) == auth.sectorNo;
}

const rt::ByteBuffer &Instance::authKey() const
{
   assert(auth.valid);

   const auto it = sectorMap.find(auth.sectorNo);

   assert(it != sectorMap.end());

   return auth.keyType == MFPLUS_KEY_A ? it->second.keyA : it->second.keyB;
}

void Instance::invalidateAuth()
{
   auth = Authentication {};
}

bool Instance::isValueBlock(unsigned int block) const
{
   const unsigned char *d = blocks[block].data();
   for (int i = 0; i < 4; i++)
   {
      if (d[i] != d[i + 8]) return false;
      if (d[i] != static_cast<unsigned char>(~d[i + 4])) return false;
   }
   if (d[12] != d[14]) return false;
   if (static_cast<unsigned char>(~d[12]) != d[13]) return false;
   if (static_cast<unsigned char>(~d[14]) != d[15]) return false;
   return true;
}

int32_t Instance::getBlockValue(unsigned int block) const
{
   const unsigned char *d = blocks[block].data();
   return static_cast<int32_t>(d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24));
}

void Instance::setBlockValue(unsigned int block, int32_t value, unsigned char addr)
{
   unsigned char *d = blocks[block].data();
   d[0] = static_cast<unsigned char>(value & 0xFF);
   d[1] = static_cast<unsigned char>((value >> 8) & 0xFF);
   d[2] = static_cast<unsigned char>((value >> 16) & 0xFF);
   d[3] = static_cast<unsigned char>((value >> 24) & 0xFF);
   d[4] = ~d[0]; d[5] = ~d[1]; d[6] = ~d[2]; d[7] = ~d[3];
   d[8] = d[0];  d[9] = d[1];  d[10] = d[2]; d[11] = d[3];
   d[12] = addr; d[13] = ~addr; d[14] = addr; d[15] = ~addr;
}

} // namespace hce::targets::mifareplus
