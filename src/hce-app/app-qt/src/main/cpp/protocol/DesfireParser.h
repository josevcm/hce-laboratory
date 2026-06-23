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

#ifndef APP_DESFIRE_PARSER_H
#define APP_DESFIRE_PARSER_H

#include <QByteArray>
#include <QString>

class DesfireParser
{
   public:

      enum class RequestKind
      {
         None,
         Native,
         Wrapped,
         Iso
      };

      struct RequestInfo
      {
         bool valid = false;
         RequestKind kind = RequestKind::None;
         unsigned int command = 0;
         QString commandName;
         QString summary;
         QString params;
      };

      struct ResponseInfo
      {
         bool valid = false;
         bool ok = false;
         QString status;
         QString summary;
         QString params;
      };

   public:

      static RequestInfo parseRequest(const QByteArray &apdu) ;

      static ResponseInfo parseResponse(const QByteArray &apdu, const RequestInfo &request) ;

      static QString toHex(const QByteArray &data);

   private:

      static QString commandName(unsigned int command, RequestKind kind);

      static QString nativeStatusName(unsigned int status);

      static QString isoStatusName(unsigned int status);
};

#endif
