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

#include "ProtocolParser.h"

struct ProtocolParser::Impl
{
   unsigned int frameCount;

   hce::Frame lastFrame;

   Impl() : frameCount(1)
   {
   }

   void reset()
   {
      frameCount = 1;
   }

   ProtocolFrame *parse(const hce::Frame &frame)
   {
      switch (frame.techType())
      {
         // case hce::FrameTech::NfcATech:
         //    return nfca.parse(frame);
         //
         // case hce::FrameTech::NfcBTech:
         //    return nfcb.parse(frame);
         //
         // case hce::FrameTech::NfcFTech:
         //    return nfcf.parse(frame);
         //
         // case hce::FrameTech::NfcVTech:
         //    return nfcv.parse(frame);
         //
         // case hce::FrameTech::Iso7816Tech:
         //    return iso7816.parse(frame);

         default:
            return nullptr;
      }
   }
};

ProtocolParser::ProtocolParser(QObject *parent) : QObject(parent), impl(new Impl)
{
}

ProtocolParser::~ProtocolParser() = default;

void ProtocolParser::reset()
{
   impl->reset();
}

ProtocolFrame *ProtocolParser::parse(const hce::Frame &frame)
{
   return impl->parse(frame);
}
