#include <rt/ByteBuffer.h>

#include "TestStandardFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0C0D0E;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

static constexpr unsigned int FILE_ID        = 0x00;
static constexpr unsigned int FILE_SIZE      = 32;

// 2K3DES default key (all zeros, 16 bytes)
static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testStandardFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- standardFile (create/getSettings/write/read/delete) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // --- Setup: create test app ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0C0D0E) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // --- Select and authenticate in test app ---
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0C0D0E) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Create standard file ---
      sw = card.createStandardFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(id=0, size=32, plain) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Verify file appears in listing ---
      {
         std::vector<unsigned int> fileIds;
         sw = card.listFiles(fileIds);
         ctx.check("listFiles returns STATUS_OK", sw == STATUS_OK);

         bool found = false;
         for (auto id : fileIds)
            if (id == FILE_ID) { found = true; break; }
         ctx.check("file 0x00 is listed", found);
      }

      // --- Get file settings and verify ---
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings(0) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_STANDARD", fs.fileType == FILE_STANDARD);
         ctx.check("commMode is COMM_PLAIN",    fs.commSettings == COMM_PLAIN);
         ctx.check("fileSize is 32",            fs.fileSize == FILE_SIZE);
         LOG_INFO(ctx.log, "  fileSettings: type={} comm={} size={}", {fs.fileType, fs.commSettings, fs.fileSize});
      }

      // --- Write data ---
      {
         rt::ByteBuffer writeData(FILE_SIZE);
         for (unsigned int i = 0; i < FILE_SIZE; i++) writeData.putInt(i, 1);
         writeData.flip();

         sw = card.writeData(FILE_ID, 0, FILE_SIZE, COMM_PLAIN, writeData);
         ctx.check("writeData(0, 0, 32, plain) returns STATUS_OK", sw == STATUS_OK);
         if (sw != STATUS_OK) return;
      }

      // --- Read data back and verify ---
      {
         rt::ByteBuffer readData(FILE_SIZE + 8);
         sw = card.readData(FILE_ID, 0, FILE_SIZE, COMM_PLAIN, readData);
         ctx.check("readData(0, 0, 32, plain) returns STATUS_OK", sw == STATUS_OK);

         if (sw == STATUS_OK)
         {
            ctx.check("readData returns 32 bytes", readData.remaining() == FILE_SIZE);

            bool match = true;
            for (unsigned int i = 0; i < FILE_SIZE && i < readData.remaining(); i++)
               if (readData.data()[readData.position() + i] != static_cast<unsigned char>(i)) { match = false; break; }
            ctx.check("read data matches written pattern", match);
         }
      }

      // --- Delete file ---
      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile(0) returns STATUS_OK", sw == STATUS_OK);

      // --- Verify file gone ---
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings after delete returns FILE_NOT_FOUND", sw == STATUS_FILE_NOT_FOUND);
      }
   }();

   // --- Teardown ---
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
