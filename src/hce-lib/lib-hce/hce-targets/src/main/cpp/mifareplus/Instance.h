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

#include <map>
#include <vector>

#include <rt/ByteBuffer.h>
#include <rt/Logger.h>
#include <hce/crypto/CipherAES.h>

namespace hce::targets::mifareplus {

using namespace crypto;

// --- size ---
#define MFPLUS_SIZE_2K   32   // sectors 0-31
#define MFPLUS_SIZE_4K   40   // sectors 0-39

// --- key type ---
#define MFPLUS_KEY_A     0x00
#define MFPLUS_KEY_B     0x01

// --- status ---
#define MFPLUS_STATUS_OK              0x00
#define MFPLUS_STATUS_ERR_AUTH        0x06
#define MFPLUS_STATUS_ERR_LENGTH      0x07
#define MFPLUS_STATUS_ERR_PARAM       0x0B
#define MFPLUS_STATUS_ERR_NOTAUTH     0x0D
#define MFPLUS_STATUS_ERR_BOUNDARY    0x0E
#define MFPLUS_STATUS_ERR_CMD         0x0F

// --- ISO SW ---
#define MFPLUS_SW_OK                  0x9000
#define MFPLUS_SW_ERR_AUTH            0x6300
#define MFPLUS_SW_ERR_NOTAUTH         0x6982
#define MFPLUS_SW_ERR_PARAM           0x6A86
#define MFPLUS_SW_ERR_LENGTH          0x6700
#define MFPLUS_SW_NOTFOUND            0x6A82

struct SectorEntry
{
   rt::ByteBuffer keyA; // AES-128, 16 bytes
   rt::ByteBuffer keyB; // AES-128, 16 bytes
   unsigned char accessBits[4] = {0xFF, 0x07, 0x80, 0xFF};
};

struct Authentication
{
   bool valid = false;
   unsigned int sectorNo = 0;
   unsigned int keyType = MFPLUS_KEY_A;
   rt::ByteBuffer ti; // Transaction Identifier (4 bytes)
   rt::ByteBuffer rndA; // stored for FollowingAuth
   rt::ByteBuffer sessionEncKey; // SES_AUTH_ENC (16 bytes)
   rt::ByteBuffer sessionMacKey; // SES_AUTH_MAC (16 bytes)
   rt::ByteBuffer iv; // CBC IV (16 bytes, updated per operation)
   rt::ByteBuffer transferBuffer; // value block transfer register (16 bytes)
   bool transferValid = false;
   unsigned int transferSector = 0; // sector owning the transfer
};

struct Instance
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.mifareplus.Instance");

   rt::ByteBuffer uid; // 7 bytes
   unsigned int sectorCount; // 32 (2K) or 40 (4K)

   std::vector<rt::ByteBuffer> blocks; // 16 bytes each
   std::map<unsigned int, SectorEntry> sectorMap; // sector number → keys + access bits

   Authentication auth;
   CipherAES aes;

   bool dirty = false;

   // --- helpers ---

   // total block count
   unsigned int blockCount() const;

   // first block of sector
   unsigned int firstBlock(unsigned int sector) const;

   // sector trailer block number
   unsigned int trailerBlock(unsigned int sector) const;

   // sector that owns the given block
   unsigned int sectorOf(unsigned int block) const;

   // true if block is the sector trailer
   bool isSectorTrailer(unsigned int block) const;

   // true if block index is valid
   bool isValidBlock(unsigned int block) const;

   // true if auth covers the given block
   bool isAuthenticated(unsigned int block) const;

   // key for current auth (KeyA or KeyB of authenticated sector)
   const rt::ByteBuffer &authKey() const;

   // invalidate auth state
   void invalidateAuth();

   // true if block contains a valid value block structure
   bool isValueBlock(unsigned int block) const;

   // get value from a value block (little-endian int32)
   int32_t getBlockValue(unsigned int block) const;

   // write value into a value block with address byte
   void setBlockValue(unsigned int block, int32_t value, unsigned char addr);
};

} // namespace hce::targets::mifareplus

#endif // HCE_MIFAREPLUS_INSTANCE_H
