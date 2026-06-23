#include <rt/ByteBuffer.h>

#include "TestDuplicateErrors.h"

using namespace hce::cards::desfire;

static constexpr unsigned int DUPL_APP_ID    = 0x3A3B3C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01; // DES, 1 key
static constexpr unsigned int FILE_SIZE      = 16;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testDuplicateErrors(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- duplicateErrors ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      LOG_INFO(ctx.log, "  Test A: duplicate AID returns DUPLICATE_ERROR");

      sw = card.createApplication(DUPL_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
      ctx.check("createApplication(0x3A3B3C) first call returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createApplication(DUPL_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
      ctx.check("createApplication(0x3A3B3C) duplicate returns STATUS_DUPLICATE_ERROR", sw == STATUS_DUPLICATE_ERROR);

      LOG_INFO(ctx.log, "  Test B: duplicate file ID returns DUPLICATE_ERROR");

      sw = card.selectApplication(DUPL_APP_ID);
      ctx.check("selectApplication(0x3A3B3C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(id=0) first call returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createStandardFile(id=0) duplicate returns STATUS_DUPLICATE_ERROR", sw == STATUS_DUPLICATE_ERROR);

      // Verify only 1 file exists
      std::vector<unsigned int> files;
      sw = card.listFiles(files);
      ctx.check("listFiles returns STATUS_OK", sw == STATUS_OK);
      ctx.check("listFiles shows exactly 1 file after duplicate attempt", files.size() == 1);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
