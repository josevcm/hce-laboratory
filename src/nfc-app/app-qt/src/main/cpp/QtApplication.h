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

#ifndef APP_QTAPPLICATION_H
#define APP_QTAPPLICATION_H

#include <QDir>
#include <QFile>
#include <QApplication>
#include <QSharedPointer>

class QtApplication : public QApplication
{
      Q_OBJECT

      struct Impl;

   public:

      QtApplication(int &argc, char **argv);

      static void post(QEvent *event, int priority = Qt::NormalEventPriority);

      static QDir dataPath();

      static QFile dataFile(const QString &fileName);

      static QDir tempPath();

      static QFile tempFile(const QString &fileName);

      void setPrintFramesEnabled(bool enabled);

   protected:

      void startup();

      void shutdown();

      void customEvent(QEvent *event) override;

   private:

      QSharedPointer<Impl> impl;
};

#endif
