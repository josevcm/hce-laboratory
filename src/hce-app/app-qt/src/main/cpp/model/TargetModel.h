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

#ifndef APP_TARGET_MODEL_H
#define APP_TARGET_MODEL_H

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QString>

class TargetItem;

class TargetModel : public QAbstractItemModel
{
      Q_OBJECT

   public:

      enum Columns
      {
         Name = 0, Type = 1
      };

      enum Roles
      {
         BytesRole = Qt::UserRole,
         InfoRole  = Qt::UserRole + 1,
         JsonRole  = Qt::UserRole + 2
      };

   public:

      explicit TargetModel(QObject *parent = nullptr);

      ~TargetModel() override;

      QModelIndex index(int row, int column, const QModelIndex &parent) const override;

      QModelIndex parent(const QModelIndex &index) const override;

      int rowCount(const QModelIndex &parent) const override;

      int columnCount(const QModelIndex &) const override;

      bool hasChildren(const QModelIndex &parent) const override;

      bool insertRows(int position, int rows, const QModelIndex &parent) override;

      QVariant data(const QModelIndex &index, int role) const override;

      QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

      Qt::ItemFlags flags(const QModelIndex &index) const override;

      bool load(const QString &fileName);

      void resetModel();

      void setActiveTarget(int row);

      void clearActiveTarget();

      void updateContent(int row, const QJsonObject &root);

   private:

      void mergeTree(const QModelIndex &parentIndex, TargetItem *existing, TargetItem *newItem);

      struct Impl;
      std::unique_ptr<Impl> impl;
};

#endif
