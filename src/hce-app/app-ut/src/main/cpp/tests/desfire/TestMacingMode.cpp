#include <rt/ByteBuffer.h>

#include "TestMacingMode.h"

using namespace hce::cards::desfire;

static constexpr unsigned int MAC_APP_ID    = 0x4D4143;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x81; // AES key type, 1 key
static constexpr unsigned int STD_FILE_ID   = 0x00;
static constexpr unsigned int BAK_FILE_ID   = 0x01;
static constexpr unsigned int VAL_FILE_ID   = 0x02;
static constexpr unsigned int LIN_FILE_ID   = 0x03;
static constexpr unsigned int FILE_SIZE     = 16;
static constexpr unsigned int RECORD_SIZE   = 8;
static constexpr unsigned int MAX_RECORDS   = 4;

void testMacingMode(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- macingMode (COMM_MACING with AES) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto aesKey    = rt::ByteBuffer::fromHex(MASTER_KEY_AES_HEX);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(MAC_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x4D4143, AES) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(MAC_APP_ID);
      ctx.check("selectApplication(0x4D4143) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("authenticateAES(key0) for file creation returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createStandardFile(STD_FILE_ID, COMM_MACING, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createStandardFile(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(BAK_FILE_ID, COMM_MACING, 0x0, 0x0, 0x0, 0x0, FILE_SIZE);
      ctx.check("createBackupFile(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createValueFile(VAL_FILE_ID, COMM_MACING, 0x0, 0x0, 0x0, 0x0, 0, 1000, 0, false);
      ctx.check("createValueFile(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createLinearRecordFile(LIN_FILE_ID, COMM_MACING, 0x0, 0x0, 0x0, 0x0, RECORD_SIZE, MAX_RECORDS);
      ctx.check("createLinearRecordFile(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: standard file COMM_MACING write/read");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for standard write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer wdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) wdata.putInt(0xA0 + i, 1);
      wdata.flip();

      sw = card.writeData(STD_FILE_ID, 0, FILE_SIZE, COMM_MACING, wdata);
      ctx.check("writeData(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Re-auth to reset session IV before read (write ACK MAC not processed by client)
      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for standard read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer rdata(FILE_SIZE);
      sw = card.readData(STD_FILE_ID, 0, FILE_SIZE, COMM_MACING, rdata);
      ctx.check("readData(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(COMM_MACING) returns FILE_SIZE bytes", rdata.remaining() == FILE_SIZE);

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
         ctx.check("readData(COMM_MACING) returns original data", match);
      }

      LOG_INFO(ctx.log, "  Test B: backup file COMM_MACING write/commit/read");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for backup write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer bdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) bdata.putInt(0xB0 + i, 1);
      bdata.flip();

      sw = card.writeData(BAK_FILE_ID, 0, FILE_SIZE, COMM_MACING, bdata);
      ctx.check("writeData(backupFile, COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after backup write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for backup read returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer brdata(FILE_SIZE);
      sw = card.readData(BAK_FILE_ID, 0, FILE_SIZE, COMM_MACING, brdata);
      ctx.check("readData(backupFile, COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readData(backupFile, COMM_MACING) returns FILE_SIZE bytes", brdata.remaining() == FILE_SIZE);

      if (brdata.remaining() == FILE_SIZE)
      {
         bool match = true;
         for (unsigned int i = 0; i < FILE_SIZE; i++)
         {
            if (brdata[i] != static_cast<unsigned char>(0xB0 + i))
            {
               match = false;
               break;
            }
         }
         ctx.check("readData(backupFile, COMM_MACING) matches written data", match);
      }

      LOG_INFO(ctx.log, "  Test C: value file COMM_MACING credit/commit/getValue");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for credit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.credit(VAL_FILE_ID, 500, COMM_MACING);
      ctx.check("credit(500, COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after credit returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for getValue returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      int value = -1;
      sw = card.getValue(VAL_FILE_ID, COMM_MACING, value);
      ctx.check("getValue(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("getValue(COMM_MACING) returns 500", value == 500);

      LOG_INFO(ctx.log, "  Test D: linear record file COMM_MACING writeRecord/commit/readRecords");

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for writeRecord returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer recData(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) recData.putInt(0xD0 + i, 1);
      recData.flip();

      sw = card.writeRecord(LIN_FILE_ID, 0, RECORD_SIZE, COMM_MACING, recData);
      ctx.check("writeRecord(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after writeRecord returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateAES(0, aesKey);
      ctx.check("re-authenticateAES for readRecords returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      rt::ByteBuffer readRec(RECORD_SIZE * MAX_RECORDS + 8);
      sw = card.readRecords(LIN_FILE_ID, 0, 1, COMM_MACING, readRec);
      ctx.check("readRecords(COMM_MACING) returns STATUS_OK", sw == STATUS_OK);
      ctx.check("readRecords(COMM_MACING) returns RECORD_SIZE bytes", readRec.remaining() == RECORD_SIZE);

      if (readRec.remaining() == RECORD_SIZE)
      {
         bool match = true;
         for (unsigned int i = 0; i < RECORD_SIZE; i++)
         {
            if (readRec.data()[readRec.position() + i] != static_cast<unsigned char>(0xD0 + i))
            {
               match = false;
               break;
            }
         }
         ctx.check("readRecords(COMM_MACING) returns original record data", match);
      }

      sw = card.deleteFile(LIN_FILE_ID);
      ctx.check("deleteFile(linearRecord) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(VAL_FILE_ID);
      ctx.check("deleteFile(value) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(BAK_FILE_ID);
      ctx.check("deleteFile(backup) returns STATUS_OK", sw == STATUS_OK);

      sw = card.deleteFile(STD_FILE_ID);
      ctx.check("deleteFile(standard) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
