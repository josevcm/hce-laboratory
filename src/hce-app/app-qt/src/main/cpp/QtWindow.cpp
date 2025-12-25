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
#include <QKeyEvent>
#include <QClipboard>
#include <QComboBox>
#include <QTimer>
#include <QStandardPaths>
#include <QScreen>
#include <QDesktopServices>
#include <QScrollBar>

#include <rt/Subject.h>

#include <events/ConsoleLogEvent.h>
#include <events/ListenerControlEvent.h>
#include <events/SystemShutdownEvent.h>
#include <events/SystemStartupEvent.h>
#include <events/ListenerFrameEvent.h>
#include <events/ListenerStatusEvent.h>

#include <model/StreamFilter.h>
#include <model/StreamModel.h>
#include <model/ParserModel.h>

#include <styles/Theme.h>

#include "ui_QtWindow.h"

#include "QtApplication.h"

#include "QtConfig.h"
#include "QtWindow.h"

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 720

struct QtWindow::Impl
{
   // application window
   QtWindow *window;

   // configuration
   QSettings settings;

   // Toolbar status
   bool followEnabled = false;
   bool filterEnabled = false;

   // interface
   QSharedPointer<Ui_QtWindow> ui;

   // Clipboard data
   QString clipboard;

   // last decoder status received
   QString targetListenerStatus = ListenerStatusEvent::Disabled;

   // Frame view model
   QPointer<StreamModel> streamModel;

   // Frame view model
   QPointer<ParserModel> parserModel;

   // Frame filter
   QPointer<StreamFilter> streamFilter;

   // refresh timer
   QPointer<QTimer> refreshTimer;

   // signal connections
   QMetaObject::Connection decodeViewDoubleClickedConnection;
   QMetaObject::Connection decodeViewSelectionChangedConnection;
   QMetaObject::Connection decodeViewValueChangedConnection;
   QMetaObject::Connection decodeViewIndicatorChangedConnection;
   QMetaObject::Connection parserViewSelectionChangedConnection;
   QMetaObject::Connection refreshTimerTimeoutConnection;

   explicit Impl(QtWindow *window) : window(window),
                                     ui(new Ui_QtWindow()),
                                     streamModel(new StreamModel()),
                                     parserModel(new ParserModel()),
                                     streamFilter(new StreamFilter()),
                                     refreshTimer(new QTimer())

   {
   }

   ~Impl()
   {
      disconnect(decodeViewIndicatorChangedConnection);
      disconnect(decodeViewValueChangedConnection);
      disconnect(decodeViewSelectionChangedConnection);
      disconnect(decodeViewDoubleClickedConnection);
      disconnect(parserViewSelectionChangedConnection);
      disconnect(refreshTimerTimeoutConnection);
   }

   void setupUi()
   {
      ui->setupUi(window);

      // fix decoder toolbar
      auto *decoderToolBarSeparator = new QWidget();
      decoderToolBarSeparator->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
      decoderToolBarSeparator->setStyleSheet("QWidget{background-color: transparent;}");
      ui->decoderToolBar->widgetForAction(ui->actionFollow)->setFixedSize(30, 32);
      ui->decoderToolBar->widgetForAction(ui->actionFilter)->setFixedSize(30, 32);
      ui->decoderToolBar->widgetForAction(ui->actionReset)->setFixedSize(30, 32);
      ui->decoderToolBarLayout->addWidget(decoderToolBarSeparator);

      // setup filter
      streamFilter->setSourceModel(streamModel);

      // setup display stretch
      ui->workbench->setStretchFactor(0, 3);
      ui->workbench->setStretchFactor(1, 2);

      // setup display stretch
      ui->decoding->setStretchFactor(0, 3);
      ui->decoding->setStretchFactor(1, 2);

      // setup frame view model
      ui->decodeView->setModel(streamFilter);
      ui->decodeView->setColumnWidth(StreamModel::Id, 50);
      ui->decodeView->setColumnWidth(StreamModel::Time, 175);
      ui->decodeView->setColumnWidth(StreamModel::Delta, 80);
      ui->decodeView->setColumnWidth(StreamModel::Rate, 80);
      ui->decodeView->setColumnWidth(StreamModel::Tech, 80);
      ui->decodeView->setColumnWidth(StreamModel::Event, 100);
      ui->decodeView->setColumnWidth(StreamModel::Flags, 80);

      // disable sort for frame column
      ui->decodeView->setSortingEnabled(StreamModel::Id, true);
      ui->decodeView->setSortingEnabled(StreamModel::Time, true);
      ui->decodeView->setSortingEnabled(StreamModel::Delta, true);
      ui->decodeView->setSortingEnabled(StreamModel::Rate, true);
      ui->decodeView->setSortingEnabled(StreamModel::Tech, true);
      ui->decodeView->setSortingEnabled(StreamModel::Event, true);

      // initialize column display type
      ui->decodeView->setColumnType(StreamModel::Id, StreamWidget::Integer);
      ui->decodeView->setColumnType(StreamModel::Time, StreamWidget::DateTime);
      ui->decodeView->setColumnType(StreamModel::Delta, StreamWidget::Elapsed);
      ui->decodeView->setColumnType(StreamModel::Rate, StreamWidget::Rate);
      ui->decodeView->setColumnType(StreamModel::Tech, StreamWidget::String);
      ui->decodeView->setColumnType(StreamModel::Flags, StreamWidget::None);
      ui->decodeView->setColumnType(StreamModel::Event, StreamWidget::String);
      ui->decodeView->setColumnType(StreamModel::Data, StreamWidget::Hex);

      // disable move columns
      ui->decodeView->horizontalHeader()->setSectionsMovable(false);

      // setup protocol view model
      ui->parserView->setModel(parserModel);
      ui->parserView->setColumnWidth(ParserModel::Name, 120);
      ui->parserView->setColumnWidth(ParserModel::Flags, 48);

      // hide parser view
      ui->parserWidget->setVisible(false);

      // update window caption
      window->setWindowTitle(HCE_LAB_VENDOR_STRING);

      // connect stream view double click signal
      decodeViewDoubleClickedConnection = connect(ui->decodeView, &QTableView::doubleClicked, [=](const QModelIndex &index) {
         updateInspectDialog(index);
      });

      // connect stream view selection signal
      decodeViewSelectionChangedConnection = connect(ui->decodeView->selectionModel(), &QItemSelectionModel::selectionChanged, [=](const QItemSelection &selected, const QItemSelection &deselected) {
         decoderSelectionChanged(selected, deselected);
      });

      // connect stream view scroll changed
      decodeViewValueChangedConnection = connect(ui->decodeView->verticalScrollBar(), &QScrollBar::valueChanged, [=](int value) {
         decoderScrollChanged(value);
      });

      // connect stream view sort changed
      decodeViewIndicatorChangedConnection = connect(ui->decodeView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, [=](int section, Qt::SortOrder order) {
         decoderSortChanged(section, order);
      });

      // connect selection signal from parser model
      parserViewSelectionChangedConnection = connect(ui->parserView->selectionModel(), &QItemSelectionModel::selectionChanged, [=](const QItemSelection &selected, const QItemSelection &deselected) {
         parserSelectionChanged();
      });

      // connect refresh timer signal
      refreshTimerTimeoutConnection = refreshTimer->callOnTimeout([=] {
         refreshView();
      });

      // start timer
      refreshTimer->start(500);
   }

   // event handlers
   void systemStartupEvent(SystemStartupEvent *event)
   {
      // update actions status
      updateActions();

      // update header view
      updateStatus();

      // trigger ready signal
      window->ready();
   }

   void systemShutdownEvent(SystemShutdownEvent *event)
   {
   }

   void consoleLogEvent(ConsoleLogEvent *event)
   {
   }

   void listenerFrameEvent(ListenerFrameEvent *event) const
   {
      if (event->frame().isValid())
      {
         streamModel->append(event->frame());
      }
   }

   void listenerStatusEvent(ListenerStatusEvent *event)
   {
      bool updated = false;

      if (event->hasStatus())
         updated |= updateTargetListenerStatus(event->status());

      if (updated)
      {
         updateStatus();
         updateActions();
      }
   }

   /*
 * logic status updates
 */
   bool updateTargetListenerStatus(const QString &value)
   {
      if (targetListenerStatus == value)
         return false;

      qInfo().noquote().nospace() << "target listener status changed from [" << targetListenerStatus << "] to [" << value << "]";

      targetListenerStatus = value;

      return true;
   }

   void setFollowEnabled(bool enabled)
   {
      followEnabled = enabled;

      ui->actionFollow->setChecked(followEnabled);
   }

   void setFilterEnabled(bool enabled)
   {
      filterEnabled = enabled;

      ui->actionFilter->setChecked(filterEnabled);

      streamFilter->setEnabled(filterEnabled);
   }

   /*
    * global status updates
    */
   void updateActions() const
   {
      // flags for device status
      const bool targetListenerDevicePresent = targetListenerStatus != ListenerStatusEvent::Absent;
      const bool targetListenerDeviceEnabled = targetListenerStatus != ListenerStatusEvent::Disabled;
      const bool targetListenerDeviceListening = targetListenerStatus == ListenerStatusEvent::Listening;

      // disable / enable actions based on streaming status
      if (targetListenerDeviceListening)
      {
         // disable actions during streaming
         ui->actionListen->setEnabled(false);
         ui->actionStop->setEnabled(true);
      }
      else
      {
         // reset actions to default state
         ui->actionListen->setEnabled(targetListenerDevicePresent && targetListenerDeviceEnabled);
         ui->actionStop->setEnabled(false);
      }
   }

   void updateStatus() const
   {
   }

   /*
    * data status updates
    */
   void updateInspectDialog(const QModelIndex &index) const
   {
   }

   /*
    * clipboard update
    */
   void clipboardPrepare(QModelIndexList &indexList)
   {
   }

   void clipboardCopy() const
   {
      QApplication::clipboard()->setText(clipboard);
   }

   void decoderSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
   {
   }

   void decoderScrollChanged(int value)
   {
      setFollowEnabled(ui->decodeView->isLastRowVisible());
   }

   void decoderSortChanged(int section, Qt::SortOrder order)
   {
      clearSelection();
   }

   void parserSelectionChanged() const
   {
   }

   void refreshView() const
   {
      if (!streamModel->canFetchMore())
         return;

      // fetch pending data from model
      streamModel->fetchMore();

      // enable view if data is present
      if (!ui->decodeView->isEnabled() && streamModel->rowCount() > 0)
         ui->decodeView->setEnabled(true);

      if (followEnabled)
         ui->decodeView->scrollToBottom();

      // update view to fit all content
      ui->decodeView->resizeColumnToContents(StreamModel::Data);
   }

   /*
    * slots for interface actions
    */
   void openFile()
   {
      qInfo() << "open file";

      QDir dataPath = QtApplication::dataPath();

      QString fileName = Theme::openFileDialog(window, tr("Open trace file"), dataPath.absolutePath(), tr("Capture (*.wav *.trz)"));

      if (fileName.isEmpty())
         return;

      if (!(fileName.endsWith(".wav") || fileName.endsWith(".trz")))
      {
         Theme::messageDialog(window, tr("Unable to open file"), tr("Invalid file name: %1").arg(fileName));
         return;
      }

      QFile file(fileName);

      if (!file.open(QIODevice::ReadOnly))
      {
         Theme::messageDialog(window, tr("Unable to open file"), file.errorString());
         return;
      }

      clearView();
   }

   void saveFile() const
   {
      qInfo() << "save signal trace";
   }

   void openConfig() const
   {
      QString filePath = settings.fileName();

      QFileInfo info(filePath);

      if (!info.exists())
      {
         qWarning("File not found: %s", qUtf8Printable(filePath));
         return;
      }

      QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
   }

   void toggleStart(bool recording)
   {
      qInfo() << "listener starting";

      // disable action to avoid multiple start
      ui->actionListen->setEnabled(false);

      // enable follow
      setFollowEnabled(true);

      // start listener
      QtApplication::post(new ListenerControlEvent(ListenerControlEvent::Start));

      clearView();
   }

   void toggleStop()
   {
      qInfo() << "listener stopping";

      // disable action to avoid multiple pause / stop
      ui->actionStop->setEnabled(false);

      // stop listener
      QtApplication::post(new ListenerControlEvent(ListenerControlEvent::Stop));
   }

   void toggleFollow()
   {
      setFollowEnabled(ui->actionFollow->isChecked());
   }

   void toggleFilter()
   {
      setFilterEnabled(ui->actionFilter->isChecked());
   }

   void showAboutInfo()
   {
   }

   void showHelpInfo()
   {
   }

   void clearSelection() const
   {
      qInfo() << "clear selection";
   }

   void clearView()
   {
      qInfo() << "clear events and views";
   }

   void resetView() const
   {
      qInfo() << "reset view";
   }

   void readSettings()
   {
      settings.beginGroup("window");

      QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();

      Qt::WindowState windowState = static_cast<Qt::WindowState>(settings.value("windowState", Qt::WindowState::WindowNoState).toInt());

      window->setWindowState(windowState);

      if (!(windowState & Qt::WindowMaximized))
      {
         int windowWidth = settings.value("windowWidth", DEFAULT_WINDOW_WIDTH).toInt();
         int windowHeight = settings.value("windowHeight", DEFAULT_WINDOW_HEIGHT).toInt();
         int windowTop = (screenGeometry.height() - windowHeight) / 2;
         int windowLeft = (screenGeometry.width() - windowWidth) / 2;

         QRect windowGeometry;

         windowGeometry.setTop(std::clamp(windowTop, screenGeometry.top(), screenGeometry.bottom()));
         windowGeometry.setLeft(std::clamp(windowLeft, screenGeometry.left(), screenGeometry.right()));
         windowGeometry.setBottom(std::clamp(windowTop + windowHeight, screenGeometry.top(), screenGeometry.bottom()));
         windowGeometry.setRight(std::clamp(windowLeft + windowWidth, screenGeometry.left(), screenGeometry.right()));

         window->setGeometry(windowGeometry);
      }

      // restore interface preferences
      setFollowEnabled(settings.value("followEnabled", true).toBool());
      setFilterEnabled(settings.value("filterEnabled", true).toBool());

      settings.endGroup();
   }

   void writeSettings()
   {
      settings.beginGroup("window");
      settings.setValue("windowWidth", window->geometry().width());
      settings.setValue("windowHeight", window->geometry().height());
      settings.setValue("windowState", (int)window->windowState());
      settings.endGroup();
   }

   bool userReallyWantsToQuit() const
   {
      if (settings.value("settings/quitConfirmation", true).toBool())
         return Theme::messageDialog(window, tr("Confirmation"), tr("Do you want to quit?"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;

      return true;
   }

   static bool isActive(QAction *action)
   {
      return action->isEnabled() && action->isChecked();
   }

   static bool in(const QString &value, const QList<QString> &list)
   {
      return list.contains(value);
   }
};

QtWindow::QtWindow() : impl(new Impl(this))
{
   // configure window properties
   setAttribute(Qt::WA_OpaquePaintEvent, true);
   setAttribute(Qt::WA_DontCreateNativeAncestors, true);
   setAttribute(Qt::WA_NativeWindow, true);
   setAttribute(Qt::WA_NoSystemBackground, true);
   setAutoFillBackground(false);

#ifdef WIN32
   setAttribute(Qt::WA_PaintOnScreen, true);
#endif

   impl->setupUi();

   // update window size
   impl->readSettings();
}

void QtWindow::openFile()
{
   impl->openFile();
}

void QtWindow::saveFile()
{
   impl->saveFile();
}

void QtWindow::openConfig()
{
   impl->openConfig();
}

void QtWindow::toggleListen()
{
   impl->toggleStart(false);
}

void QtWindow::toggleStop()
{
   impl->toggleStop();
}

void QtWindow::toggleFollow()
{
   impl->toggleFollow();
}

void QtWindow::toggleFilter()
{
   impl->toggleFilter();
}

void QtWindow::clearView()
{
   if (Theme::messageDialog(this, tr("Confirmation"), tr("Do you want to remove all events?"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
      return;

   impl->clearView();
}

void QtWindow::resetView()
{
   impl->resetView();
}

void QtWindow::showAboutInfo()
{
   impl->showAboutInfo();
}

void QtWindow::showHelpInfo()
{
   impl->showHelpInfo();
}

void QtWindow::keyPressEvent(QKeyEvent *event)
{
   // key press with control modifier
   if (event->modifiers() & Qt::ControlModifier)
   {
      if (event->key() == Qt::Key_C)
      {
         impl->clipboardCopy();
         return;
      }
   }

   // key press without modifiers
   else
   {
      if (event->key() == Qt::Key_Escape)
      {
         impl->clearSelection();
         return;
      }
   }

   QMainWindow::keyPressEvent(event);
}

void QtWindow::handleEvent(QEvent *event)
{
   if (event->type() == ConsoleLogEvent::Type)
      impl->consoleLogEvent(dynamic_cast<ConsoleLogEvent *>(event));
   else if (event->type() == SystemStartupEvent::Type)
      impl->systemStartupEvent(dynamic_cast<SystemStartupEvent *>(event));
   else if (event->type() == SystemShutdownEvent::Type)
      impl->systemShutdownEvent(dynamic_cast<SystemShutdownEvent *>(event));
   else if (event->type() == ListenerFrameEvent::Type)
      impl->listenerFrameEvent(dynamic_cast<ListenerFrameEvent *>(event));
   else if (event->type() == ListenerStatusEvent::Type)
      impl->listenerStatusEvent(dynamic_cast<ListenerStatusEvent *>(event));
}

void QtWindow::closeEvent(QCloseEvent *event)
{
   if (impl->userReallyWantsToQuit())
   {
      impl->writeSettings();

      event->accept();
   }
   else
   {
      event->ignore();
   }
}
