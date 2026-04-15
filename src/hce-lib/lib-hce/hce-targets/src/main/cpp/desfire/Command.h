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

#ifndef HCE_DESFIRE_COMMAND_H
#define HCE_DESFIRE_COMMAND_H

#include <rt/ByteBuffer.h>
#include <rt/Logger.h>

#define DESFIRE_CLA_ISO                       0x00
#define DESFIRE_CLA_WRAPPED                   0x90
#define DESFIRE_SW1                           0x91

// --- MF3 IC D40 Security Related Commands ---
#define DESFIRE_CMD_AUTHENTICATE              0x0A
#define DESFIRE_CMD_AUTHENTICATE_ISO          0x1A
#define DESFIRE_CMD_AUTHENTICATE_AES          0xAA
#define DESFIRE_CMD_CHANGE_KEY_SETTINGS       0x54
#define DESFIRE_CMD_GET_KEY_SETTINGS          0x45
#define DESFIRE_CMD_CHANGE_KEY                0xC4
#define DESFIRE_CMD_GET_KEY_VERSION           0x64
#define DESFIRE_CMD_SET_CONFIGURATION         0x5C

// --- MF3 IC D40 PICC Level Commands ---
#define DESFIRE_CMD_CREATE_APPLICATION        0xCA
#define DESFIRE_CMD_DELETE_APPLICATION        0xDA
#define DESFIRE_CMD_GET_APPLICATION_IDS       0x6A
#define DESFIRE_CMD_GET_DF_NAMES              0x6D
#define DESFIRE_CMD_GET_FREE_MEMORY           0x6E
#define DESFIRE_CMD_SELECT_APPLICATION        0x5A
#define DESFIRE_CMD_FORMAT_PICC               0xFC
#define DESFIRE_CMD_GET_VERSION               0x60

// --- MF3 IC D40 Application Level Commands ---
#define DESFIRE_CMD_GET_FILE_IDS              0x6F
#define DESFIRE_CMD_GET_FILE_SETTINGS         0xF5
#define DESFIRE_CMD_CHANGE_FILE_SETTINGS      0x5F
#define DESFIRE_CMD_CREATE_STD_DATA_FILE      0xCD
#define DESFIRE_CMD_CREATE_BACKUP_DATA_FILE   0xCB
#define DESFIRE_CMD_CREATE_VALUE_FILE         0xCC
#define DESFIRE_CMD_CREATE_LINEAR_RECORD_FILE 0xC1
#define DESFIRE_CMD_CREATE_CYCLIC_RECORD_FILE 0xC0
#define DESFIRE_CMD_DELETE_FILE               0xDF

// --- MF3 IC D40 Data Manipulation Commands ---
#define DESFIRE_CMD_READ_DATA                 0xBD
#define DESFIRE_CMD_WRITE_DATA                0x3D
#define DESFIRE_CMD_GET_VALUE                 0x6C
#define DESFIRE_CMD_CREDIT                    0x0C
#define DESFIRE_CMD_DEBIT                     0xDC
#define DESFIRE_CMD_LIMITED_CREDIT            0x1C
#define DESFIRE_CMD_WRITE_RECORD              0x3B
#define DESFIRE_CMD_READ_RECORDS              0xBB
#define DESFIRE_CMD_CLEAR_RECORD_FILE         0xEB
#define DESFIRE_CMD_COMMIT_TRANSACTION        0xC7
#define DESFIRE_CMD_ABORT_TRANSACTION         0xA7

// --- ISO7186-4 Commands ---
#define DESFIRE_CMD_ISO_SELECT_FILE           0xA4
#define DESFIRE_CMD_ISO_READ_BINARY           0xB0
#define DESFIRE_CMD_ISO_UPDATE_BINARY         0xD6
#define DESFIRE_CMD_ISO_READ_RECORDS          0xB2
#define DESFIRE_CMD_ISO_UPDATE_RECORD         0xD2
#define DESFIRE_CMD_ISO_APPEND_RECORD         0xE2
#define DESFIRE_CMD_ISO_GET_CHALLENGE         0x84
#define DESFIRE_CMD_ISO_EXTERNAL_AUTHENTICATE 0x82
#define DESFIRE_CMD_ISO_INTERNAL_AUTHENTICATE 0x88

namespace hce::targets {

struct Instance;
struct FileEntry;

class Command
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.desfire.Desfire");

   public:

      explicit Command(Instance &instance);

      virtual ~Command() = default;

   protected:

      Instance &picc;
};

}

#endif //HCE_DESFIRE_COMMAND_H
