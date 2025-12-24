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

#include <cmath>
#include <fstream>
#include <iostream>

#include <QDir>
#include <QLocale>
#include <QSslSocket>
#include <QSettings>
#include <QStandardPaths>
#include <QCommandLineParser>

#include <rt/Logger.h>
#include <rt/Executor.h>

#include <hce/tasks/TargetListenerTask.h>

#include <styles/IconStyle.h>

#include <libusb.h>

#include "QtConfig.h"
#include "QtApplication.h"

void messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
   static rt::Logger *qlog = rt::Logger::getLogger("qt");

   const QByteArray localMsg = msg.toLocal8Bit();

   if (qlog)
   {
      switch (type)
      {
         case QtDebugMsg:
            qlog->debug(localMsg.constData());
            break;
         case QtInfoMsg:
            qlog->info(localMsg.constData());
            break;
         case QtWarningMsg:
            qlog->warn(localMsg.constData());
            break;
         case QtCriticalMsg:
            qlog->error(localMsg.constData());
            break;
         case QtFatalMsg:
            qlog->error(localMsg.constData());
            abort();
      }
   }
}

int startApp(int argc, char *argv[])
{
   const rt::Logger *log = rt::Logger::getLogger("app.main");

   rt::Logger::setLoggerLevel("app.*", rt::Logger::DEBUG_LEVEL);
   rt::Logger::setLoggerLevel("qt.*", rt::Logger::DEBUG_LEVEL);
   rt::Logger::setLoggerLevel("hw.*", rt::Logger::WARN_LEVEL);
   rt::Logger::setLoggerLevel("hce.targets.*", rt::Logger::DEBUG_LEVEL);
   rt::Logger::setLoggerLevel("worker.*", rt::Logger::INFO_LEVEL);
   rt::Logger::setLoggerLevel("rt.*", rt::Logger::INFO_LEVEL);

   // initialize application
   QtApplication::setApplicationName(HCE_LAB_APPLICATION_NAME);
   QtApplication::setApplicationVersion(HCE_LAB_VERSION_STRING);
   QtApplication::setOrganizationName(HCE_LAB_COMPANY_NAME);
   QtApplication::setOrganizationDomain(HCE_LAB_DOMAIN_NAME);

   log->warn("***********************************************************************");
   log->warn("HCE-LAB {}", {HCE_LAB_VERSION_STRING});
   log->warn("***********************************************************************");

   log->info("QtVersion: {}", {QT_VERSION_STR});

   if (argc > 1)
   {
      log->info("command line arguments:");

      for (int i = 1; i < argc; i++)
         log->info("\t{}", {argv[i]});
   }

   const libusb_version *lusbv = libusb_get_version();

   log->info("using usb library: {}.{}.{}", {lusbv->major, lusbv->minor, lusbv->micro});
   log->info("using ssl library: {}", {QSslSocket::sslLibraryBuildVersionString().toStdString()});
   log->info("using locale: {}", {QLocale().name().toStdString()});

   // override icons styles
   QtApplication::setStyle(new IconStyle());

   // configure settings location and format
   QSettings::setDefaultFormat(QSettings::IniFormat);
   //   QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

   // configure loggers
   QSettings settings;

   settings.beginGroup("logger");

   for (const auto &key: settings.childKeys())
   {
      if (key == "root")
         rt::Logger::setRootLevel(settings.value(key).toString().toStdString());
      else
         rt::Logger::getLogger(key.toStdString())->setLevel(settings.value(key).toString().toStdString());
   }

   settings.endGroup();

   // initialize application
   QtApplication app(argc, argv);

   // setup command line parser (after QtApplication is created)
   QCommandLineParser parser;
   parser.setApplicationDescription("NFC Laboratory - NFC Protocol Analyzer");
   const QCommandLineOption helpOption = parser.addHelpOption();
   const QCommandLineOption versionOption = parser.addVersionOption();

   // add custom options (can be extended later)
   const QCommandLineOption logLevelOption(QStringList() << "l" << "log-level", "Set log level: DEBUG, INFO, WARN, ERROR, NONE (default: INFO)", "level");
   parser.addOption(logLevelOption);

   const QCommandLineOption jsonFramesOption(QStringList() << "j" << "json-frames", "Output decoded NFC frames as JSON to stdout (one frame per line)");
   parser.addOption(jsonFramesOption);

   // process command line arguments
   parser.process(app);

   // Early exit for --help or --version to avoid hardware initialization
   if (parser.isSet(helpOption))
   {
      parser.showHelp(0);
   }
   if (parser.isSet(versionOption))
   {
      parser.showVersion();
   }

   // handle log level option
   if (parser.isSet(logLevelOption))
   {
      const QString level = parser.value(logLevelOption);
      rt::Logger::setRootLevel(level.toStdString());
      log->info("Log level set to: {}", {level.toStdString()});
   }

   // handle json frames option
   if (parser.isSet(jsonFramesOption))
   {
      app.setPrintFramesEnabled(true);
      log->info("JSON frame output enabled");
   }

   // create executor service
   rt::Executor executor(128, 5);

   executor.submit(hce::TargetListenerTask::construct(), rt::Executor::PRIORITY_HIGHEST);

   // start application
   return QtApplication::exec();
}

int main(int argc, char *argv[])
{
   // initialize logging system
#ifdef ENABLE_CONSOLE_LOGGING
   rt::Logger::init(std::cout);
#else

   QDir appPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + NFC_LAB_COMPANY_NAME + "/" + NFC_LAB_APPLICATION_NAME);

   std::ofstream stream;

   if (appPath.mkpath("log"))
   {
      QString logFile = appPath.filePath(QString("log/") + HCE_LAB_APPLICATION_NAME + ".log");

      stream.open(logFile.toStdString(), std::ios::out | std::ios::app);

      if (stream.is_open())
      {
         rt::Logger::init(stream);
      }
      else
      {
         std::cerr << "unable to open log file: " << logFile.toStdString() << std::endl;

         rt::Logger::init(std::cout);
      }
   }
   else
   {
      std::cerr << "unable to create log path: " << appPath.absolutePath().toStdString() << std::endl;

      rt::Logger::init(std::cout);
   }
#endif

   // set logging handler for QT components
   qInstallMessageHandler(messageOutput);

   // start application!
   int res = startApp(argc, argv);

   // shutdown logging system and flush pending messages
   rt::Logger::shutdown();

   return res;
}
