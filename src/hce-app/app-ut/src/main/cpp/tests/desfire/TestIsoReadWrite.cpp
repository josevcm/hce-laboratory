#include <iostream>

#include <rt/ByteBuffer.h>

#include "TestIsoReadWrite.h"

using namespace hce::cards::desfire;

static constexpr unsigned int RW_APP_AID = 0xEC05EA;
static constexpr unsigned int RW_APP_KS1 = 0x0F;
static constexpr unsigned int RW_APP_KS2 = 0x21; // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int RW_APP_ISO_ID = 0xE110;
static constexpr const char *RW_APP_NAME = "D2760000850101"; // 7 bytes

static constexpr unsigned int RW_FILE_ID = 0x01;
static constexpr unsigned int RW_FILE_ISO = 0xE103; // SFI = 0xE103 & 0x1F = 0x03
static constexpr unsigned int RW_FILE_SIZE = 32;
static constexpr unsigned int RW_FILE_SFI = RW_FILE_ISO & 0x1F; // = 0x10

void testIsoReadWrite(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- isoReadBinary / isoUpdateBinary ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex("00000000000000000000000000000000");

   // Setup: format card
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Create ISO-enabled application
   auto isoName = rt::ByteBuffer::fromHex(RW_APP_NAME);
   sw = card.createApplicationIso(RW_APP_AID, RW_APP_KS1, RW_APP_KS2, RW_APP_ISO_ID, isoName);
   ctx.check("createApplicationIso returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Select user app and authenticate
      sw = card.selectApplication(RW_APP_AID);
      ctx.check("selectApplication(ISO app) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create file with ISO file ID, free read/write access
      sw = card.createStandardFileIso(RW_FILE_ID, RW_FILE_ISO, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, RW_FILE_SIZE);
      ctx.check("createStandardFileIso returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Select master app to unselect currently created
      sw = card.selectApplication(0x000000);
      ctx.check("re-selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // ISO SELECT DF by name to set directoryFile context for SFI access
      sw = card.isoSelectByName(isoName);
      ctx.check("isoSelect(DF name) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      // ISO READ BINARY using SFI (p1 = 0x80 | sfi, p2 = offset=0, le = 32)
      rt::ByteBuffer readData(RW_FILE_SIZE);
      sw = card.isoReadBinary(0x80 | RW_FILE_SFI, 0x00, RW_FILE_SIZE, readData);
      ctx.check("isoReadBinary(SFI) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      ctx.check("isoReadBinary returns 32 bytes", readData.remaining() == RW_FILE_SIZE);
      if (readData.remaining() != RW_FILE_SIZE) return;

      // Initial file content must be all zeros
      bool allZero = true;

      for (unsigned int i = 0; i < readData.remaining(); ++i)
      {
         if (readData[readData.position() + i] != 0x00)
         {
            allZero = false;
            break;
         }
      }

      ctx.check("initial file content is all zeros", allZero);
      if (!allZero) return;

      // ISO UPDATE BINARY: write 4 bytes at offset 0
      const rt::ByteBuffer payload = {0xDE, 0xAD, 0xBE, 0xEF};
      sw = card.isoUpdateBinary(0x80 | RW_FILE_SFI, 0x00, payload);
      ctx.check("isoUpdateBinary(SFI) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      // Read back the 4 written bytes and verify
      rt::ByteBuffer readBack(4);
      sw = card.isoReadBinary(0x80 | RW_FILE_SFI, 0x00, 4, readBack);
      ctx.check("isoReadBinary after update returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      ctx.check("isoReadBinary returns 4 bytes", readBack.remaining() == 4);
      ctx.check("isoReadBinary returns written data", readBack.remaining() == 4 && readBack.data()[0] == 0xDE && readBack.data()[1] == 0xAD && readBack.data()[2] == 0xBE && readBack.data()[3] == 0xEF);
      if (sw != ISO_STATUS_OK) return;

      // Read beyond file bounds: must fail
      rt::ByteBuffer overflowData(4);
      sw = card.isoReadBinary(0x80 | RW_FILE_SFI, RW_FILE_SIZE, 1, overflowData);
      ctx.check("isoReadBinary beyond file end returns NOT_ENOUGH_DATA", sw == ISO_STATUS_NOT_ENOUGH_DATA);
   }();

   LOG_DEBUG(ctx.log, "  teardown, recover card status");

   // Teardown
   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
