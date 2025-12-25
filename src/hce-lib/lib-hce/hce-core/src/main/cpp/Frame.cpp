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

#include <hce/Frame.h>

namespace hce {

struct Frame::Impl
{
   unsigned int techType = 0;
   unsigned int frameType = 0;
   unsigned int frameFlags = 0;
   unsigned int frameRate = 0;
   unsigned long long frameTime = 0;
};

const Frame Frame::Nil;

Frame::Frame() : ByteBuffer(), impl(std::make_shared<Impl>())
{
}

Frame::Frame(const unsigned int size) : ByteBuffer(size), impl(std::make_shared<Impl>())
{
}

Frame::Frame(const unsigned int techType, const unsigned int frameType, const unsigned long long frameTime) : Frame(256)
{
   impl->techType = techType;
   impl->frameType = frameType;
   impl->frameTime = frameTime;
}

Frame::Frame(const unsigned int techType, const unsigned int frameType, const ByteBuffer &data, const unsigned long long frameTime) : Frame(data.size())
{
   impl->techType = techType;
   impl->frameType = frameType;
   impl->frameTime = frameTime;

   put(data).flip();
}

Frame::Frame(const Frame &other) : ByteBuffer(other)
{
   impl = other.impl;
}

Frame &Frame::operator=(const Frame &other)
{
   if (this == &other)
      return *this;

   ByteBuffer::operator=(other);

   impl = other.impl;

   return *this;
}

bool Frame::operator==(const Frame &other) const
{
   if (this == &other)
      return true;

   if (impl->techType != other.impl->techType ||
      impl->frameType != other.impl->frameType ||
      impl->frameFlags != other.impl->frameFlags ||
      impl->frameRate != other.impl->frameRate)
      return false;

   return ByteBuffer::operator==(other);
}

bool Frame::operator!=(const Frame &other) const
{
   return !operator==(other);
}

bool Frame::operator<(const Frame &other) const
{
   return impl->frameTime < other.impl->frameTime;
}

bool Frame::operator>(const Frame &other) const
{
   return impl->frameTime > other.impl->frameTime;
}

Frame::operator bool() const
{
   return isValid();
}

unsigned int Frame::techType() const
{
   return impl->techType;
}

void Frame::setTechType(unsigned int techType)
{
   impl->techType = techType;
}

unsigned int Frame::frameType() const
{
   return impl->frameType;
}

void Frame::setFrameType(unsigned int frameType)
{
   impl->frameType = frameType;
}

unsigned int Frame::frameFlags() const
{
   return impl->frameFlags;
}

void Frame::setFrameFlags(unsigned int frameFlags)
{
   impl->frameFlags |= frameFlags;
}

void Frame::clearFrameFlags(unsigned int frameFlags)
{
   impl->frameFlags &= ~frameFlags;
}

bool Frame::hasFrameFlags(unsigned int frameFlags) const
{
   return impl->frameFlags & frameFlags;
}

unsigned int Frame::frameRate() const
{
   return impl->frameRate;
}

void Frame::setFrameRate(unsigned int rate)
{
   impl->frameRate = rate;
}

unsigned long long Frame::frameTime() const
{
   return impl->frameTime;
}

void Frame::setFrameTime(unsigned long long frameTime)
{
   impl->frameTime = frameTime;
}

}
