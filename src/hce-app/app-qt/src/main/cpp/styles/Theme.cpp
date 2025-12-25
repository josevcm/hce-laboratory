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

#ifdef _WIN32

#include <dwmapi.h>

#endif

#include <QWidget>
#include <QEventLoop>

#include "Theme.h"

const QIcon Theme::sortUpIcon(QIcon::fromTheme("caret-up-filled"));
const QIcon Theme::sortDownIcon(QIcon::fromTheme("caret-down-filled"));
const QIcon Theme::filterEmptyIcon(QIcon::fromTheme("filter-empty"));
const QIcon Theme::filterFilledIcon(QIcon::fromTheme("filter-filled"));
const QIcon Theme::filterFilledVoidIcon(QIcon::fromTheme("filter-filled-void"));

const QIcon Theme::startupIcon(QIcon::fromTheme("frame-startup"));
const QIcon Theme::requestIcon(QIcon::fromTheme("frame-request"));
const QIcon Theme::responseIcon(QIcon::fromTheme("frame-response"));
const QIcon Theme::carrierOnIcon(QIcon::fromTheme("carrier-on"));
const QIcon Theme::carrierOffIcon(QIcon::fromTheme("carrier-off"));

const QColor Theme::defaultTextColor = QColor(0xE0, 0xE0, 0xE0, 0xff);
const QPen Theme::defaultTextPen = makePen(defaultTextColor);
const QFont Theme::defaultTextFont = makeFont("Verdana", 9, QFont::Normal);
const QFont Theme::monospaceTextFont = makeFont("Verdana", 9, QFont::Normal, false, QFont::TypeWriter);

const QColor Theme::defaultLabelColor({0xF0, 0xF0, 0xF0, 0xFF});
const QPen Theme::defaultLabelPen({0x2B, 0x2B, 0x2B, 0x70});
const QBrush Theme::defaultLabelBrush({0x2B, 0x2B, 0x2B, 0xC0});
const QFont Theme::defaultLabelFont("Roboto", 9, QFont::Normal);

const QPen Theme::defaultSelectionPen = makePen({0x00, 0x80, 0xFF, 0x50});
const QBrush Theme::defaultSelectionBrush = makeBrush({0x00, 0x80, 0xFF, 0x50});
const QPen Theme::defaultActiveSelectionPen = makePen({0x00, 0x80, 0xFF, 0x50});
const QBrush Theme::defaultActiveSelectionBrush = makeBrush({0x00, 0x80, 0xFF, 0x50});

int Theme::messageDialog(QWidget *parent, const QString &title, const QString &text, QMessageBox::Icon icon, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
   QMessageBox messageBox(parent);

   messageBox.setIcon(icon);
   messageBox.setWindowTitle(title);
   messageBox.setText(text);
   messageBox.setStandardButtons(buttons);
   messageBox.setDefaultButton(defaultButton);

   return showModalInDarkMode(&messageBox);
}

QString Theme::openFileDialog(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
   QFileDialog fileDialog(parent, caption, dir, filter);

   fileDialog.setOptions(options);
   fileDialog.setFileMode(QFileDialog::ExistingFile);

   //   if (showModalInDarkMode(&fileDialog) == QDialog::Accepted)
   if (fileDialog.exec() == QDialog::Accepted)
   {
      if (selectedFilter)
         *selectedFilter = fileDialog.selectedNameFilter();

      return fileDialog.selectedFiles().value(0);
   }

   return {};
}

QString Theme::saveFileDialog(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
   QFileDialog fileDialog(parent, caption, dir, filter);

   fileDialog.setOptions(options);
   fileDialog.setAcceptMode(QFileDialog::AcceptSave);

   //   if (showModalInDarkMode(&fileDialog) == QDialog::Accepted)
   if (fileDialog.exec() == QDialog::Accepted)
   {
      if (selectedFilter)
         *selectedFilter = fileDialog.selectedNameFilter();

      return fileDialog.selectedFiles().value(0);
   }

   return {};
}

int Theme::showModalInDarkMode(QDialog *dialog)
{
   QEventLoop loop;

   // show widget in dark mode
   showInDarkMode(dialog);

   // connect signal to event loop fon synchronous wait
   QObject::connect(dialog, &QDialog::finished, &loop, &QEventLoop::exit);

   // synchronous wait
   return loop.exec();
}

void Theme::showInDarkMode(QWidget *widget)
{
   widget->show();

#ifdef _WIN32
   // BOOL darkMode = TRUE;
   // DwmSetWindowAttribute((HWND) widget->winId(), 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
#endif
}

QPen Theme::makePen(const QColor &color, Qt::PenStyle style, float width)
{
   QPen pen(color);

   pen.setStyle(style);
   pen.setWidthF(width);

   return pen;
}

QBrush Theme::makeBrush(const QColor &color, Qt::BrushStyle style)
{
   QBrush brush(color);

   brush.setStyle(style);;

   return brush;
}

QFont Theme::makeFont(const QString &family, int pointSize, int weight, bool italic, int hint)
{
   QFont font(family, pointSize, weight, italic);

   if (hint >= 0)
      font.setStyleHint(QFont::StyleHint(hint));

   return font;
}
