#include <rt/ByteBuffer.h>

#include "TestValueFileValidation.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0E0F01;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testValueFileValidation(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- valueFileValidation (parameter checks) ---");

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
   ctx.check("setup: createApplication(0x0E0F01) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0E0F01) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Invalid: upper < lower (lower=100, upper=50)
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 100, 50, 75, false);
      ctx.check("createValueFile(lower=100, upper=50) returns STATUS_PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      // Invalid: initial > upper (lower=0, upper=100, initial=150)
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 100, 150, false);
      ctx.check("createValueFile(initial=150 > upper=100) returns STATUS_PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      // Invalid: initial < lower (lower=50, upper=100, initial=30)
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 50, 100, 30, false);
      ctx.check("createValueFile(initial=30 < lower=50) returns STATUS_PARAMETER_ERROR", sw == STATUS_PARAMETER_ERROR);

      // Valid: lower=0, upper=100, initial=50
      sw = card.createValueFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 100, 50, false);
      ctx.check("createValueFile(valid: lo=0, hi=100, init=50) returns STATUS_OK", sw == STATUS_OK);

      if (sw == STATUS_OK)
      {
         FileSettings fs {};
         sw = card.getFileSettings(0x00, fs);
         ctx.check("getFileSettings on valid value file returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_VALUE", fs.fileType == FILE_VALUE);

         card.deleteFile(0x00);
      }
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
