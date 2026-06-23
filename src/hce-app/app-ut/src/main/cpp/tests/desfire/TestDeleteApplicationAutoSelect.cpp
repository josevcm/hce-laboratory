#include <rt/ByteBuffer.h>

#include "TestDeleteApplicationAutoSelect.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0A0A0A;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testDeleteApplicationAutoSelect(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- deleteApplicationAutoSelect ---");

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
   ctx.check("setup: createApplication(0x0A0A0A) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Select and authenticate the app we are about to delete
   sw = card.selectApplication(TEST_APP_ID);
   ctx.check("selectApplication(0x0A0A0A) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, appKey);
   ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Delete the currently selected application
   sw = card.deleteApplication(TEST_APP_ID);
   ctx.check("deleteApplication(selected app) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Per spec §9.4.2: deleting the selected app auto-selects PICC level (AID=0x000000).
   // We can verify this by calling a PICC-level command without an explicit selectApplication.
   {
      std::vector<unsigned int> appIds;
      sw = card.listApplications(appIds);
      ctx.check("listApplications without re-select returns STATUS_OK (PICC auto-selected)", sw == STATUS_OK);
      ctx.check("listApplications returns empty list after delete", appIds.empty());
   }

   // The deleted application must not be found any more
   sw = card.selectApplication(TEST_APP_ID);
   ctx.check("selectApplication(deleted app) returns STATUS_APPLICATION_NOT_FOUND", sw == STATUS_APPLICATION_NOT_FOUND);

   // Restore clean state for subsequent tests
   card.selectApplication(0x000000);
}
