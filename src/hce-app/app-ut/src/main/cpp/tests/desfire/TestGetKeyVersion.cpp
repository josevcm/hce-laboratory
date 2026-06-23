#include "TestGetKeyVersion.h"

using namespace hce::cards::desfire;

void testGetKeyVersion(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getKeyVersion ---");

   card.selectApplication(0x000000);

   unsigned int keyVersion = 0xFF;
   int sw = card.getKeyVersion(0, keyVersion);
   ctx.check("getKeyVersion(0) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("key version is a valid byte", keyVersion <= 0xFF);

   LOG_INFO(ctx.log, "  Key 0 version: 0x{02x}", {keyVersion});
}
