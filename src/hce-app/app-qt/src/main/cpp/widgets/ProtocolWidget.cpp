/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2026 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#include <QFont>
#include <QHeaderView>

#include "ProtocolWidget.h"

ProtocolWidget::ProtocolWidget(QWidget *parent) : StreamWidget(parent)
{
   QFont tableFont = font();
   tableFont.setFamily("Courier");
   tableFont.setPointSize(10);
   tableFont.setBold(false);
   setFont(tableFont);

   setObjectName("ProtocolWidget");
   setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
   setShowGrid(false);
   setWordWrap(false);
   setAlternatingRowColors(false);
   setSelectionBehavior(QAbstractItemView::SelectRows);
   setSelectionMode(SingleSelection);

   verticalHeader()->setVisible(false);
   verticalHeader()->setMinimumSectionSize(20);
   verticalHeader()->setDefaultSectionSize(20);
}

ProtocolWidget::~ProtocolWidget() = default;

