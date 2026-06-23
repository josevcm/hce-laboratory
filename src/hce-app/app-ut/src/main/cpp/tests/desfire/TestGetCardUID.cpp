#include <rt/ByteBuffer.h>

#include "TestGetCardUID.h"

using namespace hce::cards::desfire;

// Test UID set by main.cpp on the emulated server
void testGetCardUID(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getCardUID ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);

   // Setup: format card for a clean baseline
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.selectApplication(0x000000);
   ctx.check("re-selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Without auth: must return AUTHENTICATION_ERROR
   rt::ByteBuffer uid;
   sw = card.getCardUID(uid);
   ctx.check("getCardUID without auth returns STATUS_AUTHENTICATION_ERROR", sw == STATUS_AUTHENTICATION_ERROR);

   // Authenticate and retry
   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.getCardUID(uid);
   ctx.check("getCardUID with legacy auth returns STATUS_OK", sw == STATUS_OK);
   ctx.check("getCardUID returns 7-byte UID", uid.remaining() == 7);
}
