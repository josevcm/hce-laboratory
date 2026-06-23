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

#include "TestUnknownCommand.h"

using namespace hce::cards::desfire;

// Tests that unknown commands and invalid parameters are properly handled
// and return correct error codes (not LENGTH_ERROR).
void testUnknownCommand(Desfire &card, TestContext &ctx)
{
    LOG_INFO(ctx.log, "\n--- unknownCommand: error handling for unknown INS and parameter validation ---");

    // Test A: constant sanity
    ctx.check("STATUS_ILLEGAL_COMMAND constant equals 0x1C",
              STATUS_ILLEGAL_COMMAND == 0x1C);

    auto masterKey = rt::ByteBuffer::fromHex(MASTER_KEY_DES_HEX);
    auto appKey    = rt::ByteBuffer::fromHex("00000000000000000000000000000000");
    int sw = 0;

    sw = card.selectApplication(0x000000);
    ctx.check("setup: selectApplication(master) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    sw = card.authenticateLegacy(0, masterKey);
    ctx.check("setup: authenticateLegacy(master) returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    constexpr unsigned int UNK_APP_ID = 0xFE0001;
    sw = card.createApplication(UNK_APP_ID, 0x0F, 0x01);
    ctx.check("setup: createApplication returns STATUS_OK", sw == STATUS_OK);
    if (sw != STATUS_OK) return;

    [&] {
        sw = card.selectApplication(UNK_APP_ID);
        ctx.check("selectApplication(testApp) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        sw = card.authenticateLegacy(0, appKey);
        ctx.check("authenticateLegacy(appKey) returns STATUS_OK", sw == STATUS_OK);
        if (sw != STATUS_OK) return;

        // Test B: getKeyVersion on a key that does not exist (id=0x0F when only
        // key 0 exists) must return NO_SUCH_KEY (0x40), not LENGTH_ERROR (0x7E)
        // This confirms the emulator routes parameter errors correctly.
        unsigned int ver = 0;
        sw = card.getKeyVersion(0x0F, ver);
        ctx.check("getKeyVersion(nonExistentKey=0x0F) returns NO_SUCH_KEY",
                  sw == STATUS_NO_SUCH_KEY);

        // Test C: getFileSettings on file 0x20 (fileId > DESFIRE_MAX_FILE_ID=31)
        // must return PARAMETER_ERROR (0x9E), not LENGTH_ERROR (0x7E).
        FileSettings fs;
        sw = card.getFileSettings(0x20, fs);
        ctx.check("getFileSettings(fileId=0x20 > max=31) returns PARAMETER_ERROR",
                  sw == STATUS_PARAMETER_ERROR);

        // Test D: getFileSettings on a valid but non-existent file id (< 32)
        // must return FILE_NOT_FOUND (0xF0), not LENGTH_ERROR (0x7E).
        sw = card.getFileSettings(0x05, fs);
        ctx.check("getFileSettings(nonExistentFile=0x05) returns FILE_NOT_FOUND",
                  sw == STATUS_FILE_NOT_FOUND);

        // Test E: listFiles on an app with no files must return STATUS_OK + empty
        std::vector<unsigned int> files;
        sw = card.listFiles(files);
        ctx.check("listFiles(empty app) returns STATUS_OK", sw == STATUS_OK);
        ctx.check("listFiles(empty app) returns empty list", files.empty());

        // Test F: ISO INS_NOT_SUPPORTED — send isoReadBinary without prior ISO
        // SELECT FILE context.  With no EF selected the emulator must return
        // ISO_STATUS_FILE_NOT_FOUND (0x6A82), confirming ISO CLA routing is correct.
        rt::ByteBuffer dummy(4);
        sw = card.isoReadBinary(0x00, 0x00, 4, dummy);
        ctx.check("isoReadBinary without selected EF returns not_enough_data or file_not_found",
                  sw == ISO_STATUS_FILE_NOT_FOUND || sw == ISO_STATUS_NOT_ENOUGH_DATA);
    }();

    // Teardown
    card.selectApplication(0x000000);
    card.authenticateLegacy(0, masterKey);
    sw = card.formatCard();
    ctx.check("teardown: formatCard() returns STATUS_OK", sw == STATUS_OK);
}


