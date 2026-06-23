#include <rt/ByteBuffer.h>

#include "TestCreditBoundary.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0B0C0E;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testCreditBoundary(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- creditBoundary (upper limit enforcement) ---");

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
   ctx.check("setup: createApplication(0x0B0C0E) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0B0C0E) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create value file: lower=0, upper=100, initial=50
      sw = card.createValueFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 100, 50, false);
      ctx.check("createValueFile(lo=0, hi=100, init=50) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Credit 40 — stays within limit (50+40=90 ≤ 100)
      sw = card.credit(FILE_ID, 40, COMM_PLAIN);
      ctx.check("credit(40) returns STATUS_OK (50+40=90 within upper=100)", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after credit(40) returns STATUS_OK", sw == STATUS_OK);

      {
         int value = -1;
         card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("value is 90 after credit(40)+commit", value == 90);
         LOG_INFO(ctx.log, "  value after credit(40): {}", {value});
      }

      // Credit 20 — would exceed upper limit (90+20=110 > 100): must fail
      sw = card.credit(FILE_ID, 20, COMM_PLAIN);
      ctx.check("credit(20) returns STATUS_BOUNDARY_ERROR (90+20=110 > upper=100)", sw == STATUS_BOUNDARY_ERROR);
      if (sw != STATUS_OK)
         card.abortTransaction();

      // Value must be unchanged at 90
      {
         int value = -1;
         card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("value remains 90 after failed credit + abort", value == 90);
         LOG_INFO(ctx.log, "  value after failed credit: {}", {value});
      }

      // Exact upper-limit credit: credit(10) takes value to exactly 100 — must succeed
      sw = card.credit(FILE_ID, 10, COMM_PLAIN);
      ctx.check("credit(10) returns STATUS_OK (90+10=100 == upper=100)", sw == STATUS_OK);
      if (sw == STATUS_OK)
      {
         card.commitTransaction();
         int value = -1;
         card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("value is 100 after credit(10)+commit", value == 100);
         LOG_INFO(ctx.log, "  value at upper limit: {}", {value});
      }

      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
