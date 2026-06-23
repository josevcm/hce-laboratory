#include <rt/ByteBuffer.h>

#include "TestClearRecordBehavior.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x050607;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;
static constexpr unsigned int RECORD_SIZE    = 8;
static constexpr unsigned int MAX_RECORDS    = 4;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testClearRecordBehavior(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- clearRecordBehavior (commit/abort semantics) ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);
   int sw = 0;

   // --- Setup ---
   sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x050607) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x050607) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createLinearRecordFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, RECORD_SIZE, MAX_RECORDS);
      ctx.check("createLinearRecordFile returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Write 2 records and commit — baseline state
      rt::ByteBuffer rec1(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) rec1.putInt(0x11 + i, 1);
      rec1.flip();

      rt::ByteBuffer rec2(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) rec2.putInt(0x21 + i, 1);
      rec2.flip();

      card.writeRecord(FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec1);
      card.commitTransaction();
      card.writeRecord(FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec2);
      card.commitTransaction();

      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("baseline: currentRecords is 2", fs.currentRecords == 2);
      }

      LOG_INFO(ctx.log, "  Test A: clearRecords + abort restores records");

      // Test A: clearRecords + abortTransaction must leave records intact
      sw = card.clearRecords(FILE_ID);
      ctx.check("clearRecords returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Before commit, old records must still be readable
      {
         rt::ByteBuffer buf(RECORD_SIZE * MAX_RECORDS + 16);
         sw = card.readRecords(FILE_ID, 0, 2, COMM_PLAIN, buf);
         ctx.check("readRecords after clearRecords (before commit) returns STATUS_OK", sw == STATUS_OK);
         ctx.check("readRecords returns 2 records before commit", buf.remaining() == RECORD_SIZE * 2);
      }

      sw = card.abortTransaction();
      ctx.check("abortTransaction returns STATUS_OK", sw == STATUS_OK);

      // After abort, records must still be intact
      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("currentRecords still 2 after clearRecords + abort", fs.currentRecords == 2);
      }

      {
         rt::ByteBuffer buf(RECORD_SIZE * MAX_RECORDS + 16);
         sw = card.readRecords(FILE_ID, 0, 2, COMM_PLAIN, buf);
         ctx.check("readRecords after abort returns STATUS_OK", sw == STATUS_OK);
         ctx.check("readRecords returns 2 records after abort", buf.remaining() == RECORD_SIZE * 2);
      }

      LOG_INFO(ctx.log, "  Test B: clearRecords + commit empties the file");

      // Test B: clearRecords + commitTransaction must empty the file
      sw = card.clearRecords(FILE_ID);
      ctx.check("clearRecords (for commit test) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.commitTransaction();
      ctx.check("commitTransaction after clearRecords returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // After commit, file must be empty
      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("currentRecords is 0 after clearRecords + commit", fs.currentRecords == 0);
      }

      // New records can be written to the cleared file
      rt::ByteBuffer rec3(RECORD_SIZE);
      for (unsigned int i = 0; i < RECORD_SIZE; i++) rec3.putInt(0x33, 1);
      rec3.flip();

      sw = card.writeRecord(FILE_ID, 0, RECORD_SIZE, COMM_PLAIN, rec3);
      ctx.check("writeRecord to cleared file returns STATUS_OK", sw == STATUS_OK);
      if (sw == STATUS_OK)
      {
         sw = card.commitTransaction();
         ctx.check("commitTransaction after new writeRecord returns STATUS_OK", sw == STATUS_OK);

         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("currentRecords is 1 after writing to cleared file", fs.currentRecords == 1);
      }

      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
