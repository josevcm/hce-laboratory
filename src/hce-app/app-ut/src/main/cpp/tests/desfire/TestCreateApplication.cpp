#include <algorithm>

#include <rt/ByteBuffer.h>

#include "TestCreateApplication.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x010203;

void testCreateApplication(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- createApplication ---");

   // Select master application and authenticate
   int sw = card.selectApplication(0x000000);
   ctx.check("selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   auto keyDes = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   sw = card.authenticateLegacy(0, keyDes);
   ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Create a test application: 1 DES key, keySettings1=0x0F (all keys free), keySettings2=0x01 (1 key, DES)
   sw = card.createApplication(TEST_APP_ID, 0x0F, 0x01);
   ctx.check("createApplication(0x010203) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Verify the application appears in the directory
      std::vector<unsigned int> appIds;
      sw = card.listApplications(appIds);
      ctx.check("listApplications returns STATUS_OK", sw == STATUS_OK);

      bool found = std::find(appIds.begin(), appIds.end(), TEST_APP_ID) != appIds.end();
      ctx.check("application 0x010203 is listed", found);
      LOG_INFO(ctx.log, "  Application count after create: {}", {(int)appIds.size()});

      // Select the new app to verify it is accessible
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x010203) returns STATUS_OK", sw == STATUS_OK);

      // Return to master app and re-authenticate to allow deletion
      sw = card.selectApplication(0x000000);
      ctx.check("re-selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, keyDes);
      ctx.check("re-authenticateLegacy for delete returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Delete the test application
      sw = card.deleteApplication(TEST_APP_ID);
      ctx.check("deleteApplication(0x010203) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify it is gone
      appIds.clear();
      sw = card.listApplications(appIds);
      ctx.check("listApplications after delete returns STATUS_OK", sw == STATUS_OK);

      found = std::find(appIds.begin(), appIds.end(), TEST_APP_ID) != appIds.end();
      ctx.check("application 0x010203 is no longer listed", !found);
      LOG_INFO(ctx.log, "  Application count after delete: {}", {(int)appIds.size()});
   }();

   // Teardown: ensure PICC is clean even if the test body returned early
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, keyDes);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
