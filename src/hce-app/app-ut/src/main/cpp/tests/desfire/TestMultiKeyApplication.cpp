#include <rt/ByteBuffer.h>

#include "TestMultiKeyApplication.h"

using namespace hce::cards::desfire;

// KEY_SETTINGS_2: bits 0-3 = numKeys (4), bits 6-7 = crypto type (00 = DES/3DES)
static constexpr unsigned int TEST_APP_ID    = 0x0F0F01;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x04; // 4 DES keys, no ISO

static constexpr const char *KEY_ZEROS  = "00000000000000000000000000000000";
static constexpr const char *KEY_1_NEW  = "01010101010101010101010101010101";

void testMultiKeyApplication(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- multiKeyApplication ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto zeroKey   = rt::ByteBuffer::fromHex(KEY_ZEROS);
   auto newKey1   = rt::ByteBuffer::fromHex(KEY_1_NEW);

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x0F0F01, 4 keys) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0F0F01) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify numKeys in key settings
      {
         unsigned int ks = 0, numKeys = 0;
         sw = card.getKeySettings(ks, numKeys);
         ctx.check("getKeySettings returns STATUS_OK", sw == STATUS_OK);
         ctx.check("numKeys is 4", numKeys == 4);
         LOG_INFO(ctx.log, "  keySettings: 0x{02x}  numKeys: {}", {ks, numKeys});
      }

      // Authenticate with master key (key 0) to get permission to change key 1
      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Change key 1 from zeros to KEY_1_NEW
      sw = card.changeKey(1, newKey1, zeroKey);
      ctx.check("changeKey(1, newKey1, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-select to reset auth state, then authenticate as key 1
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication after changeKey returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(1, newKey1);
      ctx.check("authenticateLegacy(key1, newKey1) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("authKeyId is 1 after authenticating as key 1", card.authKeyId() == 1);
      ctx.check("authMode is AUTH_LEGACY after key1 auth", card.authMode() == AUTH_LEGACY);
      if (sw != STATUS_OK) return;

      // Restore: re-authenticate as key 0 to change key 1 back to zeros
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0F0F01) for restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, zeroKey);
      ctx.check("authenticateLegacy(key0) for restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.changeKey(1, zeroKey, newKey1);
      ctx.check("changeKey(1, zeros, newKey1) restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify key 1 is back to zeros
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication after restore returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(1, zeroKey);
      ctx.check("authenticateLegacy(key1, zeros) after restore returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
