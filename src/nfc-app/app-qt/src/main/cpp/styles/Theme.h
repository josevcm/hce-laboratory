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

#ifndef HCE_LAB_THEME_H
#define HCE_LAB_THEME_H

#include <QPen>
#include <QBrush>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QFileDialog>

class Theme
{
   public:

      static const QIcon sortUpIcon;
      static const QIcon sortDownIcon;
      static const QIcon filterEmptyIcon;
      static const QIcon filterFilledIcon;
      static const QIcon filterFilledVoidIcon;

      static const QIcon startupIcon;
      static const QIcon requestIcon;
      static const QIcon responseIcon;
      static const QIcon carrierOnIcon;
      static const QIcon carrierOffIcon;

      static const QColor defaultTextColor;
      static const QPen defaultTextPen;
      static const QFont defaultTextFont;
      static const QFont monospaceTextFont;

      static const QColor defaultLabelColor;
      static const QPen defaultLabelPen;
      static const QBrush defaultLabelBrush;
      static const QFont defaultLabelFont;

      static const QPen defaultSelectionPen;
      static const QBrush defaultSelectionBrush;
      static const QPen defaultActiveSelectionPen;
      static const QBrush defaultActiveSelectionBrush;

   public:

      static int messageDialog(QWidget *parent, const QString &title, const QString &text, QMessageBox::Icon icon = QMessageBox::Information, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

      static QString openFileDialog(QWidget *parent = nullptr, const QString &caption = QString(), const QString &dir = QString(), const QString &filter = QString(), QString *selectedFilter = nullptr, QFileDialog::Options options = QFileDialog::Options());

      static QString saveFileDialog(QWidget *parent = nullptr, const QString &caption = QString(), const QString &dir = QString(), const QString &filter = QString(), QString *selectedFilter = nullptr, QFileDialog::Options options = QFileDialog::Options());

      static void showInDarkMode(QWidget *widget);

      static int showModalInDarkMode(QDialog *dialog);

   private:

      static QPen makePen(const QColor &color, Qt::PenStyle style = Qt::SolidLine, float width = 1);

      static QBrush makeBrush(const QColor &color, Qt::BrushStyle style = Qt::SolidPattern);

      static QFont makeFont(const QString &family, int pointSize = -1, int weight = -1, bool italic = false, int hint = -1);
};

#endif // HCE_LAB_THEME_H
