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

#include "TestBackupFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x0F0E0D;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID = 0x00;
static constexpr unsigned int FILE_SIZE = 16;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testBackupFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- backupFile (create/write/commit/read/delete) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0F0E0D) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0F0E0D) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Create backup file ---
      sw = card.createBackupFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createBackupFile(id=0, size=16, plain) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Verify file settings ---
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings(0) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_BACKUP", fs.fileType == FILE_BACKUP);
         ctx.check("fileSize is 16", fs.fileSize == FILE_SIZE);
      }

      // --- Write data (pending until commit) ---
      {
         rt::ByteBuffer writeData(FILE_SIZE);
         for (unsigned int i = 0; i < FILE_SIZE; i++) writeData.putInt(0xAA + i, 1);
         writeData.flip();

         sw = card.writeData(FILE_ID, 0, FILE_SIZE, COMM_PLAIN, writeData);
         ctx.check("writeData(0, 0, 16, plain) returns STATUS_OK", sw == STATUS_OK);
         if (sw != STATUS_OK) return;
      }

      // --- Commit transaction ---
      sw = card.commitTransaction();
      ctx.check("commitTransaction returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Read back and verify ---
      {
         rt::ByteBuffer readData(FILE_SIZE + 8);
         sw = card.readData(FILE_ID, 0, FILE_SIZE, COMM_PLAIN, readData);
         ctx.check("readData after commit returns STATUS_OK", sw == STATUS_OK);

         if (sw == STATUS_OK)
         {
            ctx.check("read returns 16 bytes", readData.remaining() == FILE_SIZE);

            bool match = true;

            for (unsigned int i = 0; i < FILE_SIZE && i < readData.remaining(); i++)
            {
               if (readData[readData.position() + i] != static_cast<unsigned char>(0xAA + i))
               {
                  match = false;
                  break;
               }
            }

            ctx.check("read data matches written pattern", match);
         }
      }

      // --- Delete file ---
      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile(0) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
