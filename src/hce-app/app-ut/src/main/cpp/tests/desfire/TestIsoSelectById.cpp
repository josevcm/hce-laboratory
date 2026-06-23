#include <rt/ByteBuffer.h>

#include "TestIsoSelectById.h"

using namespace hce::cards::desfire;

// ISO-enabled application parameters
static constexpr unsigned int ISO_APP_AID    = 0x0B0B0B;
static constexpr unsigned int ISO_APP_KS1    = 0x0F;
static constexpr unsigned int ISO_APP_KS2    = 0x21; // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int ISO_APP_ISO_ID = 0x4321;
static constexpr const char  *ISO_APP_NAME   = "B0B0B0B0B0B0B0"; // 7 bytes

void testIsoSelectById(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- isoSelectById ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex("00000000000000000000000000000000");

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Create ISO-enabled application with a known 2-byte ISO ID
   auto isoName = rt::ByteBuffer::fromHex(ISO_APP_NAME);
   sw = card.createApplicationIso(ISO_APP_AID, ISO_APP_KS1, ISO_APP_KS2, ISO_APP_ISO_ID, isoName);
   ctx.check("createApplicationIso(0x0B0B0B, isoId=0x4321) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Select the application by its ISO 2-byte file identifier
      sw = card.isoSelectById(ISO_APP_ISO_ID);
      ctx.check("isoSelectById(0x4321) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      // Confirm the application is selected by authenticating with its key
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy after isoSelectById returns STATUS_OK", sw == STATUS_OK);
      ctx.check("authMode is AUTH_LEGACY after isoSelectById + auth", card.authMode() == AUTH_LEGACY);
      ctx.check("authKeyId is 0 after isoSelectById + auth", card.authKeyId() == 0);
   }();

   // Restore clean state
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
