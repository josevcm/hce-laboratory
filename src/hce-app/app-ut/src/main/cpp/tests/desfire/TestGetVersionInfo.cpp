#include <rt/ByteBuffer.h>

#include "TestGetVersionInfo.h"

using namespace hce::cards::desfire;

void testGetVersionInfo(Desfire &card, TestContext &ctx)
{
   LOG_INFO(ctx.log, "\n--- getVersionInfo ---");

   VersionInfo info;
   int sw = card.getVersionInfo(info);

   ctx.check("returns STATUS_OK (0x9100)", sw == STATUS_OK);
   ctx.check("HW vendor is NXP (0x04)", info.hwVendorId == 0x04);
   ctx.check("HW type is DESFire (0x01)", info.hwType == 0x01);
   ctx.check("SW vendor is NXP (0x04)", info.swVendorId == 0x04);
   ctx.check("SW type is DESFire (0x01)", info.swType == 0x01);
   ctx.check("UID length is 7 bytes", info.uid.remaining() == 7);
   ctx.check("Batch length is 5 bytes", info.batch.remaining() == 5);
   ctx.check("Production year >= 2000", info.productionYear >= 2000);
   ctx.check("Production week in [1..53]", info.productionWeek >= 1 && info.productionWeek <= 53);

   LOG_INFO(ctx.log, "\n  UID        : {}", {rt::ByteBuffer::toHex(info.uid)});
   LOG_INFO(ctx.log, "  Batch      : {}", {rt::ByteBuffer::toHex(info.batch)});
   LOG_INFO(ctx.log, "  Produced   : week {}, {}", {info.productionWeek, info.productionYear});
   LOG_INFO(ctx.log, "  HW version : {}.{}  storage=0x{02x}  protocol=0x{02x}", {info.hwMajorVersion, info.hwMinorVersion, info.hwStorageSize, info.hwProtocol});
   LOG_INFO(ctx.log, "  SW version : {}.{}  storage=0x{02x}  protocol=0x{02x}", {info.swMajorVersion, info.swMinorVersion, info.swStorageSize, info.swProtocol});
}
