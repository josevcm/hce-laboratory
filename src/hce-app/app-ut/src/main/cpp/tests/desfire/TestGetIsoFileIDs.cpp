#include <rt/ByteBuffer.h>

#include "TestGetIsoFileIDs.h"

using namespace hce::cards::desfire;

static constexpr unsigned int ISO_APP_AID     = 0x010203;
static constexpr unsigned int ISO_APP_KS1     = 0x0F;
static constexpr unsigned int ISO_APP_KS2     = 0x21; // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int ISO_APP_ISO_ID  = 0xA001;
static constexpr const char  *ISO_APP_NAME    = "A0000001020305"; // 7 bytes

static constexpr unsigned int ISO_FILE0_ID    = 0x00;
static constexpr unsigned int ISO_FILE0_ISO   = 0xE001;
static constexpr unsigned int ISO_FILE1_ID    = 0x01;
static constexpr unsigned int ISO_FILE1_ISO   = 0xE002;

void testIsoGetFileIDs(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getIsoFileIDs ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex("00000000000000000000000000000000");

   // Setup: format card
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Create ISO-enabled application
   auto isoName = rt::ByteBuffer::fromHex(ISO_APP_NAME);
   sw = card.createApplicationIso(ISO_APP_AID, ISO_APP_KS1, ISO_APP_KS2, ISO_APP_ISO_ID, isoName);
   ctx.check("createApplicationIso returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Select user app and authenticate
      sw = card.selectApplication(ISO_APP_AID);
      ctx.check("selectApplication(ISO app) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // No files yet: getIsoFileIDs must return empty list
      std::vector<unsigned int> ids;
      sw = card.getIsoFileIDs(ids);
      ctx.check("getIsoFileIDs on empty app returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getIsoFileIDs on empty app returns empty list", ids.empty());

      // Create two ISO-enabled files
      sw = card.createStandardFileIso(ISO_FILE0_ID, ISO_FILE0_ISO, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 16);
      ctx.check("createStandardFileIso(0, isoId=0xE001) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFileIso(ISO_FILE1_ID, ISO_FILE1_ISO, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 16);
      ctx.check("createStandardFileIso(1, isoId=0xE002) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // getIsoFileIDs must now return both ISO IDs
      ids.clear();
      sw = card.getIsoFileIDs(ids);
      ctx.check("getIsoFileIDs after file creation returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getIsoFileIDs returns 2 IDs", ids.size() == 2);

      if (ids.size() >= 2)
      {
         // IDs are returned in file order (file 0 first, then file 1)
         ctx.check("first ISO file ID is 0xE001", ids[0] == ISO_FILE0_ISO);
         ctx.check("second ISO file ID is 0xE002", ids[1] == ISO_FILE1_ISO);
      }
   }();

   // Teardown
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
