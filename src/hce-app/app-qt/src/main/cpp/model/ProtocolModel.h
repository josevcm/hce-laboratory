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

#ifndef APP_PROTOCOL_MODEL_H
#define APP_PROTOCOL_MODEL_H

#include <QAbstractTableModel>

namespace hce {
class Frame;
}

class ProtocolModel : public QAbstractTableModel
{
      Q_OBJECT

   public:

      enum Columns
      {
         Time = 0,
         Command = 1,
         Status = 2,
         Detail = 3,
      };

      enum Roles
      {
         RawRequestRole = Qt::UserRole,
         RawResponseRole = Qt::UserRole + 1,
         RequestTimeRole = Qt::UserRole + 2,
         ResponseTimeRole = Qt::UserRole + 3,
      };

   public:

      explicit ProtocolModel(QObject *parent = nullptr);

      ~ProtocolModel() override;

      int rowCount(const QModelIndex &parent = QModelIndex()) const override;

      int columnCount(const QModelIndex &parent = QModelIndex()) const override;

      QVariant data(const QModelIndex &index, int role) const override;

      QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

      Qt::ItemFlags flags(const QModelIndex &index) const override;

      void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

      void append(const hce::Frame &frame);

      void resetModel();

   private:

      struct Impl;
      std::unique_ptr<Impl> impl;
};

#endif

