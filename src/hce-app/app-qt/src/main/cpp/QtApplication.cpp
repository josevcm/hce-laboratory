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

#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QSettings>
#include <QPointer>
#include <QThreadPool>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "QtControl.h"
#include "QtWindow.h"

#include "styles/Theme.h"

#include "events/SystemStartupEvent.h"
#include "events/SystemShutdownEvent.h"

#include "QtApplication.h"

struct QtApplication::Impl
{
   // app reference
   QtApplication *app;

   // global settings
   QSettings settings;

   // decoder control
   QPointer<QtControl> control;

   // interface control
   QPointer<QtWindow> window;

   // splash screen
   QSplashScreen splash;

   // console output stream
   mutable QTextStream console;

   // signal connections
   QMetaObject::Connection applicationShutdownConnection;
   QMetaObject::Connection splashScreenCloseConnection;
   QMetaObject::Connection windowReloadConnection;

   // print frames flag
   bool printFramesEnabled = false;

   //  shutdown flag
   static bool shuttingDown;

   explicit Impl(QtApplication *app) : app(app), splash(QPixmap(":/app/app-splash"), Qt::WindowStaysOnTopHint), console(stdout)
   {
      // show splash screen
      showSplash(settings.value("settings/splashScreen", "2500").toInt());

      // create decoder control interface
      control = new QtControl();

      // create user interface window
      window = new QtWindow();

      // connect shutdown signal
      applicationShutdownConnection = connect(app, &QtApplication::aboutToQuit, app, &QtApplication::shutdown);

      // connect window show signal
      splashScreenCloseConnection = connect(window, &QtWindow::ready, &splash, &QSplashScreen::close);

      // connect reload signal
      windowReloadConnection = connect(window, &QtWindow::reload, [=] { reload(); });
   }

   ~Impl()
   {
      disconnect(windowReloadConnection);
      disconnect(splashScreenCloseConnection);
      disconnect(applicationShutdownConnection);
   }

   void startup()
   {
      qInfo() << "startup QT Interface";

      selectTheme();

      QMap<QString, QString> meta;

      postEvent(instance(), new SystemStartupEvent(meta));

      // open main window in dark mode
      Theme::showInDarkMode(window);
   }

   void reload()
   {
      qInfo() << "reload QT Interface";

      // hide window
      window->hide();

      // restart interface
      startup();
   }

   void shutdown()
   {
      qInfo() << "shutdown QT Interface";

      postEvent(instance(), new SystemShutdownEvent);

      shuttingDown = true;
   }

   void showSplash(int timeout)
   {
      if (timeout > 0)
      {
         // show splash screen
         splash.show();

         // note, window is not valid until full initialization is completed! and execute event loop
         QTimer::singleShot(timeout, &splash, &QSplashScreen::close);
      }
   }

   void selectTheme() const
   {
      //      QString style = settings.value("settings/style", "Fusion").toString();
      QString theme = settings.value("settings/theme", "dark").toString();

      //      qInfo() << "selected style:" << style;
      qInfo().noquote() << "selected theme:" << theme;

      // set style: NOTE, this is not working properly, it seems that the IconStyle is not being applied correctly
      //      if (QStyleFactory::keys().contains(style))
      //      {
      //         setStyle(style);
      //      }
      //      else
      //      {
      //         qWarning() << "style" << style << "not found, available list:" << QStyleFactory::keys();
      //      }

      // configure stylesheet
      QFile styleFile(":qdarkstyle/" + theme + "/style.qss");

      if (styleFile.open(QFile::ReadOnly | QFile::Text))
      {
         app->setStyleSheet(QTextStream(&styleFile).readAll());
      }
      else
      {
         qWarning() << "unable to set stylesheet, file not found: " << styleFile.fileName();
      }

      QIcon::setThemeName(theme);
   }

   void setPrintFramesEnabled(bool enabled)
   {
      printFramesEnabled = enabled;
   }

   void handleEvent(QEvent *event) const
   {
      window->handleEvent(event);
      control->handleEvent(event);
   }
};

bool QtApplication::Impl::shuttingDown = false;

QtApplication::QtApplication(int &argc, char **argv) : QApplication(argc, argv), impl(new Impl(this))
{
   // setup thread pool
   QThreadPool::globalInstance()->setMaxThreadCount(8);

   // startup interface
   QTimer::singleShot(0, this, [=] { startup(); });
}

void QtApplication::startup()
{
   impl->startup();
}

void QtApplication::shutdown()
{
   impl->shutdown();
}

void QtApplication::post(QEvent *event, int priority)
{
   if (!Impl::shuttingDown)
      postEvent(instance(), event, priority);
}

QDir QtApplication::dataPath()
{
   return {QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/data"};
}

QDir QtApplication::tempPath()
{
   return {QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/tmp"};
}

QFile QtApplication::dataFile(const QString &fileName)
{
   QDir dataPath = QtApplication::dataPath();

   if (!dataPath.exists())
      dataPath.mkpath(".");

   return QFile(dataPath.absoluteFilePath(fileName));
}

QFile QtApplication::tempFile(const QString &fileName)
{
   QDir tempPath = QtApplication::tempPath();

   if (!tempPath.exists())
      tempPath.mkpath(".");

   return QFile(tempPath.absoluteFilePath(fileName));
}

void QtApplication::customEvent(QEvent *event)
{
   impl->handleEvent(event);
}

void QtApplication::setPrintFramesEnabled(bool enabled)
{
   impl->setPrintFramesEnabled(enabled);
}
