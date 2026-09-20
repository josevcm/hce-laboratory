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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <libkftdi_mpsse.h>
#include <iterator>

#include <rt/Logger.h>
#include <hw/proto/MPSSE.h>

namespace hw {

static inline bool gpioIsAd(MPSSE::GPIO g) { return g <= MPSSE::GPIOL3; }
static inline BYTE gpioBit(MPSSE::GPIO g)  { return gpioIsAd(g) ? static_cast<BYTE>(g + 4) : static_cast<BYTE>(g - MPSSE::GPIOH0); }

struct MPSSE::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.MPSSE");

   KFTDI_MPSSE_SPI_HANDLE handle {};
   bool opened = false;
   unsigned int clock = 0;
   FT_STATUS status = FT_OK;
   int deviceNumber = 0;
   std::string description;

   ~Impl()
   {
      close();
   }

   int open(const Protocol protocol, const unsigned int freq, const ByteOrder endianess) // only first device at this time
   {
      close();

      if (protocol != SPI0)
      {
         log->error("D2XX backend supports SPI mode 0 only (PN7160)");
         return RESULT_ERROR;
      }

      BYTE mode = KFTDI_MPSSE_SPI_MODE_0 | ((endianess == BYTEORDER_BIG_ENDIAN) ? KFTDI_MPSSE_SPI_MODE_MSB : KFTDI_MPSSE_SPI_MODE_LSB);

      if (!FT_SUCCESS(status = KFTDI_MPSSE_SPI_Open(deviceNumber, mode, freq, &handle)))
      {
         log->warn("KFTDI_MPSSE_SPI_Open failed: {}", {errorString()});
         return RESULT_ERROR;
      }

      opened = true;
      clock = freq;

      FT_SetLatencyTimer(handle.ftHandle, 1);

      char desc[64] = {0};
      FT_DEVICE type;
      DWORD id;

      if (FT_SUCCESS(FT_GetDeviceInfo(handle.ftHandle, &type, &id, nullptr, desc, nullptr)))
         description = desc;

      LOG_INFO(log, "D2XX device ready ({}), {}Hz", {description, clock});

      return RESULT_OK;
   }

   void close()
   {
      if (opened)
      {
         KFTDI_MPSSE_SPI_Close(&handle);
         opened = false;
      }
   }

   int start()
   {
      return FT_SUCCESS(status = KFTDI_MPSSE_SPI_CS_LOW(&handle)) ? RESULT_OK : RESULT_ERROR;
   }

   int stop()
   {
      return FT_SUCCESS(status = KFTDI_MPSSE_SPI_CS_HIGH(&handle)) ? RESULT_OK : RESULT_ERROR;
   }

   int write(const rt::ByteBuffer &data)
   {
      const DWORD length = data.remaining();

      if (length == 0)
         return RESULT_OK;

      status = KFTDI_MPSSE_SPI_WRITE_EX(&handle, data.ptr(), length);

      return FT_SUCCESS(status) ? RESULT_OK : RESULT_ERROR;
   }

   int read(rt::ByteBuffer &data, const int timeout)
   {
      const int want = data.remaining();

      if (want <= 0)
         return RESULT_OK;

      if (timeout >= 0)
         FT_SetTimeouts(handle.ftHandle, timeout, timeout);

      status = KFTDI_MPSSE_SPI_READ_EX(&handle, data.ptr(), static_cast<DWORD>(want));

      if (timeout >= 0)
         FT_SetTimeouts(handle.ftHandle, 5000, 5000); // restore default

      if (!FT_SUCCESS(status))
         return RESULT_ERROR;

      data.skip(want);
      data.flip();

      return RESULT_OK;
   }

   int getGpio(const GPIO gpio)
   {
      BYTE v;
      status = (gpioIsAd(gpio) ? KFTDI_MPSSE_SPI_GPIO_AD_GetPin : KFTDI_MPSSE_SPI_GPIO_AC_GetPin)(&handle, gpioBit(gpio), &v);
      return FT_SUCCESS(status) ? v : RESULT_ERROR;
   }

   int setGpio(const GPIO gpio, const int value)
   {
      status = (gpioIsAd(gpio) ? KFTDI_MPSSE_SPI_GPIO_AD_SetPinDirValue : KFTDI_MPSSE_SPI_GPIO_AC_SetPinDirValue)(&handle, gpioBit(gpio), PIN_OUTPUT, value ? PIN_HIGH : PIN_LOW);
      return FT_SUCCESS(status) ? RESULT_OK : RESULT_ERROR;
   }

   int setClock(const unsigned int freq)
   {
	  status = KFTDI_MPSSE_SPI_SetFrequency(&handle, freq);
      if (FT_SUCCESS(status))
	  {
		 clock = freq;
		 return RESULT_OK;
	  }
	  else return RESULT_ERROR;
   }

   std::string errorString() const
   {
      static const char *names[] = { "OK", "INVALID_HANDLE", "DEVICE_NOT_FOUND", "DEVICE_NOT_OPENED", "IO_ERROR", "INSUFFICIENT_RESOURCES", "INVALID_PARAMETER" };

      if (status < std::size(names))
	  {
         return std::string("FT_") + names[status];
	  }
	  else return "FT_STATUS_" + std::to_string(status);
   }
};

MPSSE::MPSSE() : impl(std::make_shared<Impl>())
{
}

int MPSSE::open(const Protocol protocol, const unsigned int clock, ByteOrder endianess)
{
   return impl->open(protocol, clock, endianess);
}

void MPSSE::close()
{
   return impl->close();
}

int MPSSE::start() const
{
   return impl->start();
}

int MPSSE::stop() const
{
   return impl->stop();
}

int MPSSE::read(rt::ByteBuffer &data, int timeout) const
{
   return impl->read(data, timeout);
}

int MPSSE::write(const rt::ByteBuffer &data) const
{
   return impl->write(data);
}

int MPSSE::queue(std::function<void(Queue *ops)> &batch) const
{
   return RESULT_ERROR;
}

int MPSSE::getGpio(const GPIO gpio) const
{
   return impl->getGpio(gpio);
}

int MPSSE::setGpio(const GPIO gpio, const int value) const
{
   return impl->setGpio(gpio, value);
}

int MPSSE::getClock() const
{
   return static_cast<int>(impl->clock);
}

int MPSSE::setClock(unsigned int clock) const
{
   return impl->setClock(clock);
}

std::string MPSSE::deviceName() const
{
   return impl->description.empty() ? "FTDI D2XX" : impl->description;
}

std::string MPSSE::errorString() const
{
   return impl->errorString();
}

MPSSE::Queue *MPSSE::Queue::start()
{
   return this;
}

MPSSE::Queue *MPSSE::Queue::stop()
{
   return this;
}

MPSSE::Queue *MPSSE::Queue::read(rt::ByteBuffer &data, int timeout)
{
   return this;
}

MPSSE::Queue *MPSSE::Queue::write(const rt::ByteBuffer &data)
{
   return this;
}

}
