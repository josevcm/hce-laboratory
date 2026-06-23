#include <rt/ByteBuffer.h>

#include "TestLinearRecordFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x010305;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int LINEAR_FILE_ID = 0x00;
static constexpr unsigned int CYCLIC_FILE_ID = 0x01;
static constexpr unsigned int RECORD_SIZE = 8;
static constexpr unsigned int MAX_RECORDS = 4;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testLinearRecordFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- linearRecord ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // --- Setup ---
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x010305) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x010305) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create linear record file
      sw = card.createLinearRecordFile(LINEAR_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, RECORD_SIZE, MAX_RECORDS);
      ctx.check("createLinearRecordFile(id=0, recSize=8, maxRec=4) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify settings
      {
         FileSettings fs {};
         sw = card.getFileSettings(LINEAR_FILE_ID, fs);
         ctx.check("getFileSettings(0) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_LINEAR_RECORD", fs.fileType == FILE_LINEAR_RECORD);
         ctx.check("recordSize is 8", fs.recordSize == RECORD_SIZE);
         ctx.check("maxRecords is 4", fs.maxRecords == MAX_RECORDS);
         ctx.check("currentRecords is 0", fs.currentRecords == 0);
      }

      // Write 2 records
      rt::ByteBuffer rec1(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) rec1.putInt(0x11 + i, 1);
      rec1.flip();

      rt::ByteBuffer rec2(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) rec2.putInt(0x21 + i, 1);
      rec2.flip();

      sw = card.writeRecord(LINEAR_FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec1);
      ctx.check("writeRecord(0, rec1) returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction after rec1 returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.writeRecord(LINEAR_FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec2);
      ctx.check("writeRecord(0, rec2) returns STATUS_OK", sw == STATUS_OK);

      sw = card.commitTransaction();
      ctx.check("commitTransaction after rec2 returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify record count updated
      {
         FileSettings fs {};
         card.getFileSettings(LINEAR_FILE_ID, fs);
         ctx.check("currentRecords is 2 after 2 writes", fs.currentRecords == 2);
      }

      // Read all records (offset=0, count=2)
      {
         rt::ByteBuffer readData(RECORD_SIZE * MAX_RECORDS + 8);
         sw = card.readRecords(LINEAR_FILE_ID, 0, 2, COMM_PLAIN, readData);
         ctx.check("readRecords(0, 0, 2) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("readRecords returns 16 bytes", readData.remaining() == RECORD_SIZE * 2);

         if (readData.remaining() == RECORD_SIZE * 2)
         {
            bool r1ok = true, r2ok = true;

            for (unsigned int i = 0; i < RECORD_SIZE; i++)
            {
               if (readData.data()[readData.position() + i] != static_cast<unsigned char>(0x11 + i))
               {
                  r1ok = false;
                  break;
               }
            }

            for (unsigned int i = 0; i < RECORD_SIZE; i++)
            {
               if (readData.data()[readData.position() + RECORD_SIZE + i] != static_cast<unsigned char>(0x21 + i))
               {
                  r2ok = false;
                  break;
               }
            }

            ctx.check("record 1 content matches", r1ok);
            ctx.check("record 2 content matches", r2ok);
         }
      }

      // Clear records
      sw = card.clearRecords(LINEAR_FILE_ID);
      ctx.check("clearRecords(0) returns STATUS_OK", sw == STATUS_OK);

      if (sw == STATUS_OK)
      {
         sw = card.commitTransaction();
         ctx.check("commitTransaction after clearRecords returns STATUS_OK", sw == STATUS_OK);

         FileSettings fs {};
         card.getFileSettings(LINEAR_FILE_ID, fs);
         ctx.check("currentRecords is 0 after clear", fs.currentRecords == 0);
      }

      // Delete file
      sw = card.deleteFile(LINEAR_FILE_ID);
      ctx.check("deleteFile(linearRecord) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
