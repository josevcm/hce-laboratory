#include <rt/ByteBuffer.h>

#include "TestCyclicRecordFile.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID = 0x010306;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int CYCLIC_FILE_ID = 0x01;
static constexpr unsigned int RECORD_SIZE = 8;
static constexpr unsigned int MAX_RECORDS = 4;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testCyclicRecordFile(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- cyclicRecord ---");

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

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x010306) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x010306) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0, zeros) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Create cyclic record file
      sw = card.createCyclicRecordFile(CYCLIC_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, RECORD_SIZE, MAX_RECORDS);
      ctx.check("createCyclicRecordFile(id=1, recSize=8, maxRec=4) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Verify settings
      {
         FileSettings fs {};
         sw = card.getFileSettings(CYCLIC_FILE_ID, fs);
         ctx.check("getFileSettings(1) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("fileType is FILE_CYCLIC_RECORD", fs.fileType == FILE_CYCLIC_RECORD);
         ctx.check("recordSize is 8", fs.recordSize == RECORD_SIZE);
      }

      // Write 3 records (each needs its own commit — DESFire adds one record per transaction)
      for (unsigned int r = 0; r < 3; r++)
      {
         rt::ByteBuffer rec(RECORD_SIZE);
         for (unsigned int i = 0; i < RECORD_SIZE; i++) rec.putInt(0x10 * (r + 1) + i, 1);
         rec.flip();

         sw = card.writeRecord(CYCLIC_FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec);
         ctx.check("writeRecord to cyclic file returns STATUS_OK", sw == STATUS_OK);
         if (sw != STATUS_OK) return;

         sw = card.commitTransaction();
         ctx.check("commitTransaction after writeRecord returns STATUS_OK", sw == STATUS_OK);
         if (sw != STATUS_OK) return;
      }

      // Verify record count
      {
         FileSettings fs {};
         card.getFileSettings(CYCLIC_FILE_ID, fs);
         ctx.check("currentRecords is 3 after 3 writes", fs.currentRecords == 3);
         LOG_INFO(ctx.log, "  cyclic currentRecords: {}", {fs.currentRecords});
      }

      // Delete file
      sw = card.deleteFile(CYCLIC_FILE_ID);
      ctx.check("deleteFile(cyclicRecord) returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
