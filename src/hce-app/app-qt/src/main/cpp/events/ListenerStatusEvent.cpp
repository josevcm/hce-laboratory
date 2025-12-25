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

#include "ListenerStatusEvent.h"

const int ListenerStatusEvent::Type = registerEventType();

const QString ListenerStatusEvent::Absent = "absent";
const QString ListenerStatusEvent::Idle = "idle";
const QString ListenerStatusEvent::Listening = "listening";
const QString ListenerStatusEvent::Disabled = "disabled";

ListenerStatusEvent::ListenerStatusEvent() : QEvent(static_cast<QEvent::Type>(Type))
{
}

ListenerStatusEvent::ListenerStatusEvent(int status) : QEvent(static_cast<QEvent::Type>(Type))
{
}

ListenerStatusEvent::ListenerStatusEvent(QJsonObject data) : QEvent(static_cast<QEvent::Type>(Type)), data(std::move(data))
{
}

const QJsonObject &ListenerStatusEvent::content() const
{
   return data;
}

bool ListenerStatusEvent::hasStatus() const
{
   return data.contains("status");
}

QString ListenerStatusEvent::status() const
{
   return data["status"].toString();
}

bool ListenerStatusEvent::hasIso7816() const
{
   return !data["protocol"]["iso7816"].isUndefined();
}

QJsonObject ListenerStatusEvent::iso7816() const
{
   return data["protocol"]["iso7816"].toObject();
}

ListenerStatusEvent *ListenerStatusEvent::create()
{
   return new ListenerStatusEvent();
}

ListenerStatusEvent *ListenerStatusEvent::create(const QJsonObject &data)
{
   return new ListenerStatusEvent(data);
}
