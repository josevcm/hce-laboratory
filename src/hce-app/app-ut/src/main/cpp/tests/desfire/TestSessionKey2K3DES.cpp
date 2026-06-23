/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

  HCE-LABORATORY is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HCE-LABORATORY is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with HCE-LABORATORY. If not, see <http://www.gnu.org/licenses/>.

*/

#include <rt/ByteBuffer.h>

#include "TestSessionKey2K3DES.h"

using namespace hce::cards::desfire;

// KS2 = 0x41: bits7:6=01 -> 3K3DES, bits3:0=1 key
static constexpr unsigned int SK_APP_ID  = 0x2D3E4F;
static constexpr unsigned int SK_KS1     = 0x0F;
static constexpr unsigned int SK_KS2     = 0x41;
static constexpr unsigned int SK_FILE_ID = 0x00;
static constexpr unsigned int SK_FILE_SZ = 16;

static constexpr const char *SK_APP_KEY =
    "000000000000000000000000000000000000000000000000";

void testSessionKey2K3DES(Desfire &card, TestContext &ctx)
{
    LOG_INFO(ctx.log, "\n--- sessionKey2K3DES: 3K3DES ISO auth session key construction ---");

    auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
    auto appKey    = rt::ByteBuffer::fromHex(SK_APP_KEY);
    int sw = 0;

    sw = card.selectApplication(0x000000);
    ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    sw = card.authenticateLegacy(0, masterKey);
    ctx.check("setup: authenticateLegacy(master) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    sw = card.createApplication(SK_APP_ID, SK_KS1, SK_KS2);
    ctx.check("setup: createApplication(3K3DES, 0x2D3E4F) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    [&] {
        sw = card.selectApplication(SK_APP_ID);
        ctx.check("selectApplication(3K3DES app) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateISO(0, appKey);
        ctx.check("authenticateISO(3K3DES zero key) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        ctx.check("authMode is AUTH_ISO after authenticateISO", card.authMode() == AUTH_ISO);

        sw = card.createStandardFile(SK_FILE_ID, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, SK_FILE_SZ);
        ctx.check("createStandardFile(COMM_CRYPT, ISO/3K3DES) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateISO(0, appKey);
        ctx.check("re-authenticateISO for write returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        rt::ByteBuffer wdata(SK_FILE_SZ);
        for (unsigned int i = 0; i < SK_FILE_SZ; i++) wdata.putInt(0xD0 + i, 1);
        wdata.flip();

        sw = card.writeData(SK_FILE_ID, 0, SK_FILE_SZ, COMM_CRYPT, wdata);
        ctx.check("writeData(COMM_CRYPT, ISO/3K3DES) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateISO(0, appKey);
        ctx.check("re-authenticateISO for read returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        rt::ByteBuffer rdata(SK_FILE_SZ);
        sw = card.readData(SK_FILE_ID, 0, SK_FILE_SZ, COMM_CRYPT, rdata);
        ctx.check("readData(COMM_CRYPT, ISO/3K3DES) returns STATUS_OK", sw == STATUS_OK);
        ctx.check("readData(COMM_CRYPT, ISO/3K3DES) returns SK_FILE_SZ bytes",
                  rdata.remaining() == SK_FILE_SZ);

        if (rdata.remaining() == SK_FILE_SZ)
        {
            bool match = true;
            for (unsigned int i = 0; i < SK_FILE_SZ; i++)
            {
                if (rdata[i] != static_cast<unsigned char>(0xD0 + i)) { match = false; break; }
            }
            ctx.check("readData(COMM_CRYPT, ISO/3K3DES) returns original data", match);
        }

        // Sub-test: 2K3DES app via authenticateISO
        sw = card.selectApplication(0x000000);
        ctx.check("selectApplication(master) for 2K3DES sub-test returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateLegacy(0, masterKey);
        ctx.check("authenticateLegacy(master) for 2K3DES sub-test returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        constexpr unsigned int SK2_APP_ID = 0x2D3E5F;
        sw = card.createApplication(SK2_APP_ID, 0x0F, 0x01); // KS2=0x01: DES, 1 key
        ctx.check("createApplication(2K3DES, 0x2D3E5F) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.selectApplication(SK2_APP_ID);
        ctx.check("selectApplication(2K3DES app) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        auto k2 = rt::ByteBuffer::fromHex("00000000000000000000000000000000");

        sw = card.authenticateISO(0, k2);
        ctx.check("authenticateISO(2K3DES zero key) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        ctx.check("authMode is AUTH_ISO (2K3DES path)", card.authMode() == AUTH_ISO);

        sw = card.createStandardFile(0x00, COMM_CRYPT, 0x0, 0x0, 0x0, 0x0, SK_FILE_SZ);
        ctx.check("createStandardFile(COMM_CRYPT, ISO/2K3DES) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateISO(0, k2);
        ctx.check("re-authenticateISO(2K3DES) for write returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        rt::ByteBuffer wd2(SK_FILE_SZ);
        for (unsigned int i = 0; i < SK_FILE_SZ; i++) wd2.putInt(0xE0 + i, 1);
        wd2.flip();

        sw = card.writeData(0x00, 0, SK_FILE_SZ, COMM_CRYPT, wd2);
        ctx.check("writeData(COMM_CRYPT, ISO/2K3DES) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateISO(0, k2);
        ctx.check("re-authenticateISO(2K3DES) for read returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        rt::ByteBuffer rd2(SK_FILE_SZ);
        sw = card.readData(0x00, 0, SK_FILE_SZ, COMM_CRYPT, rd2);
        ctx.check("readData(COMM_CRYPT, ISO/2K3DES) returns STATUS_OK", sw == STATUS_OK);
        ctx.check("readData(COMM_CRYPT, ISO/2K3DES) returns SK_FILE_SZ bytes",
                  rd2.remaining() == SK_FILE_SZ);

        if (rd2.remaining() == SK_FILE_SZ)
        {
            bool m2 = true;
            for (unsigned int i = 0; i < SK_FILE_SZ; i++)
            {
                if (rd2[i] != static_cast<unsigned char>(0xE0 + i)) { m2 = false; break; }
            }
            ctx.check("readData(COMM_CRYPT, ISO/2K3DES) returns original data", m2);
        }
    }();

    card.selectApplication(0x000000);
    card.authenticateLegacy(0, masterKey);
    sw = card.formatCard();
    ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}


