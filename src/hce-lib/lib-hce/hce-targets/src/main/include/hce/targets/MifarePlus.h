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

#ifndef HCE_MIFAREPLUS_H
#define HCE_MIFAREPLUS_H

#include <hce/Target.h>

namespace hce::targets::mifareplus {

class MifarePlus final : public Target
{
   struct Impl;

   public:

      explicit MifarePlus();

      explicit MifarePlus(const std::string &tag);

      rt::Variant get(int id) const override;

      bool set(int id, const rt::Variant &value) override;

      void select() override;

      void deselect() override;

      std::string raw() const override;

      int process(const rt::ByteBuffer &request, rt::ByteBuffer &response) override;

      bool isDirty() const override;

      void clearDirty() override;

   private:

      std::shared_ptr<Impl> impl;
};

} // namespace hce::targets::mifareplus

#endif // HCE_MIFAREPLUS_H
