#include <rt/ByteBuffer.h>

#include "TestGetKeySettings.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0B0C0D;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testGetKeySettings(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getKeySettings ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);

   // --- Master app ---
   int sw = card.selectApplication(0x000000);
   ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   unsigned int masterSettings = 0, masterNumKeys = 0;
   sw = card.getKeySettings(masterSettings, masterNumKeys);
   ctx.check("getKeySettings(master) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("master keySettings is 0x0F (default)", masterSettings == 0x0F);
   ctx.check("master numKeys is 1", masterNumKeys == 1);
   LOG_INFO(ctx.log, "  master keySettings: 0x{02x}  numKeys: {}", {masterSettings, masterNumKeys});

   // --- User app: verify getKeySettings reflects creation parameters ---
   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0B0C0D) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0B0C0D) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      unsigned int appSettings = 0, appNumKeys = 0;
      sw = card.getKeySettings(appSettings, appNumKeys);
      ctx.check("getKeySettings(user app) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("user app keySettings matches KEY_SETTINGS_1 (0x0F)", appSettings == KEY_SETTINGS_1);
      ctx.check("user app numKeys matches KEY_SETTINGS_2 low nibble (1)", appNumKeys == (KEY_SETTINGS_2 & 0x0F));
      LOG_INFO(ctx.log, "  app keySettings: 0x{02x}  numKeys: {}", {appSettings, appNumKeys});
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
