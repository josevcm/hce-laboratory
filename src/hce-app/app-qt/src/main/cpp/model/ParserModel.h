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

#ifndef APP_FRAME_MODEL_H
#define APP_FRAME_MODEL_H

#include <QObject>
#include <QVariant>
#include <QModelIndex>
#include <QAbstractItemModel>
#include <QList>
#include <QSharedPointer>

#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QPointer>

#include <protocol/ProtocolFrame.h>

namespace hce {
class Frame;
}

class ParserModel : public QAbstractItemModel
{
      struct Impl;

   Q_OBJECT

   public:

      enum Columns
      {
         Name = 0, Flags = 1, Data = 2
      };

   public:

      ParserModel(QObject *parent = nullptr);

      QVariant data(const QModelIndex &index, int role) const override;

      Qt::ItemFlags flags(const QModelIndex &index) const override;

      QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

      QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

      QModelIndex parent(const QModelIndex &index) const override;

      bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

      int rowCount(const QModelIndex &parent = QModelIndex()) const override;

      int columnCount(const QModelIndex &parent = QModelIndex()) const override;

      bool insertRows(int position, int rows, const QModelIndex &parent) override;

      void resetModel();

      void append(const hce::Frame &frame);

      ProtocolFrame *entry(const QModelIndex &index) const;

   signals:

      void modelChanged();

   private:

      QSharedPointer<Impl> impl;
};

#endif
