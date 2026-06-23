#include <rt/ByteBuffer.h>

#include "TestEncipheredMode.h"

using namespace hce::cards::desfire;

static constexpr unsigned int ENC_APP_ID = 0x4A4B4C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01; // DES/2K3DES, 1 key
static constexpr unsigned int STD_FILE_ID = 0x00;
static constexpr unsigned int BAK_FILE_ID = 0x01;
static constexpr unsigned int VAL_FILE_ID = 0x02;
static constexpr unsigned int FILE_SIZE = 16;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testEncipheredMode(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- encipheredMode (COMM_CRYPT) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(ENC_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x4A4B4C) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(ENC_APP_ID);
      ctx.check("selectApplication(0x4A4B4C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(STD_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createStandardFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(BAK_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createBackupFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createValueFile(VAL_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, 0, 1000, 0, false);
      ctx.check("createValueFile(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: standard file COMM_CRYPT write/read");

      // Re-auth to get fresh session key after file creation
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-authenticateLegacy(key0) for session key returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer wdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) wdata.putInt(0xA0 + i, 1);
      wdata.flip();

      sw = card.writeData(STD_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, wdata);
      ctx.check("writeData(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer rdata(FILE_SIZE);
      sw = card.readData(STD_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, rdata);
      ctx.check("readData(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      ctx.check("readData(COMM_CRYPT) returns FILE_SIZE bytes", rdata.remaining() == FILE_SIZE);

      if (rdata.remaining() == FILE_SIZE)
      {
         bool match = true;

         for (unsigned int i = 0; i < FILE_SIZE; i++)
         {
            if (rdata[i] != static_cast<unsigned char>(0xA0 + i))
            {
               match = false;
               break;
            }
         }

         ctx.check("readData(COMM_CRYPT) returns original data", match);
      }

      sw = card.writeData(STD_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, wdata);
      ctx.check("writeData2(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test B: backup file COMM_CRYPT write/commit/read");

      // Re-auth for backup file ops
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-authenticateLegacy for backup file returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer bdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) bdata.putInt(0xB0 + i, 1);
      bdata.flip();

      sw = card.writeData(BAK_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, bdata);
      ctx.check("writeData(backupFile, COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after backup write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-auth for read
      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-authenticateLegacy for backup read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer brdata(FILE_SIZE);
      sw = card.readData(BAK_FILE_ID, 0, FILE_SIZE, COMM_CRYPT, brdata);
      ctx.check("readData(backupFile, COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(backupFile, COMM_CRYPT) returns FILE_SIZE bytes", brdata.remaining() == FILE_SIZE);

      LOG_INFO(ctx.log, "  Test C: value file COMM_CRYPT credit/commit/getValue");

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-authenticateLegacy for value file returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.credit(VAL_FILE_ID, 500, COMM_CRYPT);
      ctx.check("credit(500, COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after credit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("re-authenticateLegacy for getValue returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      int value = -1;
      sw = card.getValue(VAL_FILE_ID, COMM_CRYPT, value);
      ctx.check("getValue(COMM_CRYPT) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getValue(COMM_CRYPT) returns 500", value == 500);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
