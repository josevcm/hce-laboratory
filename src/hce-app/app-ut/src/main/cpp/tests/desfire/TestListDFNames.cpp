#include <rt/ByteBuffer.h>

#include "TestListDFNames.h"

using namespace hce::cards::desfire;

// ISO-enabled app parameters
static constexpr unsigned int ISO_APP_AID       = 0x010203;
static constexpr unsigned int ISO_APP_KS1       = 0x0F;
static constexpr unsigned int ISO_APP_KS2       = 0x21; // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int ISO_APP_ISO_ID    = 0xA000;
static constexpr const char  *ISO_APP_ISO_NAME  = "A0000001020304"; // 7 bytes

void testListDFNames(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- listDFNames ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex("00000000000000000000000000000000");

   // Setup: format card
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Empty card: listDFNames must return empty list
   std::vector<rt::ByteBuffer> dfs;
   sw = card.listDFNames(dfs);
   ctx.check("listDFNames on empty card returns STATUS_OK", sw == STATUS_OK);
   ctx.check("listDFNames on empty card returns empty list", dfs.empty());

   // Create ISO-enabled application
   auto isoName = rt::ByteBuffer::fromHex(ISO_APP_ISO_NAME);
   sw = card.createApplicationIso(ISO_APP_AID, ISO_APP_KS1, ISO_APP_KS2, ISO_APP_ISO_ID, isoName);
   ctx.check("createApplicationIso returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // listDFNames must now return the ISO-enabled app
   dfs.clear();
   sw = card.listDFNames(dfs);
   ctx.check("listDFNames after ISO app creation returns STATUS_OK", sw == STATUS_OK);
   ctx.check("listDFNames returns 1 entry", dfs.size() == 1);

   if (!dfs.empty())
   {
      rt::ByteBuffer entry = dfs[0];

      // Each entry: AID[3 LE] + isoId[2 LE] + isoName[N]
      unsigned int aid   = entry.getInt(3);
      unsigned int isoId = entry.getInt(2);

      LOG_INFO(ctx.log, "  DF entry: AID=0x{06x} isoId=0x{04x}", {aid, isoId});
      ctx.check("listDFNames entry AID matches", aid == ISO_APP_AID);
      ctx.check("listDFNames entry isoId matches", isoId == ISO_APP_ISO_ID);
      ctx.check("listDFNames entry name length is 7", entry.remaining() == 7);
   }

   // Teardown
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
