#include <rt/ByteBuffer.h>

#include "TestGetFreeMemory.h"

using namespace hce::cards::desfire;

static constexpr unsigned int TEST_APP_ID    = 0x0F0F0F;
static constexpr unsigned int KEY_SETTINGS_1 = 0x0F;
static constexpr unsigned int KEY_SETTINGS_2 = 0x01;

static constexpr const char *APP_KEY_ZEROS = "00000000000000000000000000000000";

void testGetFreeMemory(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getFreeMemory ---");

   auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
   auto appKey    = rt::ByteBuffer::fromHex(APP_KEY_ZEROS);

   // Format card for a clean baseline
   int sw = card.selectApplication(0x000000);
   ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   sw = card.authenticateLegacy(0, masterKey);
   ctx.check("setup: authenticateLegacy(master key0) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   // Baseline free memory (fresh card)
   unsigned int baseline = 0;
   sw = card.getFreeMemory(baseline);
   ctx.check("getFreeMemory (baseline) returns STATUS_OK", sw == STATUS_OK);
   ctx.check("baseline free memory > 0", baseline > 0);
   LOG_INFO(ctx.log, "  baseline free memory: {} bytes", {baseline});

   // Create application
   sw = card.createApplication(TEST_APP_ID, KEY_SETTINGS_1, KEY_SETTINGS_2);
   ctx.check("createApplication(0x0F0F0F) returns STATUS_OK", sw == STATUS_OK);
   if (sw != STATUS_OK) return;

   [&] {
      sw = card.selectApplication(TEST_APP_ID);
      ctx.check("selectApplication(0x0F0F0F) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      sw = card.authenticateLegacy(0, appKey);
      ctx.check("authenticateLegacy(app key0) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      // Memory after app creation
      unsigned int mem0 = 0;
      sw = card.getFreeMemory(mem0);
      ctx.check("getFreeMemory after createApplication returns STATUS_OK", sw == STATUS_OK);
      ctx.check("free memory decreased after createApplication", mem0 < baseline);
      LOG_INFO(ctx.log, "  after createApplication: {} bytes", {mem0});

      // Create standard file 0 (256 bytes)
      sw = card.createStandardFile(0x00, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 256);
      ctx.check("createStandardFile(0, 256) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      unsigned int mem1 = 0;
      sw = card.getFreeMemory(mem1);
      ctx.check("getFreeMemory after file 0 returns STATUS_OK", sw == STATUS_OK);
      ctx.check("free memory decreased after createStandardFile(0)", mem1 < mem0);
      LOG_INFO(ctx.log, "  after createStandardFile(0): {} bytes", {mem1});

      // Create standard file 1 (256 bytes)
      sw = card.createStandardFile(0x01, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 256);
      ctx.check("createStandardFile(1, 256) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      unsigned int mem2 = 0;
      sw = card.getFreeMemory(mem2);
      ctx.check("getFreeMemory after file 1 returns STATUS_OK", sw == STATUS_OK);
      ctx.check("free memory decreased after createStandardFile(1)", mem2 < mem1);
      LOG_INFO(ctx.log, "  after createStandardFile(1): {} bytes", {mem2});

      // Create value file 2
      sw = card.createValueFile(0x02, COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, 0, 1000, 0, false);
      ctx.check("createValueFile(2) returns STATUS_OK", sw == STATUS_OK);
      if (sw != STATUS_OK) return;

      unsigned int mem3 = 0;
      sw = card.getFreeMemory(mem3);
      ctx.check("getFreeMemory after value file returns STATUS_OK", sw == STATUS_OK);
      ctx.check("free memory decreased after createValueFile(2)", mem3 < mem2);
      LOG_INFO(ctx.log, "  after createValueFile(2): {} bytes", {mem3});
   }();

   card.selectApplication(0x000000);
   card.authenticateLegacy(0, masterKey);
   sw = card.formatCard();
   ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}
