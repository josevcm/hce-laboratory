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

#ifndef HCE_DESFIRE_ISO_AUTHENTICATE_H
#define HCE_DESFIRE_ISO_AUTHENTICATE_H

#include <rt/Logger.h>

#include "Command.h"
#include "Instance.h"

namespace hce::targets {

class DesfireIsoAuthentication final : public Command
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.desfire.DesfireIsoAuthenticate");

   public:

      explicit DesfireIsoAuthentication(Instance &bundle);

      int process(rt::ByteBuffer &request, rt::ByteBuffer &response);

   private:

      int getChallenge(rt::ByteBuffer &request, rt::ByteBuffer &response);
      int externalAuth(rt::ByteBuffer &request, rt::ByteBuffer &response);
      int internalAuth(rt::ByteBuffer &request, rt::ByteBuffer &response);

   private:

      int status;
      int algorithm;
      int secret;

      Authentication auth;

      rt::ByteBuffer rpicc1;
      rt::ByteBuffer rpicc2;

      rt::ByteBuffer rpcd1;
      rt::ByteBuffer rpcd2;
};

}

#endif //HCE_DESFIRE_ISO_AUTHENTICATE_H
