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

#ifndef HCE_DESFIRE_H
#define HCE_DESFIRE_H

#include <hce/Target.h>

namespace hce::targets {

enum DesfireType
{
   DesfireEV0 = 0,
   DesfireEV1 = 1,
   DesfireEV2 = 2
};

enum DesfireSize
{
   Desfire2K = 0x16,
   Desfire4K = 0x18,
   Desfire8K = 0x1A,
};

class Desfire final : public Target
{
   struct Impl;

   public:

      explicit Desfire(DesfireSize size = Desfire4K);

      explicit Desfire(const std::string &tag);

      rt::Variant get(int id) const override;

      bool set(int id, const rt::Variant &value) override;

      void select() override;

      void deselect() override;

      std::string raw() const override;

      int process(const rt::ByteBuffer &request, rt::ByteBuffer &response) override;

   private:

      std::shared_ptr<Impl> impl;
};

}

#endif //HCE_DESFIRE_H
