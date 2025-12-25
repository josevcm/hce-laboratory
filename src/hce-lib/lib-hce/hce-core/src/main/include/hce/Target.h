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

#ifndef HCE_HANDLER_H
#define HCE_HANDLER_H

#include <rt/ByteBuffer.h>
#include <rt/Variant.h>

namespace hce {

class Target
{
   public:

      enum Param
      {
         PARAM_UID = 0, // tag UID (4, 7 or 10 bytes)
         PARAM_ATQA = 1, // 2 bytes, response to REQA / WUPA (example 0x4403)
         PARAM_SAK = 2, // 1 byte, response to SELECT (example 0x20 for ISO14443-4 compliant tags)
         PARAM_RATS_TB1 = 10, // byte TB1, WFT / SFGT (example 0x81 for FWT=77,33 ms, SFGT=0,60ms)
         PARAM_RATS_TC1 = 11, // byte TC1
         PARAM_RATS_HB = 12, // Historical bytes
      };

   public:

      virtual ~Target() = default;

      template <typename T>
      const T &get(int id)
      {
         return std::get<T>(get(id));
      }

      virtual rt::Variant get(int id);

      virtual bool set(int id, const rt::Variant &value);

      virtual void select();

      virtual void deselect();

      virtual int process(const rt::ByteBuffer &request, rt::ByteBuffer &response);
};

}

#endif //HCE_HANDLER_H
