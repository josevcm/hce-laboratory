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
#include <cstdint>
#include "TestMFPValueBlock.h"

using namespace hce::cards::mifareplus;

// Build a properly formatted Mifare value block for a given int32 value and address byte.
// Format: [val LE 4][~val 4][val 4][addr][~addr][addr][~addr]
static rt::ByteBuffer makeValueBlock(int32_t value, unsigned char addr = 0x00)
{
   rt::ByteBuffer blk(16);
   unsigned char v[4];
   v[0] = static_cast<unsigned char>(value & 0xFF);
   v[1] = static_cast<unsigned char>((value >> 8) & 0xFF);
   v[2] = static_cast<unsigned char>((value >> 16) & 0xFF);
   v[3] = static_cast<unsigned char>((value >> 24) & 0xFF);

   blk.put(v[0]).put(v[1]).put(v[2]).put(v[3]);
   blk.put(static_cast<unsigned char>(~v[0])).put(static_cast<unsigned char>(~v[1]))
      .put(static_cast<unsigned char>(~v[2])).put(static_cast<unsigned char>(~v[3]));
   blk.put(v[0]).put(v[1]).put(v[2]).put(v[3]);
   blk.put(addr).put(static_cast<unsigned char>(~addr)).put(addr).put(static_cast<unsigned char>(~addr));
   blk.flip();
   return blk;
}

static int32_t readValueFromBlock(const rt::ByteBuffer &blk)
{
   const unsigned char *d = blk.data();
   return static_cast<int32_t>(d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24));
}

void testMFPValueBlock(MifarePlus &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- MFP value block (increment / decrement / restore / transfer) ---");

   auto zeroKey = rt::ByteBuffer::zero(16);

   // --- Setup: write value block with initial value 100 to block 1 ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("setup: authenticate(sector=0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto initBlock = makeValueBlock(100);
      sw = card.writeBlock(1, initBlock);
      ctx.check("setup: write value block 1 (value=100) returns STATUS_OK", sw == STATUS_OK);
   }();

   // --- Increment: 100 + 50 → transfer register, then transfer to same block ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for increment test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.increment(1, 50);
      ctx.check("increment(block=1, value=50) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.transfer(1);
      ctx.check("transfer(block=1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Read back and verify value is 150
      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after increment+transfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data(16);
      sw = card.readBlock(1, 1, data);
      ctx.check("readBlock after increment+transfer returns STATUS_OK", sw == STATUS_OK);
      data.flip();
      ctx.check("block value is 150 after increment(50)+transfer", readValueFromBlock(data) == 150);
   }();

   // --- Decrement: 150 - 30 → transfer register, then transfer to same block ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for decrement test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.decrement(1, 30);
      ctx.check("decrement(block=1, value=30) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.transfer(1);
      ctx.check("transfer(block=1) after decrement returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after decrement+transfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data(16);
      sw = card.readBlock(1, 1, data);
      ctx.check("readBlock after decrement+transfer returns STATUS_OK", sw == STATUS_OK);
      data.flip();
      ctx.check("block value is 120 after decrement(30)+transfer", readValueFromBlock(data) == 120);
   }();

   // --- Restore: copy block 1 to transfer register, then transfer to block 2 ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for restore test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Initialise block 2 as value block (value=0)
      auto blk2 = makeValueBlock(0);
      sw = card.writeBlock(2, blk2);
      ctx.check("setup: write value block 2 (value=0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate for restore test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.restore(1);
      ctx.check("restore(block=1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.transfer(2);
      ctx.check("transfer(block=2) after restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after restore+transfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data2(16);
      sw = card.readBlock(2, 1, data2);
      ctx.check("readBlock(2) after restore+transfer returns STATUS_OK", sw == STATUS_OK);
      data2.flip();
      ctx.check("block 2 value equals block 1 value after restore+transfer", readValueFromBlock(data2) == 120);
   }();

   // --- IncrementTransfer: atomic increment + transfer to destination block ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for incrementTransfer test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.incrementTransfer(1, 2, 5);
      ctx.check("incrementTransfer(src=1, dst=2, value=5) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after incrementTransfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data2(16);
      sw = card.readBlock(2, 1, data2);
      ctx.check("readBlock(dst=2) after incrementTransfer returns STATUS_OK", sw == STATUS_OK);
      data2.flip();
      ctx.check("dst block value is 125 after incrementTransfer(src=120, +5)", readValueFromBlock(data2) == 125);
   }();

   // --- DecrementTransfer: atomic decrement + transfer ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for decrementTransfer test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.decrementTransfer(1, 2, 20);
      ctx.check("decrementTransfer(src=1, dst=2, value=20) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after decrementTransfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data2(16);
      sw = card.readBlock(2, 1, data2);
      ctx.check("readBlock(dst=2) after decrementTransfer returns STATUS_OK", sw == STATUS_OK);
      data2.flip();
      ctx.check("dst block value is 100 after decrementTransfer(src=120, -20)", readValueFromBlock(data2) == 100);
   }();

   // --- RestoreTransfer: atomic copy src to dst ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for restoreTransfer test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.restoreTransfer(1, 2);
      ctx.check("restoreTransfer(src=1, dst=2) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("re-authenticate after restoreTransfer returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer data1(16), data2(16);
      card.readBlock(1, 1, data1);
      data1.flip();
      card.authenticate(0, KEY_A, zeroKey);
      card.readBlock(2, 1, data2);
      data2.flip();
      ctx.check("dst block value equals src block value after restoreTransfer", readValueFromBlock(data1) == readValueFromBlock(data2));
   }();

   // --- Increment on non-value block returns error ---
   [&] {
      int sw = card.authenticate(0, KEY_A, zeroKey);
      ctx.check("authenticate for non-value-block test returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Block 1 is currently a proper value block, so clear it first
      auto plain = rt::ByteBuffer::fromHex("AABBCCDDEEFF00112233445566778899");
      card.writeBlock(1, plain);

      card.authenticate(0, KEY_A, zeroKey);
      sw = card.increment(1, 10);
      ctx.check("increment on non-value block returns error", sw != STATUS_OK);
   }();

   // Restore auth state
   card.authenticate(0, KEY_A, zeroKey);
}
