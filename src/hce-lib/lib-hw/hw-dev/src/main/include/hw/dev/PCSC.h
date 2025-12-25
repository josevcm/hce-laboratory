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

#ifndef DEV_PCSC_H
#define DEV_PCSC_H

#include <vector>
#include <memory>

#include <rt/ByteBuffer.h>

namespace hw {

class PCSC
{
   struct Impl;

   public:

      enum Mode
      {
         Direct, Shared, Exclusive
      };

      enum Protocol
      {
         None = 0,
         T0 = 1,
         T1 = 2,
         Any = 3
      };

      enum IOCTL
      {
         IOCTL_CCID_ESCAPE = 1
      };

   public:

      PCSC();

      std::vector<std::string> listReaders() const;

      int connect(const std::string &reader, Mode mode = Shared, Protocol protocol = Any);

      int disconnect();

      int transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &resp);

      int control(int controlCode, const rt::ByteBuffer &cmd, rt::ByteBuffer &resp);

   private:

      std::shared_ptr<Impl> impl;
};

}

#endif //DEV_PCSC_H
