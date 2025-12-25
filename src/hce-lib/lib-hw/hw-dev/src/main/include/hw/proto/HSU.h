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

#ifndef DEV_HSU_H
#define DEV_HSU_H

#include <string>

#include <rt/ByteBuffer.h>

namespace hw {

class HSU
{

   struct Impl;

   public:

      HSU();

      int open(const std::string &device, const std::string &config);

      void close();

      int transmit(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout);

   private:

      std::shared_ptr<Impl> impl;

};

}

#endif // DEV_HSU_H
