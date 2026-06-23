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

#include <rt/ByteBuffer.h>

#include "TestCmacPlain.h"

using namespace hce::cards::desfire;

static constexpr unsigned int CMAC_APP_ID = 0x4C4D4E;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x81; // AES, 1 key
static constexpr unsigned int STD_FILE_ID = 0x00;
static constexpr unsigned int FILE_SIZE = 16;

void testCmacPlain(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- cmacPlain: CMAC on plain responses with AES session ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto aesKey = rt::ByteBuffer::fromHex(MASTER_KEY_AES_HEX);
   int sw = 0;

   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(CMAC_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(AES, 0x4C4D4E) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(CMAC_APP_ID);
      ctx.check("selectApplication(AES app) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("authenticateAES for file creation returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(STD_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(COMM_PLAIN) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES before write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer wdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) wdata.putInt(0xA0 + i, 1);
      wdata.flip();

      sw = card.writeData(STD_FILE_ID, 0, FILE_SIZE, COMM_PLAIN, wdata);
      ctx.check("writeData(COMM_PLAIN, AES session) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES before read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // in AES mode there are no real PLAIN communication
      rt::ByteBuffer rdata(FILE_SIZE);
      sw = card.readData(STD_FILE_ID, 0, FILE_SIZE, COMM_MACING, rdata);
      ctx.check("readData(COMM_MACING, AES session) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(COMM_MACING, AES session) returns FILE_SIZE bytes", rdata.remaining() == FILE_SIZE);
      ctx.check("readData(COMM_MACING, AES session) returns original data", wdata == rdata);

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES before getFileSettings returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      FileSettings fs;
      sw = card.getFileSettings(STD_FILE_ID, fs);
      ctx.check("getFileSettings(COMM_PLAIN, AES session) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getFileSettings returns correct fileSize", fs.fileSize == FILE_SIZE);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
