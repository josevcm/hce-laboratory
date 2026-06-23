#include <rt/ByteBuffer.h>

#include "TestIsoSelect.h"

using namespace hce::cards::desfire;

// DESFire master application fixed ISO DF name (7 bytes)
static constexpr const char *MASTER_ISO_DF_NAME = "D2760000850100";

void testIsoSelect(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- isoSelect ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);

   // Select master app via ISO DF name
   auto dfName = rt::ByteBuffer::fromHex(MASTER_ISO_DF_NAME);
   int sw = card.isoSelectByName(dfName);
   ctx.check("isoSelect(master DF name) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
   if (sw != ISO_STATUS_OK) return;

   // Confirm the master app is now selected by authenticating
   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("authenticateLegacy after isoSelect returns STATUS_OK", sw == STATUS_OK);
   ctx.check("authMode is AUTH_LEGACY after isoSelect + auth", card.authMode() == AUTH_LEGACY);

   // Select master via native command to restore clean state
   card.selectApplication(0x000000);
}
