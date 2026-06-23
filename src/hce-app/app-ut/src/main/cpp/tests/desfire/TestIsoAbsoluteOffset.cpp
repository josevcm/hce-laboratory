#include <rt/ByteBuffer.h>

#include "TestIsoAbsoluteOffset.h"

using namespace hce::cards::desfire;

static constexpr unsigned int ABS_APP_AID = 0x6A6B6C;
static constexpr unsigned int ABS_APP_KS1 = 0x0F;
static constexpr unsigned int ABS_APP_KS2 = 0x21; // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int ABS_APP_ISO_ID = 0xF100;
static constexpr const char *ABS_APP_NAME = "AABBCCDDEEFF00"; // 7 bytes

static constexpr unsigned int ABS_FILE_ID = 0x00;
static constexpr unsigned int ABS_FILE_ISO_ID = 0xF101;
static constexpr unsigned int ABS_FILE_SIZE = 32;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testIsoAbsoluteOffset(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- isoAbsoluteOffset (absolute P1P2 offset + isoSelectByName) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
   auto isoName = rt::ByteBuffer::fromHex(ABS_APP_NAME);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplicationIso(ABS_APP_AID, ABS_APP_KS1, ABS_APP_KS2, ABS_APP_ISO_ID, isoName);
   ctx.check("createApplicationIso(0x6A6B6C, isoId=0xF100, name=AABBCCDDEEFF00) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      // Create an ISO standard file inside the app
      sw = card.selectApplication(ABS_APP_AID);
      ctx.check("selectApplication(0x6A6B6C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFileIso(ABS_FILE_ID, ABS_FILE_ISO_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, ABS_FILE_SIZE);
      ctx.check("createStandardFileIso(isoId=0xF101, size=32) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Return to master before ISO select
      sw = card.selectApplication(0x000000);
      ctx.check("selectApplication(master) before isoSelectByName returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: isoSelectByName for user app");

      sw = card.isoSelectByName(isoName);
      ctx.check("isoSelectByName(user app DF name) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      // Confirm app is selected by authenticating
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy after isoSelectByName returns STATUS_OK", sw == STATUS_OK);
      ctx.check("authMode is AUTH_LEGACY after isoSelectByName + auth", card.authMode() == AUTH_LEGACY);

      LOG_INFO(ctx.log, "  Test B: isoReadBinary with absolute offset (P1=0x00)");

      // Re-select via isoSelectByName for ISO read
      card.selectApplication(0x000000);
      sw = card.isoSelectByName(isoName);
      ctx.check("isoSelectByName(user app) for ISO read returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      sw = card.isoSelectById(ABS_FILE_ISO_ID);
      ctx.check("isoSelectById(0xF101) before absolute offset read/update returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      // SFI = ABS_FILE_ISO_ID & 0x1F = 0x01
      const unsigned int sfi = ABS_FILE_ISO_ID & 0x1F;

      // Absolute offset read: P1=0x00, P2=0x00 (offset 0)
      rt::ByteBuffer absRead(ABS_FILE_SIZE);
      sw = card.isoReadBinary(0x00, 0x00, ABS_FILE_SIZE, absRead);
      ctx.check("isoReadBinary(absolute offset 0) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      ctx.check("isoReadBinary(absolute offset) returns ABS_FILE_SIZE bytes", absRead.remaining() == ABS_FILE_SIZE);

      LOG_INFO(ctx.log, "  Test C: isoUpdateBinary with absolute offset (P1=0x00, P2=0x10)");

      const rt::ByteBuffer payload = {0xAA, 0xBB, 0xCC, 0xDD};
      sw = card.isoUpdateBinary(0x00, 0x10, payload);
      ctx.check("isoUpdateBinary(absolute offset 16) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      if (sw != ISO_STATUS_OK) return;

      rt::ByteBuffer verifyBuf(4);
      sw = card.isoReadBinary(0x00, 0x10, 4, verifyBuf);
      ctx.check("isoReadBinary(absolute offset 16) after update returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
      ctx.check("isoReadBinary returns written bytes at offset 16", sw == ISO_STATUS_OK && verifyBuf.remaining() == 4 && verifyBuf.data()[0] == 0xAA && verifyBuf.data()[1] == 0xBB && verifyBuf.data()[2] == 0xCC && verifyBuf.data()[3] == 0xDD);

      LOG_INFO(ctx.log, "  Test D: isoReadBinary beyond file end");

      // SFI read beyond end (using SFI mode as baseline)
      rt::ByteBuffer overflowBuf(4);
      sw = card.isoReadBinary(0x80 | sfi, ABS_FILE_SIZE, 1, overflowBuf);
      ctx.check("isoReadBinary(SFI, beyond end) returns ISO_STATUS_NOT_ENOUGH_DATA", sw == ISO_STATUS_NOT_ENOUGH_DATA);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
