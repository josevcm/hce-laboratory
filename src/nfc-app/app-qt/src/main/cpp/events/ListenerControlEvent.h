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

#ifndef APP_LISTENER_CONTROL_EVENT_H
#define APP_LISTENER_CONTROL_EVENT_H

#include <QEvent>
#include <QMap>
#include <QVariant>

class ListenerControlEvent : public QEvent
{
   public:

      static int Type;

      enum Command
      {
         Start,
         Stop,
         Config
      };

   public:

      explicit ListenerControlEvent(int command);

      explicit ListenerControlEvent(int command, QMap<QString, QVariant> parameters);

      explicit ListenerControlEvent(int command, const QString &name, int value);

      explicit ListenerControlEvent(int command, const QString &name, float value);

      explicit ListenerControlEvent(int command, const QString &name, bool value);

      explicit ListenerControlEvent(int command, const QString &name, const QString &value);

      int command() const;

      bool contains(const QString &name) const;

      const QMap<QString, QVariant> &parameters() const;

      ListenerControlEvent *setInteger(const QString &name, int value);

      int getInteger(const QString &name, int defVal = 0) const;

      ListenerControlEvent *setFloat(const QString &name, float value);

      float getFloat(const QString &name, float defVal = 0.0) const;

      ListenerControlEvent *setDouble(const QString &name, double value);

      double getDouble(const QString &name, double defVal = 0.0) const;

      ListenerControlEvent *setBoolean(const QString &name, bool value);

      bool getBoolean(const QString &name, bool defVal = false) const;

      ListenerControlEvent *setString(const QString &name, const QString &value);

      QString getString(const QString &name, const QString &defVal = {}) const;

   private:

      int mCommand;

      QMap<QString, QVariant> mParameters;
};

#endif // DECODECONTROLEVENT_H
