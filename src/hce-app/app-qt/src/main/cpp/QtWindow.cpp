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
#include <QColor>
#include <QKeyEvent>
#include <QClipboard>
#include <QComboBox>
#include <QTimer>
#include <QStandardPaths>
#include <QScreen>
#include <QDesktopServices>
#include <QScrollBar>
#include <QCoreApplication>
#include <QFileInfo>

#include <rt/Subject.h>

#include <events/ConsoleLogEvent.h>
#include <events/ListenerControlEvent.h>
#include <events/SystemShutdownEvent.h>
#include <events/SystemStartupEvent.h>
#include <events/ListenerFrameEvent.h>
#include <events/ListenerStatusEvent.h>
#include <events/TargetStateEvent.h>

#include <hce/Frame.h>

#include <model/ParserModel.h>
#include <model/ProtocolModel.h>
#include <model/StreamFilter.h>
#include <model/StreamModel.h>
#include <model/TargetModel.h>

#include <protocol/DesfireParser.h>

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

   // detected devices
   QStringList enabledDevices;
   QStringList disabledDevices;

   // interface
   QSharedPointer<Ui_QtWindow> ui;

   // Clipboard data
   QString clipboard;

   // last decoder status received
   QString targetListenerStatus = ListenerStatusEvent::Disabled;
   QString targetListenerName;
   bool targetListenerEnabled = false;

   int selectedRootRow = -1; // row of the currently selected root card item in the tree
   int pendingActiveRow = -1; // row passed with the last Start command

   // Target view model
   QPointer<TargetModel> targetModel;

   // Frame view model
   QPointer<StreamModel> streamModel;

   // Parser view model
   QPointer<ParserModel> parserModel;

   // Per-target APDU protocol models
   QVector<QPointer<ProtocolModel>> protocolModels;
   QPointer<ProtocolModel> protocolFallbackModel;

   // Frame filter
   QPointer<StreamFilter> streamFilter;

   // refresh timer
   QPointer<QTimer> refreshTimer;

   // signal connections
   QMetaObject::Connection targetTreeSelectionChangedConnection;
   QMetaObject::Connection decodeViewDoubleClickedConnection;
   QMetaObject::Connection protocolViewDoubleClickedConnection;
   QMetaObject::Connection protocolViewSelectionChangedConnection;
   QMetaObject::Connection decodeViewSelectionChangedConnection;
   QMetaObject::Connection decodeViewValueChangedConnection;
   QMetaObject::Connection decodeViewIndicatorChangedConnection;
   QMetaObject::Connection parserViewSelectionChangedConnection;
   QMetaObject::Connection refreshTimerTimeoutConnection;

   bool syncingSelection = false;

   explicit Impl(QtWindow *window) : window(window),
                                     ui(new Ui_QtWindow()),
                                     targetModel(new TargetModel()),
                                     streamModel(new StreamModel()),
                                     parserModel(new ParserModel()),
                                     protocolFallbackModel(new ProtocolModel()),
                                     streamFilter(new StreamFilter()),
                                     refreshTimer(new QTimer())
   {
   }

   void ensureProtocolModels()
   {
      const int rootCount = targetModel->rowCount({});

      while (protocolModels.size() < static_cast<qsizetype>(rootCount))
         protocolModels.append(new ProtocolModel(window));
   }

   ProtocolModel *protocolModelForRow(int row)
   {
      if (row < 0)
         return protocolFallbackModel;

      ensureProtocolModels();

      if (row >= static_cast<int>(protocolModels.size()) || protocolModels[row].isNull())
         return protocolFallbackModel;

      return protocolModels[row];
   }

   void showProtocolModel(int row)
   {
      ui->targetProtocolTable->setModel(protocolModelForRow(row));

      disconnect(protocolViewSelectionChangedConnection);

      if (ui->targetProtocolTable->selectionModel())
      {
         protocolViewSelectionChangedConnection = connect(
            ui->targetProtocolTable->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            [=](const QItemSelection &selected, const QItemSelection &deselected) {
               protocolSelectionChanged(selected, deselected);
            });
      }
   }

   ~Impl()
   {
      disconnect(targetTreeSelectionChangedConnection);
      disconnect(decodeViewIndicatorChangedConnection);
      disconnect(decodeViewValueChangedConnection);
      disconnect(decodeViewSelectionChangedConnection);
      disconnect(decodeViewDoubleClickedConnection);
      disconnect(protocolViewDoubleClickedConnection);
      disconnect(protocolViewSelectionChangedConnection);
      disconnect(parserViewSelectionChangedConnection);
      disconnect(refreshTimerTimeoutConnection);
   }

   QModelIndex findStreamIndexByTime(const qulonglong frameTime, const hce::FrameType frameType) const
   {
      if (frameTime == 0)
         return {};

      for (int row = 0; row < streamFilter->rowCount(); ++row)
      {
         const QModelIndex candidate = streamFilter->index(row, 0);
         const hce::Frame *frame = streamFilter->frame(candidate);

         if (!frame)
            continue;

         if (frame->frameTime() == frameTime && frame->frameType() == frameType)
            return candidate;
      }

      return {};
   }

   void protocolSelectionChanged(const QItemSelection &selected, const QItemSelection &)
   {
      if (syncingSelection || selected.isEmpty())
         return;

      const QModelIndex protocolIndex = selected.indexes().first();
      const QModelIndex requestIndex = protocolIndex.sibling(protocolIndex.row(), ProtocolModel::Time);
      const qulonglong requestTime = requestIndex.data(ProtocolModel::RequestTimeRole).toULongLong();

      const QModelIndex requestStreamIndex = findStreamIndexByTime(requestTime, hce::FrameType::NfcRequestFrame);
      if (!requestStreamIndex.isValid())
         return;

      syncingSelection = true;

      QItemSelection streamSelection;

      if (requestStreamIndex.isValid())
         streamSelection.select(requestStreamIndex, requestStreamIndex.sibling(requestStreamIndex.row(), StreamModel::Data));

      ui->decodeView->selectionModel()->select(streamSelection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      ui->decodeView->setCurrentIndex(requestStreamIndex);
      ui->decodeView->scrollTo(requestStreamIndex, QAbstractItemView::PositionAtCenter);

      syncingSelection = false;
   }

   static QString rawHex(const QByteArray &data)
   {
      if (data.isEmpty())
         return "<empty>";

      return data.toHex(' ').toUpper();
   }

   static QString multilineParts(const QString &value, const QString &empty = "<none>")
   {
      if (value.trimmed().isEmpty())
         return empty;

      QStringList lines;
      const QStringList parts = value.split(" | ", Qt::SkipEmptyParts);

      for (const QString &part: parts)
         lines << QString("- %1").arg(part.trimmed());

      return lines.join("\n");
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

      // setup workbench stretch
      ui->workbench->setStretchFactor(0, 3);
      ui->workbench->setStretchFactor(1, 2);

      // setup navigation stretch
      ui->navigation->setStretchFactor(0, 0);
      ui->navigation->setStretchFactor(1, 1);

      // setup display stretch
      ui->decoding->setStretchFactor(0, 3);
      ui->decoding->setStretchFactor(1, 2);

      // setup target view model
      ui->targetTree->setModel(targetModel);
      ui->targetTree->setColumnWidth(TargetModel::Name, 250);
      ui->targetTree->setColumnWidth(TargetModel::Type, 150);

      // setup info table
      ui->targetInfoTable->setColumnWidth(0, 130);

      // setup protocol table
      showProtocolModel(-1);
      ui->targetProtocolTable->setColumnWidth(ProtocolModel::Time, 175);
      ui->targetProtocolTable->setColumnWidth(ProtocolModel::Command, 150);
      ui->targetProtocolTable->setColumnWidth(ProtocolModel::Status, 100);
      ui->targetProtocolTable->setColumnWidth(ProtocolModel::Detail, 420);
      ui->targetProtocolTable->setColumnType(ProtocolModel::Time, StreamWidget::DateTime);
      ui->targetProtocolTable->setColumnType(ProtocolModel::Command, StreamWidget::String);
      ui->targetProtocolTable->setColumnType(ProtocolModel::Status, StreamWidget::String);
      ui->targetProtocolTable->setColumnType(ProtocolModel::Detail, StreamWidget::String);
      ui->targetProtocolTable->setSortingEnabled(ProtocolModel::Time, true);
      ui->targetProtocolTable->setSortingEnabled(ProtocolModel::Command, true);
      ui->targetProtocolTable->setSortingEnabled(ProtocolModel::Status, true);
      ui->targetProtocolTable->setSortingEnabled(ProtocolModel::Detail, true);

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

      // show raw APDU payloads for selected protocol row
      protocolViewDoubleClickedConnection = connect(ui->targetProtocolTable, &QTableView::doubleClicked, [=](const QModelIndex &index) {
         protocolDoubleClicked(index);
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

      // connect target tree selection to hex view and info table
      targetTreeSelectionChangedConnection = connect(ui->targetTree->selectionModel(), &QItemSelectionModel::selectionChanged, [=](const QItemSelection &selected, const QItemSelection &) {
         targetSelectionChanged(selected);
      });

      // start timer
      refreshTimer->start(500);
   }

   // event handlers
   void systemStartupEvent(SystemStartupEvent *event)
   {
      loadDefaultTarget();

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

         if (pendingActiveRow >= 0 && pendingActiveRow < protocolModels.size() &&
            (event->frame().frameType() == hce::FrameType::NfcRequestFrame ||
               event->frame().frameType() == hce::FrameType::NfcResponseFrame))
         {
            protocolModels[pendingActiveRow]->append(event->frame());
         }
      }
   }

   void listenerStatusEvent(ListenerStatusEvent *event)
   {
      bool updated = false;

      if (event->hasName())
         updated |= updateTargetListenerName(event->name());

      if (event->hasStatus())
         updated |= updateTargetListenerStatus(event->status());

      if (updated)
      {
         updateStatus();
         updateActions();

         // sync tree item availability with emulation state
         if (targetListenerStatus == ListenerStatusEvent::Listening)
            targetModel->setActiveTarget(pendingActiveRow);
         else
            targetModel->clearActiveTarget();
      }
   }

   void targetStateEvent(TargetStateEvent *event)
   {
      if (pendingActiveRow < 0)
         return;

      const QJsonObject &content = event->content();

      if (content.isEmpty())
         return;

      targetModel->updateContent(pendingActiveRow, content);
   }

   /*
    * listener status updates
    */
   bool updateTargetListenerStatus(const QString &value)
   {
      if (targetListenerStatus == value)
         return false;

      qInfo().noquote().nospace() << "target listener status changed from [" << targetListenerStatus << "] to [" << value << "]";

      targetListenerStatus = value;

      if (targetListenerEnabled = targetListenerStatus != ListenerStatusEvent::Absent; !targetListenerEnabled)
      {
         // clear device information
         targetListenerName.clear();
      }

      updateDevices();

      return true;
   }

   bool updateTargetListenerName(const QString &value)
   {
      if (targetListenerName == value)
         return false;

      qInfo().noquote().nospace() << "target listener name changed from [" << targetListenerName << "] to [" << value << "]";

      targetListenerName = value;

      return true;
   }

   void updateDevices()
   {
      enabledDevices.clear();
      disabledDevices.clear();

      if (targetListenerEnabled)
         enabledDevices << targetListenerName;
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
      const bool targetSelected = selectedRootRow >= 0;

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
         ui->actionListen->setEnabled(targetListenerDevicePresent && targetListenerDeviceEnabled && targetSelected);
         ui->actionStop->setEnabled(false);
      }

      ui->actionSave->setEnabled(targetSelected);
   }

   void updateStatus() const
   {
      if (enabledDevices.isEmpty())
         ui->statusBar->showMessage(tr("No devices available"));
      else
         ui->statusBar->showMessage(QString(tr("Detected %1").arg(enabledDevices.join(", "))));
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

   void targetSelectionChanged(const QItemSelection &selected)
   {
      // clear both panels
      ui->targetHexView->clear();
      ui->targetInfoTable->clearContents();
      ui->targetInfoTable->setRowCount(0);

      if (selected.isEmpty())
      {
         showProtocolModel(-1);
         return;
      }

      QModelIndex index = selected.indexes().first();
      QModelIndex rootIndex = index;

      while (rootIndex.parent().isValid())
         rootIndex = rootIndex.parent();

      showProtocolModel(rootIndex.row());

      // update hex view (BytesRole)
      QByteArray bytes = index.data(TargetModel::BytesRole).toByteArray();

      if (!bytes.isEmpty())
         ui->targetHexView->setData(bytes);

      // track root card item selection and refresh action state
      if (rootIndex.isValid() && !rootIndex.data(TargetModel::JsonRole).toString().isEmpty())
      {
         selectedRootRow = rootIndex.row();
         updateActions();
      }

      // update info table (InfoRole)
      QVariantList info = index.data(TargetModel::InfoRole).toList();

      if (info.isEmpty())
         return;

      ui->targetInfoTable->setRowCount(info.size());

      for (int row = 0; row < info.size(); ++row)
      {
         QVariantList pair = info[row].toList();
         auto *nameItem = new QTableWidgetItem(pair.value(0).toString());
         auto *valueItem = new QTableWidgetItem(pair.value(1).toString());

         if (pair.size() > 2 && pair.value(2).toBool())
         {
            const QColor warningColor(0xE5, 0x39, 0x35);
            nameItem->setForeground(warningColor);
            valueItem->setForeground(warningColor);
         }

         ui->targetInfoTable->setItem(row, 0, nameItem);
         ui->targetInfoTable->setItem(row, 1, valueItem);
      }
   }

   void decoderSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
   {
      if (syncingSelection || selected.isEmpty())
         return;

      const QModelIndex streamIndex = selected.indexes().first();
      const hce::Frame *frame = streamFilter->frame(streamIndex);

      if (!frame)
         return;

      auto *model = dynamic_cast<ProtocolModel *>(ui->targetProtocolTable->model());

      if (!model)
         return;

      const bool isRequest = frame->frameType() == hce::FrameType::NfcRequestFrame;
      const bool isResponse = frame->frameType() == hce::FrameType::NfcResponseFrame;

      if (!isRequest && !isResponse)
         return;

      const int role = isRequest ? ProtocolModel::RequestTimeRole : ProtocolModel::ResponseTimeRole;
      QModelIndex protocolMatch;

      for (int row = 0; row < model->rowCount(); ++row)
      {
         const QModelIndex candidate = model->index(row, ProtocolModel::Time);

         if (candidate.data(role).toULongLong() == frame->frameTime())
         {
            protocolMatch = candidate;
            break;
         }
      }

      if (!protocolMatch.isValid())
         return;

      syncingSelection = true;
      ui->targetProtocolTable->selectionModel()->select(protocolMatch, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      ui->targetProtocolTable->setCurrentIndex(protocolMatch);
      ui->targetProtocolTable->scrollTo(protocolMatch, QAbstractItemView::PositionAtCenter);
      syncingSelection = false;
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

   void protocolDoubleClicked(const QModelIndex &index) const
   {
      if (!index.isValid())
         return;

      const QModelIndex timeIndex = index.sibling(index.row(), ProtocolModel::Time);
      const QModelIndex commandIndex = index.sibling(index.row(), ProtocolModel::Command);
      const QModelIndex statusIndex = index.sibling(index.row(), ProtocolModel::Status);
      const QModelIndex infoIndex = index.sibling(index.row(), ProtocolModel::Detail);

      const QByteArray rawRequest = index.data(ProtocolModel::RawRequestRole).toByteArray();
      const QByteArray rawResponse = index.data(ProtocolModel::RawResponseRole).toByteArray();

      const auto request = DesfireParser::parseRequest(rawRequest);
      const auto response = DesfireParser::parseResponse(rawResponse, request);

      const QString requestTransport = request.summary.isEmpty() ? "<unknown>" : request.summary;
      const QString requestParams = multilineParts(request.params);

      const QString responseStatus = response.status.isEmpty() ? statusIndex.data().toString() : response.status;
      const QString responseMeaning = multilineParts(response.summary);
      const QString responseParams = multilineParts(response.params);

      const QString text = QString("Time: %1\nCommand: %2\nStatus: %3\nInfo: %4\n\nREQUEST\nTransport: %5\nParameters:\n%6\nAPDU (%7 bytes):\n%8\n\nRESPONSE\nStatus word: %9\nMeaning:\n%10\nParameters:\n%11\nAPDU (%12 bytes):\n%13")
         .arg(timeIndex.data().toString())
         .arg(commandIndex.data().toString())
         .arg(statusIndex.data().toString())
         .arg(infoIndex.data().toString())
         .arg(requestTransport)
         .arg(requestParams)
         .arg(rawRequest.size())
         .arg(rawHex(rawRequest))
         .arg(responseStatus)
         .arg(responseMeaning)
         .arg(responseParams)
         .arg(rawResponse.size())
         .arg(rawHex(rawResponse));

      Theme::messageDialog(window, tr("Protocol APDU"), text);
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

      QStringList fileNames = Theme::openFilesDialog(window, tr("Open target files"), dataPath.absolutePath(), tr("Target files (*.json)"));

      if (fileNames.isEmpty())
         return;

      bool anyLoaded = false;

      for (const QString &fileName: fileNames)
      {
         if (fileName.endsWith(".json"))
         {
            if (!targetModel->load(fileName))
            {
               Theme::messageDialog(window, tr("Unable to open file"), tr("Invalid or unsupported target file:\n%1").arg(fileName));
               continue;
            }

            qInfo().noquote() << "target loaded:" << fileName;
            anyLoaded = true;
         }
         else
         {
            Theme::messageDialog(window, tr("Unable to open file"), tr("Unsupported file format:\n%1").arg(fileName));
         }
      }

      if (anyLoaded)
      {
         ensureProtocolModels();
         updateActions();
      }
   }

   bool loadDefaultTarget()
   {
      const QString fileName = resolveDefaultTargetFile();

      if (fileName.isEmpty())
      {
         Theme::messageDialog(
            window,
            tr("Default target not found"),
            tr("Cannot find default target file:\n%1").arg("desfire-factory.json"));
         return false;
      }

      // Force the default target on each startup.
      targetModel->resetModel();
      selectedRootRow = -1;
      pendingActiveRow = -1;

      if (!targetModel->load(fileName))
      {
         Theme::messageDialog(
            window,
            tr("Unable to load default target"),
            tr("Invalid or unsupported target file:\n%1").arg(fileName));
         return false;
      }

      ensureProtocolModels();
      selectDefaultTarget();
      updateActions();

      qInfo().noquote() << "default target loaded:" << fileName;
      return true;
   }

   QString resolveDefaultTargetFile() const
   {
      const QString relativePath = "targets/desfire/desfire-factory.json";
      const QDir appDir(QCoreApplication::applicationDirPath());
      const QStringList candidates = {
         QtApplication::dataPath().absoluteFilePath(relativePath),
         appDir.absoluteFilePath(relativePath),
         appDir.absoluteFilePath("../" + relativePath),
         QDir::current().absoluteFilePath(relativePath),
         QDir::current().absoluteFilePath("../../" + relativePath)
      };

      for (const QString &candidate: candidates)
      {
         if (QFileInfo::exists(candidate))
            return candidate;
      }

      return {};
   }

   void selectDefaultTarget() const
   {
      const QModelIndex rootIndex = targetModel->index(0, 0, {});

      if (!rootIndex.isValid())
         return;

      QModelIndex preferredIndex = rootIndex;

      if (targetModel->rowCount(rootIndex) > 0)
         preferredIndex = targetModel->index(0, 0, rootIndex);

      ui->targetTree->expand(rootIndex);
      ui->targetTree->setCurrentIndex(preferredIndex);
      ui->targetTree->selectionModel()->select(preferredIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      ui->targetTree->scrollTo(preferredIndex);
   }

   void saveFile() const
   {
      if (selectedRootRow < 0)
         return;

      QModelIndex rootIndex = targetModel->index(selectedRootRow, 0, {});
      QString json = rootIndex.data(TargetModel::JsonRole).toString();

      if (json.isEmpty())
         return;

      QDir dataPath = QtApplication::dataPath();

      QString fileName = Theme::saveFileDialog(window, tr("Save target file"), dataPath.absolutePath(), tr("Target files (*.json)"));

      if (fileName.isEmpty())
         return;

      QJsonParseError parseError;
      QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

      if (doc.isNull())
      {
         Theme::messageDialog(window, tr("Unable to save file"), tr("Internal error: invalid target JSON."));
         return;
      }

      QFile file(fileName);

      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
         Theme::messageDialog(window, tr("Unable to save file"), tr("Cannot open file for writing:\n%1").arg(fileName));
         return;
      }

      file.write(doc.toJson(QJsonDocument::Indented));
      qInfo().noquote() << "target saved:" << fileName;
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

      QModelIndex rootIndex = targetModel->index(selectedRootRow, 0, {});
      QString json = rootIndex.data(TargetModel::JsonRole).toString();

      if (json.isEmpty())
         return;

      pendingActiveRow = selectedRootRow;

      if (auto *model = protocolModelForRow(pendingActiveRow))
         model->resetModel();

      // disable action to avoid multiple start
      ui->actionListen->setEnabled(false);

      // clear previous data
      clearView();

      // enable follow
      setFollowEnabled(true);

      const int protocolTabIndex = ui->upperTabs->indexOf(ui->targetProtocol);

      if (protocolTabIndex >= 0)
         ui->upperTabs->setCurrentIndex(protocolTabIndex);

      QtApplication::post(new ListenerControlEvent(ListenerControlEvent::Start, "targetJson", json));
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

      // clear stream model selection
      ui->decodeView->clearSelection();
   }

   void clearView()
   {
      qInfo() << "clear events and views";

      // clear stream model
      streamModel->resetModel();

      // clear parser model
      parserModel->resetModel();

      // hide parser view
      ui->parserWidget->hide();
   }

   void resetView() const
   {
      qInfo() << "reset view";

      // reset columns width
      ui->decodeView->setColumnWidth(StreamModel::Id, 50);
      ui->decodeView->setColumnWidth(StreamModel::Time, 175);
      ui->decodeView->setColumnWidth(StreamModel::Delta, 80);
      ui->decodeView->setColumnWidth(StreamModel::Rate, 80);
      ui->decodeView->setColumnWidth(StreamModel::Tech, 80);
      ui->decodeView->setColumnWidth(StreamModel::Event, 100);
      ui->decodeView->setColumnWidth(StreamModel::Flags, 80);

      // initialize column display type
      ui->decodeView->setColumnType(StreamModel::Id, StreamWidget::Integer);
      ui->decodeView->setColumnType(StreamModel::Time, StreamWidget::DateTime);
      ui->decodeView->setColumnType(StreamModel::Delta, StreamWidget::Elapsed);
      ui->decodeView->setColumnType(StreamModel::Rate, StreamWidget::Rate);
      ui->decodeView->setColumnType(StreamModel::Tech, StreamWidget::String);
      ui->decodeView->setColumnType(StreamModel::Flags, StreamWidget::None);
      ui->decodeView->setColumnType(StreamModel::Event, StreamWidget::String);
      ui->decodeView->setColumnType(StreamModel::Data, StreamWidget::Hex);

      // reset sort indicator to first column
      ui->decodeView->horizontalHeader()->setSortIndicator(StreamModel::Id, Qt::AscendingOrder);

      // clear all filters
      ui->decodeView->clearFilters();
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

QtWindow::QtWindow() : impl(std::make_unique<Impl>(this))
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

QtWindow::~QtWindow() = default;

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
   else if (event->type() == TargetStateEvent::Type)
      impl->targetStateEvent(dynamic_cast<TargetStateEvent *>(event));
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
