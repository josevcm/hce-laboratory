#include <rt/ByteBuffer.h>

#include "TestCyclicRecordWrap.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0C0D0E;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;
static constexpr unsigned int FILE_ID        = 0x00;
static constexpr unsigned int RECORD_SIZE    = 8;
// Spec: cyclic file reserves 1 extra slot for backup; specify N+1 to hold N useful records.
// With maxRecords=4 the file holds 3 usable records.
static constexpr unsigned int MAX_RECORDS    = 4;
static constexpr unsigned int USABLE_RECORDS = MAX_RECORDS - 1; // 3

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

static void writeAndCommit(Desfire &card, unsigned int fileId, unsigned char fill, TestContext &ctx)
{
   rt::ByteBuffer rec(RECORD_SIZE);
   for (unsigned int i = 0; i < RECORD_SIZE; i++) rec.putInt(fill, 1);
   rec.flip();

   int sw = card.writeRecord(fileId, 0, RECORD_SIZE, COMM_PLAIN, rec);
   ctx.check("writeRecord returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.commitTransaction();
   ctx.check("commitTransaction after writeRecord returns STATUS_OK", sw == STATUS_OK);
}

void testCyclicRecordWrap(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- cyclicRecordWrap ---");

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
   ctx.check("setup: createApplication(0x0C0D0E) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0C0D0E) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createCyclicRecordFile(FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, RECORD_SIZE, MAX_RECORDS);
      ctx.check("createCyclicRecordFile(recSize=8, maxRec=4) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Fill all 3 usable slots: A=0xAA, B=0xBB, C=0xCC
      writeAndCommit(card, FILE_ID, 0xAA, ctx);
      writeAndCommit(card, FILE_ID, 0xBB, ctx);
      writeAndCommit(card, FILE_ID, 0xCC, ctx);

      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("currentRecords is 3 after writing A/B/C", fs.currentRecords == USABLE_RECORDS);
      }

      // Write a 4th record D=0xDD — must wrap, overwriting A (oldest)
      writeAndCommit(card, FILE_ID, 0xDD, ctx);

      {
         FileSettings fs {};
         card.getFileSettings(FILE_ID, fs);
         ctx.check("currentRecords still 3 after wrap-around write", fs.currentRecords == USABLE_RECORDS);
      }

      // Read back 3 records; expected order oldest→newest: B, C, D
      {
         rt::ByteBuffer buf(RECORD_SIZE * USABLE_RECORDS + 16);
         sw = card.readRecords(FILE_ID, 0, USABLE_RECORDS, COMM_PLAIN, buf);
         ctx.check("readRecords after wrap returns STATUS_OK", sw == STATUS_OK);
         ctx.check("readRecords returns 3 records (24 bytes)", buf.remaining() == RECORD_SIZE * USABLE_RECORDS);

         if (buf.remaining() == RECORD_SIZE * USABLE_RECORDS)
         {
            const unsigned char *p = buf.data() + buf.position();

            bool r0ok = true, r1ok = true, r2ok = true;
            for (unsigned int i = 0; i < RECORD_SIZE; i++)
            {
               if (p[i]                      != 0xBB) r0ok = false;
               if (p[RECORD_SIZE + i]        != 0xCC) r1ok = false;
               if (p[2 * RECORD_SIZE + i]    != 0xDD) r2ok = false;
            }
            ctx.check("oldest record is B=0xBB (A was overwritten)", r0ok);
            ctx.check("middle record is C=0xCC", r1ok);
            ctx.check("newest record is D=0xDD", r2ok);
         }
      }

      sw = card.deleteFile(FILE_ID);
      ctx.check("deleteFile returns STATUS_OK", sw == STATUS_OK);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
