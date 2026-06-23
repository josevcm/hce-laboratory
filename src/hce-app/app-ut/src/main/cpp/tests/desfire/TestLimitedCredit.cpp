#include <rt/ByteBuffer.h>

#include "TestLimitedCredit.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0E0F10;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testLimitedCredit(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- limitedCredit ---");

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
   ctx.check("setup: createApplication(0x0E0F10) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0E0F10) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create value file with limitedCredit=true: lower=0, upper=1000, initial=500
      sw = card.createValueFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 1000, 500, true);
      ctx.check("createValueFile(lo=0, hi=1000, init=500, limitedCredit=true) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify limitedCredit is enabled in file settings
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_VALUE", fs.fileType == FILE_VALUE);
         ctx.check("limitedCreditEnabled is true", fs.limitedCreditEnabled);
      }

      // Verify initial value
      {
         int value = -1;
         sw = card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("getValue (initial) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("initial value is 500", value == 500);
         LOG_INFO(ctx.log, "  initial value: {}", {value});
      }

      // Debit 200 and commit (value → 300)
      sw = card.debit(FILE_ID, 200, COMM_PLAIN);
      ctx.check("debit(200) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after debit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      {
         int value = -1;
         card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("value after debit(200) is 300", value == 300);
         LOG_INFO(ctx.log, "  after debit(200): {}", {value});
      }

      // limitedCredit(200) — restore exactly the debited amount (value → 500)
      sw = card.limitedCredit(FILE_ID, 200, COMM_PLAIN);
      ctx.check("limitedCredit(200) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after limitedCredit returns STATUS_OK", sw == STATUS_OK);

      {
         int value = -1;
         card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("value after limitedCredit(200) is 500", value == 500);
         LOG_INFO(ctx.log, "  after limitedCredit(200): {}", {value});
      }

      // Debit 100 and commit (value → 400), then try limitedCredit exceeding the debit
      sw = card.debit(FILE_ID, 100, COMM_PLAIN);
      ctx.check("debit(100) returns STATUS_OK", sw == STATUS_OK);
      if (sw == STATUS_OK) card.commitTransaction();

      // limitedCredit beyond the debited amount should fail
      sw = card.limitedCredit(FILE_ID, 200, COMM_PLAIN);
      ctx.check("limitedCredit beyond debit amount returns STATUS_BOUNDARY_ERROR", sw == STATUS_BOUNDARY_ERROR);
      if (sw != STATUS_OK)
         card.abortTransaction();

      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile(0) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
