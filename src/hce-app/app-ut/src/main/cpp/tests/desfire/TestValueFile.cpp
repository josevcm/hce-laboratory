#include <rt/ByteBuffer.h>

#include "TestValueFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0A0B0D;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testValueFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- valueFile (create/getValue/credit/debit/commit/delete) ---");

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
   ctx.check("setup: createApplication(0x0A0B0D) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0A0B0D) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Create value file: lower=0, upper=1000, initial=100, limitedCredit=false ---
      sw = card.createValueFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 1000, 100, false);
      ctx.check("createValueFile(id=0, lo=0, hi=1000, init=100) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // --- Verify file settings ---
      {
         FileSettings fs {};
         sw = card.getFileSettings(FILE_ID, fs);
         ctx.check("getFileSettings(0) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_VALUE", fs.fileType == FILE_VALUE);
         ctx.check("lowerLimit is 0",        fs.lowerLimit == 0);
         ctx.check("upperLimit is 1000",     fs.upperLimit == 1000);
      }

      // --- Read initial value ---
      {
         int value = -1;
         sw = card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("getValue returns STATUS_OK", sw == STATUS_OK);
         ctx.check("initial value is 100", value == 100);
         LOG_INFO(ctx.log, "  initial value: {}", {value});
      }

      // --- Credit 50 ---
      sw = card.credit(FILE_ID, 50, COMM_PLAIN);
      ctx.check("credit(50) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after credit returns STATUS_OK", sw == STATUS_OK);

      {
         int value = -1;
         sw = card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("getValue after credit+commit returns STATUS_OK", sw == STATUS_OK);
         ctx.check("value after credit is 150", value == 150);
         LOG_INFO(ctx.log, "  value after credit: {}", {value});
      }

      // --- Debit 30 ---
      sw = card.debit(FILE_ID, 30, COMM_PLAIN);
      ctx.check("debit(30) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after debit returns STATUS_OK", sw == STATUS_OK);

      {
         int value = -1;
         sw = card.getValue(FILE_ID, COMM_PLAIN, value);
         ctx.check("getValue after debit+commit returns STATUS_OK", sw == STATUS_OK);
         ctx.check("value after debit is 120", value == 120);
         LOG_INFO(ctx.log, "  value after debit: {}", {value});
      }

      // --- Debit beyond lower limit ---
      sw = card.debit(FILE_ID, 200, COMM_PLAIN);
      ctx.check("debit beyond lower limit returns STATUS_BOUNDARY_ERROR", sw == STATUS_BOUNDARY_ERROR);
      if (sw != STATUS_OK)
         card.abortTransaction();

      // --- Delete file ---
      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile(0) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
