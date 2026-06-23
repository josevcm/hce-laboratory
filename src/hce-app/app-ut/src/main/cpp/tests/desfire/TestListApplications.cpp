#include <vector>
#include <algorithm>

#include <rt/ByteBuffer.h>

#include "TestListApplications.h"

using namespace hce::cards::desfire;

static constexpr unsigned int APP_ID_1 = 0x010101;
static constexpr unsigned int APP_ID_2 = 0x020202;
static constexpr unsigned int APP_ID_3 = 0x030303;

static bool contains(const std::vector<unsigned int> &ids, unsigned int id)
{
   return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void testListApplications(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- listApplications ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);

   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Create 3 test applications
      sw = card.createApplication(APP_ID_1, 0x0F, 0x01);
      ctx.check("setup: createApplication(0x010101) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createApplication(APP_ID_2, 0x0F, 0x01);
      ctx.check("setup: createApplication(0x020202) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createApplication(APP_ID_3, 0x0F, 0x01);
      ctx.check("setup: createApplication(0x030303) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify all 3 appear in list
      {
         std::vector<unsigned int> appIds;
         sw = card.listApplications(appIds);
         ctx.check("listApplications returns STATUS_OK", sw == STATUS_OK);
         ctx.check("list contains 0x010101", contains(appIds, APP_ID_1));
         ctx.check("list contains 0x020202", contains(appIds, APP_ID_2));
         ctx.check("list contains 0x030303", contains(appIds, APP_ID_3));
         LOG_INFO(ctx.log, "  total apps listed: {}", {(int)appIds.size()});
      }

      // Delete APP_ID_1 and verify it disappears
      sw = card.deleteApplication(APP_ID_1);
      ctx.check("deleteApplication(0x010101) returns STATUS_OK", sw == STATUS_OK);
      {
         std::vector<unsigned int> appIds;
         card.listApplications(appIds);
         ctx.check("0x010101 absent after delete", !contains(appIds, APP_ID_1));
         ctx.check("0x020202 still present", contains(appIds, APP_ID_2));
         ctx.check("0x030303 still present", contains(appIds, APP_ID_3));
      }

      // Delete APP_ID_2 and verify it disappears
      sw = card.deleteApplication(APP_ID_2);
      ctx.check("deleteApplication(0x020202) returns STATUS_OK", sw == STATUS_OK);
      {
         std::vector<unsigned int> appIds;
         card.listApplications(appIds);
         ctx.check("0x020202 absent after delete", !contains(appIds, APP_ID_2));
         ctx.check("0x030303 still present", contains(appIds, APP_ID_3));
      }

      // Delete APP_ID_3 and verify it disappears
      sw = card.deleteApplication(APP_ID_3);
      ctx.check("deleteApplication(0x030303) returns STATUS_OK", sw == STATUS_OK);
      {
         std::vector<unsigned int> appIds;
         sw = card.listApplications(appIds);
         ctx.check("listApplications after all deletes returns STATUS_OK", sw == STATUS_OK);
         ctx.check("0x030303 absent after delete", !contains(appIds, APP_ID_3));
         LOG_INFO(ctx.log, "  apps remaining: {}", {(int)appIds.size()});
      }
   }();

   // Teardown (same style as other tests): ensure master app is selected and remove leftovers.
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
