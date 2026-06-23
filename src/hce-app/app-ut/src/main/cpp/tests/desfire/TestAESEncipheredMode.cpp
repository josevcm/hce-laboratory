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

#include "TestAESEncipheredMode.h"

using namespace hce::cards::desfire;

static constexpr unsigned int AES_APP_ID    = 0xAE5001;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x81; // AES key type, 1 key
static constexpr unsigned int STD_FILE_ID   = 0x00;
static constexpr unsigned int BAK_FILE_ID   = 0x01;
static constexpr unsigned int VAL_FILE_ID   = 0x02;
static constexpr unsigned int FILE_SIZE     = 16;

void testAESEncipheredMode(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- aesEncipheredMode (COMM_CRYPT with AES) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto aesKey    = rt::ByteBuffer::fromHex(MASTER_KEY_AES_HEX);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(AES_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0xAE5001, AES) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(AES_APP_ID);
      ctx.check("selectApplication(0xAE5001) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("authenticateAES(key0) for file creation returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(STD_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createStandardFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(BAK_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createBackupFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createValueFile(VAL_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, 0, 1000, 0, false);
      ctx.check("createValueFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: standard file COMM_CRYPT write/read with AES session");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for standard write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer wdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) wdata.putInt(0xA0 + i, 1);
      wdata.flip();

      sw = card.writeData(STD_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, wdata);
      ctx.check("writeData(COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-auth to reset session IV before read (MACED ACK from write not processed by client)
      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for standard read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer rdata(FILE_SIZE);
      sw = card.readData(STD_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, rdata);
      ctx.check("readData(COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(COMM_CRYPT, AES) returns FILE_SIZE bytes", rdata.remaining() == FILE_SIZE);

      if (rdata.remaining() == FILE_SIZE)
      {
         bool match = true;
         for (unsigned int i = 0; i < FILE_SIZE; i++)
         {
            if (rdata[i] != static_cast<unsigned char>(0xA0 + i))
            {
               match = false;
               break;
            }
         }
         ctx.check("readData(COMM_CRYPT, AES) returns original data", match);
      }

      LOG_INFO(ctx.log, "  Test B: backup file COMM_CRYPT write/commit/read with AES session");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for backup write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer bdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) bdata.putInt(0xB0 + i, 1);
      bdata.flip();

      sw = card.writeData(BAK_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, bdata);
      ctx.check("writeData(backupFile, COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after backup write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for backup read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer brdata(FILE_SIZE);
      sw = card.readData(BAK_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, brdata);
      ctx.check("readData(backupFile, COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(backupFile, COMM_CRYPT, AES) returns FILE_SIZE bytes", brdata.remaining() == FILE_SIZE);

      if (brdata.remaining() == FILE_SIZE)
      {
         bool match = true;
         for (unsigned int i = 0; i < FILE_SIZE; i++)
         {
            if (brdata[i] != static_cast<unsigned char>(0xB0 + i))
            {
               match = false;
               break;
            }
         }
         ctx.check("readData(backupFile, COMM_CRYPT, AES) matches written data", match);
      }

      LOG_INFO(ctx.log, "  Test C: value file COMM_CRYPT credit/commit/getValue with AES session");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for credit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.credit(VAL_FILE_ID, 300, COMM_CRYPT);
      ctx.check("credit(300, COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after credit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for getValue returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      int value = -1;
      sw = card.getValue(VAL_FILE_ID, COMM_CRYPT, value);
      ctx.check("getValue(COMM_CRYPT, AES) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getValue(COMM_CRYPT, AES) returns 300", value == 300);

      sw = card.deleteFile(VAL_FILE_ID);
      ctx.check("deleteFile(value) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(BAK_FILE_ID);
      ctx.check("deleteFile(backup) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(STD_FILE_ID);
      ctx.check("deleteFile(standard) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
