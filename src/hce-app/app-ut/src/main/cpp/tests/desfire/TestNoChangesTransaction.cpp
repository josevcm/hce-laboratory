#include <rt/ByteBuffer.h>

#include "TestNoChangesTransaction.h"

using namespace hce::cards::desfire;

static constexpr unsigned int NOCH_APP_ID    = 0x5A5B5C;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01; // DES/2K3DES, 1 key
static constexpr unsigned int BAK_FILE_ID    = 0x00;
static constexpr unsigned int FILE_SIZE      = 16;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testNoChangesTransaction(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- noChangesTransaction (STATUS_NO_CHANGES semantics) ---");

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

   sw = card.createApplication(NOCH_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("setup: createApplication(0x5A5B5C) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(NOCH_APP_ID);
      ctx.check("selectApplication(0x5A5B5C) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.createBackupFile(BAK_FILE_ID, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, FILE_SIZE);
      ctx.check("createBackupFile returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      LOG_INFO(ctx.log, "  Test A: commitTransaction with no pending changes");

      sw = card.commitTransaction();
      ctx.check("commitTransaction with no changes returns STATUS_NO_CHANGES", sw == STATUS_NO_CHANGES);

      LOG_INFO(ctx.log, "  Test B: abortTransaction with no pending changes");

      sw = card.abortTransaction();
      ctx.check("abortTransaction with no changes returns STATUS_NO_CHANGES", sw == STATUS_NO_CHANGES);

      LOG_INFO(ctx.log, "  Test C: commit after abort yields STATUS_NO_CHANGES");

      rt::ByteBuffer wdata(FILE_SIZE);
      for (unsigned int i = 0; i < FILE_SIZE; i++) wdata.putInt(0x55, 1);
      wdata.flip();

      sw = card.writeData(BAK_FILE_ID, 0, FILE_SIZE, COMM_PLAIN, wdata);
      ctx.check("writeData before abort returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.abortTransaction();
      ctx.check("abortTransaction after write returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // After abort, no pending changes remain — next commit must return NO_CHANGES
      sw = card.commitTransaction();
      ctx.check("commitTransaction after abort+no-write returns STATUS_NO_CHANGES", sw == STATUS_NO_CHANGES);
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
