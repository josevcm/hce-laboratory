/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#ifndef DEV_MPSSE_H
#define DEV_MPSSE_H

#include <string>

#include <rt/ByteBuffer.h>

namespace hw {

class MPSSE
{
   struct Impl;

   public:

      enum Protocol
      {
         I2C = 0,
         SPI0 = 1,
         SPI1 = 2,
         SPI2 = 3,
         SPI3 = 4
      };

      enum ByteOrder
      {
         BYTEORDER_BIG_ENDIAN = 0,
         BYTEORDER_LITTLE_ENDIAN = 1
      };

      enum Clock
      {
         CLK_100KHZ = 100000,
         CLK_400KHZ = 400000,
         CLK_1MHZ = 1000000,
         CLK_2MHZ = 2000000,
         CLK_5MHZ = 5000000,
         CLK_6MHZ = 6000000,
         CLK_10MHZ = 10000000,
         CLK_12MHZ = 12000000,
         CLK_15MHZ = 15000000,
         CLK_30MHZ = 30000000,
         CLK_60MHZ = 60000000
      };

      enum GPIO
      {
         GPIOL0 = 0,
         GPIOL1 = 1,
         GPIOL2 = 2,
         GPIOL3 = 3,
         GPIOH0 = 4,
         GPIOH1 = 5,
         GPIOH2 = 6,
         GPIOH3 = 7,
         GPIOH4 = 8,
         GPIOH5 = 9,
         GPIOH6 = 10,
         GPIOH7 = 11
      };

      enum Wait
      {
         WAIT_LOW = 0,
         WAIT_HIGH = 1
      };

      class Queue
      {
         public:

            Queue *start();

            Queue *stop();

            Queue *read(rt::ByteBuffer &data, int timeout = -1);

            Queue *write(const rt::ByteBuffer &data);
      };

      std::function<void(Queue *ops)> Batch;

   public:

      MPSSE();

      bool open(Protocol protocol, unsigned int clock = 100000, ByteOrder endianess = BYTEORDER_BIG_ENDIAN);

      void close();

      bool start() const;

      bool stop() const;

      bool read(rt::ByteBuffer &data, int timeout = -1) const;

      bool write(const rt::ByteBuffer &data) const;

      bool queue(std::function<void(Queue *ops)> &batch) const;

      int getGpio(GPIO gpio) const;

      bool setGpio(GPIO gpio, int value) const;

      int getClock() const;

      bool setClock(unsigned int clock) const;

      std::string deviceName() const;

      std::string errorString() const;

   private:

      std::shared_ptr<Impl> impl;

};

}

#endif // DEV_MPSSE_H
