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
#ifndef HCE_MIFAREPLUS_COMMAND_H
#define HCE_MIFAREPLUS_COMMAND_H

#include <rt/ByteBuffer.h>
#include <rt/Logger.h>

#define MFPLUS_CMD_FIRST_AUTH_P1       0x70
#define MFPLUS_CMD_FIRST_AUTH_P2       0x72
#define MFPLUS_CMD_FOLLOWING_AUTH_P1   0x76
#define MFPLUS_CMD_FOLLOWING_AUTH_P2   0x77
#define MFPLUS_CMD_RESET_AUTH          0x78
#define MFPLUS_CMD_READ                0x30
#define MFPLUS_CMD_READ_ENCRYPTED      0x31
#define MFPLUS_CMD_INCREMENT_TRANSFER  0x35
#define MFPLUS_CMD_DECREMENT_TRANSFER  0x36
#define MFPLUS_CMD_RESTORE_TRANSFER    0x37
#define MFPLUS_CMD_WRITE               0xA0
#define MFPLUS_CMD_WRITE_ENCRYPTED     0xA1
#define MFPLUS_CMD_TRANSFER            0xB0
#define MFPLUS_CMD_INCREMENT           0xC0
#define MFPLUS_CMD_DECREMENT           0xC1
#define MFPLUS_CMD_RESTORE             0xC2
#define MFPLUS_CMD_GET_UID             0x56

namespace hce::targets::mifareplus {

struct Instance;

class Command
{
   protected:

      rt::Logger *log = rt::Logger::getLogger("hce.targets.mifareplus.Command");
      Instance &picc;

   public:

      explicit Command(Instance &instance);

      virtual ~Command() = default;

      virtual int process(rt::ByteBuffer &request, rt::ByteBuffer &response) = 0;
};

} // namespace hce::targets::mifareplus

#endif // HCE_MIFAREPLUS_COMMAND_H
