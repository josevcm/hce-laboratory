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
#include "TestIsoSFI.h"

using namespace hce::cards::desfire;

// App uses ISO enabled DF naming so isoSelectByName works
static constexpr unsigned int SFI_APP_AID    = 0x5F1001;
static constexpr unsigned int SFI_APP_KS1    = 0x0F;
static constexpr unsigned int SFI_APP_KS2    = 0x21;   // bit5=ISO enabled, DES, 1 key
static constexpr unsigned int SFI_APP_ISO_ID = 0xAB00;
static constexpr const char  *SFI_APP_NAME   = "D2760000850102";

// File isoId = 0xAB02:  SFI = isoId & 0x1F = 0x02
static constexpr unsigned int SFI_FILE_ID    = 0x02;
static constexpr unsigned int SFI_FILE_ISO   = 0xAB02;
static constexpr unsigned int SFI_FILE_SIZE  = 16;
static constexpr unsigned int SFI             = SFI_FILE_ISO & 0x1F; // = 0x02

void testIsoSFI(Desfire &card, TestContext &ctx)
{
    LOG_INFO(ctx.log, "\n--- isoSFI: IsoReadBinary/UpdateBinary with SFI (P1 bit7=1) ---");

    auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
    auto appKey    = rt::ByteBuffer::fromHex("00000000000000000000000000000000");
    int sw = 0;

    // --- Setup ---
    sw = card.selectApplication(0x000000);
    ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    sw = card.authenticateLegacy(0, masterKey);
    ctx.check("setup: authenticateLegacy returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    auto isoName = rt::ByteBuffer::fromHex(SFI_APP_NAME);
    sw = card.createApplicationIso(SFI_APP_AID, SFI_APP_KS1, SFI_APP_KS2,
                                   SFI_APP_ISO_ID, isoName);
    ctx.check("createApplicationIso(SFI app) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    [&] {
        sw = card.selectApplication(SFI_APP_AID);
        ctx.check("selectApplication(ISO app) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateLegacy(0, appKey);
        ctx.check("authenticateLegacy(app key) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        // Create file whose isoId encodes SFI = 0x02
        sw = card.createStandardFileIso(SFI_FILE_ID, SFI_FILE_ISO,
                                        COMM_PLAIN, 0xE, 0xE, 0xE, 0xE, SFI_FILE_SIZE);
        ctx.check("createStandardFileIso(SFI=2) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        // Write a known pattern via native WriteData
        sw = card.authenticateLegacy(0, appKey);
        ctx.check("re-auth before native write returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        rt::ByteBuffer wdata(SFI_FILE_SIZE);
        for (unsigned int i = 0; i < SFI_FILE_SIZE; i++) wdata.putInt(0xC0 + i, 1);
        wdata.flip();

        sw = card.writeData(SFI_FILE_ID, 0, SFI_FILE_SIZE, COMM_PLAIN, wdata);
        ctx.check("writeData(native, PLAIN) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        // Enter ISO context and select the DF by name
        sw = card.selectApplication(0x000000);
        ctx.check("re-selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.isoSelectByName(isoName);
        ctx.check("isoSelectByName returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
        if (sw != ISO_STATUS_OK) return;

        // Test A: IsoReadBinary with SFI (P1 = 0x80 | SFI, P2 = offset 0)
        rt::ByteBuffer rdata(SFI_FILE_SIZE);
        sw = card.isoReadBinary(0x80 | SFI, 0x00, SFI_FILE_SIZE, rdata);
        ctx.check("isoReadBinary(SFI) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
        ctx.check("isoReadBinary(SFI) returns SFI_FILE_SIZE bytes",
                  rdata.remaining() == SFI_FILE_SIZE);

        if (rdata.remaining() == SFI_FILE_SIZE)
        {
            bool match = true;
            for (unsigned int i = 0; i < SFI_FILE_SIZE; i++)
            {
                if (rdata[i] != static_cast<unsigned char>(0xC0 + i))
                { match = false; break; }
            }
            ctx.check("isoReadBinary(SFI) data matches written pattern", match);
        }

        // Test B: IsoUpdateBinary with SFI
        const rt::ByteBuffer updata = {0x11, 0x22, 0x33, 0x44};
        sw = card.isoUpdateBinary(0x80 | SFI, 0x00, updata);
        ctx.check("isoUpdateBinary(SFI) returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
        if (sw != ISO_STATUS_OK) return;

        // Verify the update via another SFI read
        rt::ByteBuffer rb(4);
        sw = card.isoReadBinary(0x80 | SFI, 0x00, 4, rb);
        ctx.check("isoReadBinary(SFI) after update returns ISO_STATUS_OK", sw == ISO_STATUS_OK);
        ctx.check("isoReadBinary(SFI) after update returns 4 bytes", rb.remaining() == 4);
        if (rb.remaining() == 4)
        {
            ctx.check("isoReadBinary(SFI) returns updated data",
                      rb[0] == 0x11 && rb[1] == 0x22 && rb[2] == 0x33 && rb[3] == 0x44);
        }

        // Test C: IsoReadBinary with SFI for a non-existent SFI must fail
        rt::ByteBuffer dummy(4);
        sw = card.isoReadBinary(0x80 | 0x1F, 0x00, 4, dummy); // SFI=31, not created
        ctx.check("isoReadBinary(SFI=31, not found) returns file-not-found",
                  sw == ISO_STATUS_FILE_NOT_FOUND || sw == ISO_STATUS_WRONG_PARAMETERS_P1P2);
    }();

    // --- Teardown ---
    card.selectApplication(0x000000);
    card.authenticateLegacy(0, masterKey);
    sw = card.formatCard();
    ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}


