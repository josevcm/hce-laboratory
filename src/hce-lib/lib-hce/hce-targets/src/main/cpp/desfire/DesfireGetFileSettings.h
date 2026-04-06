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

#ifndef HCE_DESFIRE_GET_FILE_SETTINGS_H
#define HCE_DESFIRE_GET_FILE_SETTINGS_H

#include <rt/Logger.h>

#include "Command.h"

namespace hce::targets {

class DesfireGetFileSettings final : public Command
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.desfire.GetFileSettings");

   public:

      explicit DesfireGetFileSettings(Instance &bundle);

      int process(rt::ByteBuffer &request, rt::ByteBuffer &response);
};

}

#endif //HCE_DESFIRE_GETFILESETTINGS_H
