/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2024 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#ifndef HCE_FRAME_H
#define HCE_FRAME_H

#include <rt/ByteBuffer.h>

namespace hce {

enum FrameTech
{
   NfcNoneTech = 0x0000,

   // NFC tech types
   NfcATech = 0x0101,
   NfcBTech = 0x0102,
};

enum FrameType
{
   // Frame types
   NfcNoneFrame = 0x0000,
   NfcActivateFrame = 0x0100,
   NfcDeactivateFrame = 0x0101,
   NfcRequestFrame = 0x0211,
   NfcResponseFrame = 0x0212,
};

enum FrameFlags
{
};

class Frame : public rt::ByteBuffer
{
   struct Impl;

   public:

      static const Frame Nil;

   public:

      Frame();

      explicit Frame(unsigned int size);

      Frame(unsigned int techType, unsigned int frameType, unsigned long long frameTime = 0);

      Frame(unsigned int techType, unsigned int frameType, const ByteBuffer &data, unsigned long long frameTime = 0);

      Frame(const Frame &other);

      Frame &operator=(const Frame &other);

      bool operator==(const Frame &other) const;

      bool operator!=(const Frame &other) const;

      bool operator<(const Frame &other) const;

      bool operator>(const Frame &other) const;

      operator bool() const;

      unsigned int techType() const;

      void setTechType(unsigned int techType);

      unsigned int frameType() const;

      void setFrameType(unsigned int frameType);

      unsigned int frameFlags() const;

      void setFrameFlags(unsigned int frameFlags);

      void clearFrameFlags(unsigned int frameFlags);

      bool hasFrameFlags(unsigned int frameFlags) const;

      unsigned int frameRate() const;

      void setFrameRate(unsigned int frameRate);

      unsigned long long frameTime() const;

      void setFrameTime(unsigned long long frameTime);

   private:

      std::shared_ptr<Impl> impl;
};

}

#endif
