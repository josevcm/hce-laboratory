#include <rt/ByteBuffer.h>

#include "TestFormatCard.h"

using namespace hce::cards::desfire;

void testFormatCard(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- formatCard ---");

   int sw = card.selectApplication(0x000000);
   ctx.check("pre-format: selectApplication(master) OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX));
   ctx.check("pre-format: authenticate with master key OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.formatCard();
   ctx.check("formatCard returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Verify card is back to virgin: re-select + re-auth with default key must succeed
   sw = card.selectApplication(0x000000);
   ctx.check("post-format: selectApplication(master) OK", sw == STATUS_OK);

   // Authenticate with master key
   sw = card.authenticateLegacy(0, rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX));
   ctx.check("post-format: re-auth with default key OK (virgin state confirmed)", sw == STATUS_OK);

   if (sw == STATUS_OK)
   {
      // Verify no user applications remain
      std::vector<unsigned int> appIds;
      card.listApplications(appIds);
      ctx.check("post-format: no user applications remain", appIds.empty());
      LOG_INFO(ctx.log, "  Remaining apps: {}", {(int)appIds.size()});
   }
}
