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

#include "ListenerFrameEvent.h"

int ListenerFrameEvent::Type = registerEventType();

ListenerFrameEvent::ListenerFrameEvent(const hce::Frame &frame) : QEvent(static_cast<QEvent::Type>(Type)), mFrame(frame)
{
}

const hce::Frame &ListenerFrameEvent::frame() const
{
   return mFrame;
}
