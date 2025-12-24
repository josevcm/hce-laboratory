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

#include "ListenerControlEvent.h"

#include <utility>

int ListenerControlEvent::Type = registerEventType();

ListenerControlEvent::ListenerControlEvent(int command) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command)
{
}

ListenerControlEvent::ListenerControlEvent(int command, QMap<QString, QVariant> parameters) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command), mParameters(std::move(parameters))
{
}

ListenerControlEvent::ListenerControlEvent(int command, const QString &name, int value) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command)
{
   mParameters[name] = value;
}

ListenerControlEvent::ListenerControlEvent(int command, const QString &name, float value) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command)
{
   mParameters[name] = value;
}

ListenerControlEvent::ListenerControlEvent(int command, const QString &name, bool value) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command)
{
   mParameters[name] = value;
}

ListenerControlEvent::ListenerControlEvent(int command, const QString &name, const QString &value) : QEvent(static_cast<QEvent::Type>(Type)), mCommand(command)
{
   mParameters[name] = value;
}

int ListenerControlEvent::command() const
{
   return mCommand;
}

bool ListenerControlEvent::contains(const QString &name) const
{
   return mParameters.contains(name);
}

const QMap<QString, QVariant> &ListenerControlEvent::parameters() const
{
   return mParameters;
}

ListenerControlEvent *ListenerControlEvent::setInteger(const QString &name, int value)
{
   mParameters[name] = value;

   return this;
}

int ListenerControlEvent::getInteger(const QString &name, int defVal) const
{
   if (!mParameters.contains(name))
      return defVal;

   return mParameters[name].toInt();
}

ListenerControlEvent *ListenerControlEvent::setFloat(const QString &name, float value)
{
   mParameters[name] = value;

   return this;
}

float ListenerControlEvent::getFloat(const QString &name, float defVal) const
{
   if (!mParameters.contains(name))
      return defVal;

   return mParameters[name].toFloat();
}

ListenerControlEvent *ListenerControlEvent::setDouble(const QString &name, double value)
{
   mParameters[name] = value;

   return this;
}

double ListenerControlEvent::getDouble(const QString &name, double defVal) const
{
   if (!mParameters.contains(name))
      return defVal;

   return mParameters[name].toDouble();
}

ListenerControlEvent *ListenerControlEvent::setBoolean(const QString &name, bool value)
{
   mParameters[name] = value;

   return this;
}

bool ListenerControlEvent::getBoolean(const QString &name, bool defVal) const
{
   if (!mParameters.contains(name))
      return defVal;

   return mParameters[name].toBool();
}

ListenerControlEvent *ListenerControlEvent::setString(const QString &name, const QString &value)
{
   mParameters[name] = value;

   return this;
}

QString ListenerControlEvent::getString(const QString &name, const QString &defVal) const
{
   if (!mParameters.contains(name))
      return defVal;

   return mParameters[name].toString();
}

