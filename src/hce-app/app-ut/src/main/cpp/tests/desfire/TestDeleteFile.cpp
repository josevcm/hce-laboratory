#include <rt/ByteBuffer.h>

#include "TestDeleteFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0D0E0F;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_SIZE      = 32;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testDeleteFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- deleteFile ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0D0E0F) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0D0E0F) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create 3 standard files: ids 0, 1, 2
      for (unsigned int id = 0; id <= 2; id++)
      {
         sw = card.createStandardFile(id, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
         ctx.check("createStandardFile returns STATUS_OK", sw == STATUS_OK);
         if (sw != STATUS_OK) return;
      }

      // All 3 must appear in listFiles
      {
         std::vector<unsigned int> ids;
         sw = card.listFiles(ids);
         ctx.check("listFiles before delete returns STATUS_OK", sw == STATUS_OK);
         ctx.check("listFiles returns 3 files before delete", ids.size() == 3);
      }

      // Delete file 1
      sw = card.deleteFile(1);
      ctx.check("deleteFile(1) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Only files 0 and 2 must remain
      {
         std::vector<unsigned int> ids;
         sw = card.listFiles(ids);
         ctx.check("listFiles after deleteFile(1) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("listFiles returns 2 files after delete", ids.size() == 2);

         bool has0 = false, has1 = false, has2 = false;
         for (unsigned int id : ids)
         {
            if (id == 0) has0 = true;
            if (id == 1) has1 = true;
            if (id == 2) has2 = true;
         }
         ctx.check("file 0 still present after deleteFile(1)", has0);
         ctx.check("file 1 absent after deleteFile(1)", !has1);
         ctx.check("file 2 still present after deleteFile(1)", has2);
      }

      // getFileSettings on deleted file must return FILE_NOT_FOUND
      {
         FileSettings fs {};
         sw = card.getFileSettings(1, fs);
         ctx.check("getFileSettings on deleted file returns STATUS_FILE_NOT_FOUND", sw == STATUS_FILE_NOT_FOUND);
      }

      // Second delete of the same file must also return FILE_NOT_FOUND
      sw = card.deleteFile(1);
      ctx.check("deleteFile(1) again returns STATUS_FILE_NOT_FOUND", sw == STATUS_FILE_NOT_FOUND);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
