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

#ifndef APP_QTWINDOW_H
#define APP_QTWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QSharedPointer>

class QtCache;

class QtWindow : public QMainWindow
{
      Q_OBJECT

      struct Impl;

   public:

      explicit QtWindow();

      void handleEvent(QEvent *event);

   public Q_SLOTS:

      void openFile();

      void saveFile();

      void openConfig();

      void toggleListen();

      void toggleStop();

      void toggleFollow();

      void toggleFilter();

      void showAboutInfo();

      void showHelpInfo();

      void clearView();

      void resetView();

   Q_SIGNALS:

      void ready();

      void reload();

   protected:

      void keyPressEvent(QKeyEvent *event) override;

      void closeEvent(QCloseEvent *event) override;

   private:

      QSharedPointer<Impl> impl;
};

#endif /* MAINWINDOW_H */
