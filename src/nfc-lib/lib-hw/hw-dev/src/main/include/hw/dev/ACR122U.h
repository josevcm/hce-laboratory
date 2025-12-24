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

#ifndef DEV_ACR122U_H
#define DEV_ACR122U_H

#include <string>
#include <memory>

#include <rt/ByteBuffer.h>

#include <hw/dev/PCSC.h>

namespace hw {

class ACR122U
{
      struct Impl;

   public:

      // constructor
      ACR122U();

      // connect to card reader, if no reader specified, connect to first available
      int open(PCSC::Mode = PCSC::Shared, const std::string &reader = std::string());

      // close connection to card reader
      void close();

      // send direct command to reader and receive response
      int transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout);

      // configure operation parameters
      int setParameters(int value);

   private:

      std::shared_ptr<Impl> impl;
};

}

#endif //DEV_ACR122U_H
