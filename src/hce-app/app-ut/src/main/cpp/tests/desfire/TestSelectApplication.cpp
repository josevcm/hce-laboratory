#include "TestSelectApplication.h"

using namespace hce::cards::desfire;

void testSelectApplication(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- selectApplication ---");

   int sw = card.selectApplication(0x000000);
   ctx.check("selectApplication(0x000000) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("authMode reset to AUTH_NONE", card.authMode() == AUTH_NONE);
   ctx.check("authKeyId reset to -1", card.authKeyId() == -1);
}
