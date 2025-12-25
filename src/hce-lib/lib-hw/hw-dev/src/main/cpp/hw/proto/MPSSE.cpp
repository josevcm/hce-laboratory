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

#include <unistd.h>
#include <windows.h>

#include "rt/Finally.h"

#if LIBFTDI1 == 1
#include <libftdi1/ftdi.h>
#else
#include <ftdi.h>
#endif

#include <rt/Logger.h>
#include <rt/ByteBuffer.h>

#include <hw/dev/Usb.h>

#include <hw/proto/MPSSE.h>

#define CHUNK_SIZE		            4096

#define SPI_RW_SIZE	               (63 * 1024)
#define SPI_TRANSFER_SIZE	         512
#define I2C_TRANSFER_SIZE	         64

#define LATENCY_MS                  1
#define TIMEOUT_DIVISOR             1000000
#define USB_TIMEOUT                 500
#define SETUP_DELAY                 25000

#define CMD_SET_BITS_ADBUS          0x80
#define CMD_SET_BITS_ACBUS          0x82
#define CMD_SEND_IMMEDIATE          0x87
#define CMD_WAIT_ON_HIGH            0x88
#define CMD_WAIT_ON_LOW             0x89
#define CMD_INVALID                 0xAB
#define CMD_ENABLE_ADAPTIVE_CLOCK   0x96
#define CMD_DISABLE_ADAPTIVE_CLOCK  0x97
#define CMD_TCK_DIVISOR             0x86
#define CMD_ENABLE_3_PHASE_CLOCK    0x8C
#define CMD_DISABLE_3_PHASE_CLOCK   0x8D
#define CMD_TCK_X5                  0x8A
#define CMD_TCK_D5                  0x8B
#define CMD_CLOCK_N_CYCLES		      0x8E
#define CMD_CLOCK_N8_CYCLES		   0x8F
#define CMD_PULSE_CLOCK_IO_HIGH	   0x94
#define CMD_PULSE_CLOCK_IO_LOW      0x95
#define CMD_CLOCK_N8_CYCLES_IO_HIGH 0x9C
#define CMD_CLOCK_N8_CYCLES_IO_LOW  0x9D
#define CMD_TRISTATE_IO             0x9E

/* SK and CS are high, all others low */
#define DEFAULT_PORT (SK | CS)

/* SK/DO/CS and GPIOs are outputs, DI is an input */
#define DEFAULT_TRISL (SK | DO | CS | GPIO0 | GPIO1 | GPIO2 | GPIO3)

#define DEFAULT_TRISH 0xFF

#define MSB 0x00
#define LSB 0x08

namespace hw {

struct ftdi_profile
{
   int vid;
   int pid;
   const char *description;
};

ftdi_profile ftdi_profiles[] = {
   {0x0403, 0x6010, "FT2232 Future Technology Devices International, Ltd"},
   {0x0403, 0x6011, "FT4232 Future Technology Devices International, Ltd"},
   {0x0403, 0x6014, "FT232H Future Technology Devices International, Ltd"},

   /* These devices are based on FT2232 chips, but have not been tested. */
   {0x0403, 0x8878, "Bus Blaster v2 (channel A)"},
   {0x0403, 0x8879, "Bus Blaster v2 (channel B)"},
   {0x0403, 0xBDC8, "Turtelizer JTAG/RS232 Adapter A"},
   {0x0403, 0xCFF8, "Amontec JTAGkey"},
   {0x0403, 0x8A98, "TIAO Multi Protocol Adapter"},
   {0x15BA, 0x0003, "Olimex Ltd. OpenOCD JTAG"},
   {0x15BA, 0x0004, "Olimex Ltd. OpenOCD JTAG TINY"},
};

struct mpsse_mode
{
   uint8_t pstart;
   uint8_t pstop;
   uint8_t pidle;
   uint8_t gpiol;
   uint8_t gpioh;
   uint8_t trisl;
   uint8_t trish;
   uint8_t bitbang;
   uint8_t tx;
   uint8_t rx;
   uint8_t txrx;
   uint8_t tack;
   uint8_t rack;
};

enum mpsse_pins
{
   SK = 1,
   DO = 2,
   DI = 4,
   CS = 8,
   GPIO0 = 16,
   GPIO1 = 32,
   GPIO2 = 64,
   GPIO3 = 128
};

enum gpio_pins
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

enum mpsse_status
{
   Stopped = 0,
   Started = 1
};

struct MPSSE::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.MPSSE");

   Protocol protocol;

   unsigned int clock;
   unsigned int interface;
   std::string description;
   std::string serial;

   int status = Stopped;
   int txsize = 0;

   mpsse_mode mode {};

   ftdi_profile *profile = nullptr;

   ftdi_context *ftdi = nullptr;

   Impl()
   {
      // ftdilib initialization
      if (ftdi = ftdi_new(); ftdi == nullptr)
         log->warn("error initializing FTDI");
   }

   ~Impl()
   {
      close();

      if (ftdi)
         ftdi_free(ftdi);
   }

   int open(const Protocol protocol, const unsigned int clock, const Endianess endianess)
   {
      close();

      Usb::Descriptor descriptor {};

      for (const auto &dev: Usb::list())
      {
         // search for Sipeed device profile
         for (auto &p: ftdi_profiles)
         {
            if (!(dev.vid == p.vid && dev.pid == p.pid))
               continue;

            // profile detected
            profile = &p;

            // descriptor
            descriptor = dev;

            break;
         }

         if (profile)
            break;
      }

      if (!profile)
      {
         log->warn("no FTDI device found!");
         return false;
      }

      LOG_INFO(log, "open device {} {} on bus {03} device {03}", { descriptor.manufacturer, descriptor.product, descriptor.bus, descriptor.address});

      int res = 0;

      while (true)
      {
         /* Set the read and write timeout periods */
         ftdi->usb_read_timeout = USB_TIMEOUT;
         ftdi->usb_write_timeout = USB_TIMEOUT;

         /* Set the FTDI interface  */
         if (res = ftdi_set_interface(ftdi, INTERFACE_A); res != 0)
            break;

         /* Open the specified device */
         if (res = ftdi_usb_open_desc_index(ftdi, descriptor.vid, descriptor.pid, nullptr, nullptr, 0); res != 0)
            break;

         if (res = ftdi_usb_reset(ftdi); res != 0)
            break;

         if (res = ftdi_set_latency_timer(ftdi, LATENCY_MS); res != 0)
            break;

         if (res = ftdi_write_data_set_chunksize(ftdi, CHUNK_SIZE); res != 0)
            break;

         if (res = ftdi_read_data_set_chunksize(ftdi, CHUNK_SIZE); res != 0)
            break;

         if (res = ftdi_set_bitmode(ftdi, 0, BITMODE_MPSSE); res != 0)
            break;

         if (!setClock(clock))
            break;

         if (!setMode(protocol, endianess))
            break;

         /* Give the chip a few mS to initialize */
         usleep(SETUP_DELAY);

         /*
          * Not all FTDI chips support all the commands that SetMode may have sent.
          * This clears out any errors from unsupported commands that might have been sent during set up.
          */
         ftdi_tciflush(ftdi);

         // set port properties
         this->status = Stopped;
         this->txsize = protocol == I2C ? I2C_TRANSFER_SIZE : SPI_RW_SIZE;

         LOG_INFO(log, "device {} ready!", { profile->description});

         return true;
      }

      log->warn("failed to open device: {}", {ftdiError()});

      ftdi_deinit(ftdi);

      profile = nullptr;

      return false;
   }

   void close()
   {
      if (profile)
         ftdi_deinit(ftdi);

      profile = nullptr;
   }

   int start()
   {
      if (!profile)
         return false;

      if (protocol == I2C && status == Started)
      {
         /* Set the default pin states while the clock is low since this is an I2C repeated start condition */
         if (!ftdiGpioLow(mode.pidle & ~SK))
            return false;

         /* Make sure the pins are in their default idle state */
         if (!ftdiGpioLow(mode.pidle))
            return false;
      }

      /* Set the start condition */
      if (!ftdiGpioLow(mode.pstart))
         return false;

      /*
       * Hackish work around to properly support SPI mode 3.
       * SPI3 clock idles high, but needs to be set low before sending out
       * data to prevent unintenteded clock glitches from the FT2232.
       */
      if (protocol == SPI3)
      {
         if (!ftdiGpioLow(mode.pstart & ~SK))
            return false;
      }

      /*
       * Hackish work around to properly support SPI mode 1.
       * SPI1 clock idles low, but needs to be set high before sending out
       * data to preven unintended clock glitches from the FT2232.
       */
      if (protocol == SPI1)
      {
         if (!ftdiGpioLow(mode.pstart | SK))
            return false;
      }

      status = Started;

      return true;
   }

   int stop()
   {
      if (!profile)
         return false;

      /* In I2C mode, we need to ensure that the data line goes low while the clock line is low to avoid sending an inadvertent start condition */
      if (protocol == I2C)
      {
         /* Set the default pin states while the clock is low since this is an I2C repeated start condition */
         if (!ftdiGpioLow(mode.pidle & ~DO & ~SK))
            return false;
      }

      /* Send the stop condition */
      if (!ftdiGpioLow(mode.pstop))
         return false;

      /* Restore the pins to their idle states */
      if (!ftdiGpioLow(mode.pidle))
         return false;

      status = Stopped;

      return true;

   }

   bool read(rt::ByteBuffer &data, const int timeout)
   {
      if (!profile)
         return false;

      LOG_INFO(log, "read {} bytes", {data.remaining()});

      rt::ByteBuffer cmd(16);

      // I2C mode
      if (protocol == I2C)
      {
         rt::ByteBuffer ack(1);

         // ensure that the clock pin is set low prior to clocking out data
         cmd.put(CMD_SET_BITS_ADBUS).put(mode.pstart & ~SK).put(mode.trisl & ~DO);

         // now add RX command, data length 0 = required bytes - 1
         cmd.put(mode.rx).put(data.remaining() - 1).put(0);

         // we need to make data out an input to avoid contention
         cmd.put(CMD_SET_BITS_ADBUS).put(mode.pstart & ~SK).put(mode.trisl);

         // and read ACK after each byte
         cmd.put(mode.rx | MPSSE_BITMODE).put(0).put(mode.tack);

         // prepare buffer for send
         cmd.flip();

         // send read request!
         if (!ftdiSend(cmd))
         {
            log->error("failed to send read request in I2C mode");
            return false;
         }

         // and read data back
         if (!ftdiRecv(data, timeout))
         {
            log->error("failed to read data in I2C mode");
            return false;
         }
      }

      // SPI mode
      else
      {
         while (data.remaining() > 0)
         {
            rt::ByteBuffer block(data.remaining() > txsize ? txsize : data.remaining());

            // now add TX command, data length 0 = 1 byte for I2C
            cmd.put(mode.rx);
            cmd.putInt(block.remaining() - 1, 2);
            cmd.flip();

            // send read request!
            if (!ftdiSend(cmd))
            {
               log->error("failed to send read request in I2C mode");
               return false;
            }

            // and read data back
            if (!ftdiRecv(block, timeout))
            {
               log->error("failed to read data in I2C mode");
               return false;
            }

            // add block to data buffer
            data.put(block);

            // prepare for next block
            cmd.clear();
         }

         data.flip();
      }

      LOG_DEBUG(log, "MPSSE RX: {x}", {data.copy()});

      return true;
   }

   bool write(const rt::ByteBuffer &data)
   {
      if (!profile)
         return false;

      LOG_INFO(log, "write {} bytes", {data.remaining()});

      LOG_DEBUG(log, "MPSSE TX: {x}", {data.copy()});

      // I2C mode
      if (protocol == I2C)
      {
         rt::ByteBuffer cmd(16);
         rt::ByteBuffer ack(1);

         for (int i = 0; i < data.remaining(); i++)
         {
            // ensure that the clock pin is set low prior to clocking out data
            cmd.put(CMD_SET_BITS_ADBUS).put(mode.pstart & ~SK).put(mode.trisl);

            // now add TX command, data length 0 = 1 byte for I2C
            cmd.put(mode.tx).put(0).put(0).put(data[i]);

            // we need to make data out an input to avoid contention
            cmd.put(CMD_SET_BITS_ADBUS).put(mode.pstart & ~SK).put(mode.trisl & ~DO);

            // and read ACK after each byte
            cmd.put(mode.rx | MPSSE_BITMODE).put(0).put(CMD_SEND_IMMEDIATE);

            // prepare buffer for send
            cmd.flip();

            // prepare buffer for receive ACK
            ack.clear();

            // send!
            if (!ftdiSend(cmd))
            {
               log->error("failed to send write request in I2C mode");
               return false;
            }

            // and read back ACK
            if (!ftdiRecv(ack))
            {
               log->error("failed to read ACK in I2C mode");
               return false;
            }

            // store ack
            mode.tack = ack.get();
         }

         return true;
      }

      // SPI mode
      rt::ByteBuffer block(txsize);
      rt::ByteBuffer payload(data);
      rt::ByteBuffer cmd(txsize + 3);

      while (payload.remaining() > 0)
      {
         payload.get(block);

         // now add TX command, data length 0 = 1 byte for I2C
         cmd.put(mode.tx);
         cmd.putInt(block.remaining() - 1, 2);
         cmd.put(block);
         cmd.flip();

         // send!
         if (!ftdiSend(cmd))
         {
            log->error("failed to write data in SPI mode");
            return false;
         }

         // prepare for next block
         cmd.clear();
      }

      return true;
   }

   bool setClock(const unsigned int freq)
   {
      LOG_INFO(log, "setClock, frequency {}Hz", {freq});

      const unsigned int sck = freq > CLK_6MHZ ? CLK_6MHZ : CLK_12MHZ;
      const unsigned int div = freq > 0 ? (sck / freq) / 2 - 1 : 0xFFFF;

      rt::ByteBuffer cmd1(32);
      cmd1.put(freq > CLK_6MHZ ? CMD_TCK_X5 : CMD_TCK_D5).flip();

      if (!ftdiSend(cmd1))
         return false;

      rt::ByteBuffer cmd2(32);
      cmd2.put(CMD_TCK_DIVISOR);
      cmd2.putInt(div, 2);
      cmd2.flip();

      if (!ftdiSend(cmd2))
         return false;

      clock = sck / ((1 + div) * 2);

      return true;
   }

   int setMode(const Protocol proto, const Endianess bo)
   {
      LOG_INFO(log, "setMode, protocol: {}, endianess: {}", {proto, bo});

      const unsigned char bits = (bo == BIG_ENDIAN ? MSB : LSB);

      mode.tx = MPSSE_DO_WRITE | bits;
      mode.rx = MPSSE_DO_READ | bits;
      mode.txrx = MPSSE_DO_WRITE | MPSSE_DO_READ | bits;
      mode.trisl = DEFAULT_TRISL; // Clock, data out, chip select pins are outputs; all others are inputs.
      mode.pidle = DEFAULT_PORT; // Clock and chip select pins idle high; all others are low
      mode.pstart = DEFAULT_PORT; // Clock and chip select pins idle high; all others are low
      mode.pstop = DEFAULT_PORT; // Clock and chip select pins idle high; all others are low
      mode.pstart &= ~CS; // During reads and writes the chip select pin is brought low
      mode.gpiol = 0x00;

      // All GPIO pins are outputs, set low
      mode.trish = DEFAULT_TRISH;
      mode.gpioh = 0x00;

      // set ACK by default
      mode.tack = 0x00;

      rt::ByteBuffer cmd(32);

      // Ensure adaptive clock is disabled
      cmd.put(CMD_DISABLE_ADAPTIVE_CLOCK);

      switch (proto)
      {
         case SPI0:
            mode.pidle &= ~SK;
            mode.pstart &= ~SK;
            mode.pstop &= ~SK;
            mode.tx |= MPSSE_WRITE_NEG;
            mode.rx &= ~MPSSE_READ_NEG;
            mode.txrx |= MPSSE_WRITE_NEG;
            mode.txrx &= ~MPSSE_READ_NEG;
            break;
         case SPI3:
            mode.pidle |= SK;
            mode.pstart |= SK;
            mode.pstop &= ~SK;
            mode.tx |= MPSSE_WRITE_NEG;
            mode.rx &= ~MPSSE_READ_NEG;
            mode.txrx |= MPSSE_WRITE_NEG;
            mode.txrx &= ~MPSSE_READ_NEG;
            break;
         case SPI1:
            mode.pidle &= ~SK;
            mode.pstart &= ~SK;
            mode.pstop |= SK;
            mode.rx |= MPSSE_READ_NEG;
            mode.tx &= ~MPSSE_WRITE_NEG;
            mode.txrx |= MPSSE_READ_NEG;
            mode.txrx &= ~MPSSE_WRITE_NEG;
            break;
         case SPI2:
            mode.pidle |= SK;
            mode.pstart |= SK;
            mode.pstop |= SK;
            mode.rx |= MPSSE_READ_NEG;
            mode.tx &= ~MPSSE_WRITE_NEG;
            mode.txrx |= MPSSE_READ_NEG;
            mode.txrx &= ~MPSSE_WRITE_NEG;
            break;
         case I2C:
            mode.tx |= MPSSE_WRITE_NEG;
            mode.rx &= ~MPSSE_READ_NEG;
            mode.pidle |= DO | DI;
            mode.pstart &= ~DO & ~DI;
            mode.pstop &= ~DO & ~DI;
            cmd.put(CMD_ENABLE_3_PHASE_CLOCK);
            break;
         default:
            return false;
      }

      cmd.flip();

      if (!ftdiSend(cmd))
         return false;

      if (!ftdiGpioLow(mode.pidle))
         return false;

      if (!ftdiGpioHigh(mode.gpioh))
         return false;

      protocol = proto;

      return true;
   }

   /* get the GPIO pins high/low */
   int getGpio(const int gpio) const
   {
      unsigned char states;

      if (!ftdiReadPins(states))
         return -1;

      const int pin = gpio + GPIOH0;

      return ((states & (1 << pin)) >> pin);
   }

   /* set the GPIO pins high/low */
   bool setGpio(const int gpio, const int value)
   {
      // ADBUS GPIO
      if (gpio >= GPIOL0 && gpio <= GPIOL3 && status == Stopped)
      {
         const int pin = GPIO0 << gpio;

         if (value)
         {
            mode.pstart |= pin;
            mode.pstop |= pin;
            mode.pidle |= pin;
         }
         else
         {
            mode.pstart &= ~pin;
            mode.pstop &= ~pin;
            mode.pidle &= ~pin;
         }

         return ftdiGpioLow(mode.pstart);
      }

      if (gpio >= GPIOH0 && gpio <= GPIOH7)
      {
         // Convert pin number (4 - 11) to the corresponding pin bit
         const int pin = 1 << (gpio - GPIOH0);

         if (value)
            mode.gpioh |= pin;
         else
            mode.gpioh &= ~pin;

         return ftdiGpioHigh(mode.gpioh);
      }

      return false;
   }

   int ftdiReadPins(unsigned char &val) const
   {
      if (ftdi_read_pins(ftdi, &val) < 0)
         return false;

      return true;
   }

   int ftdiGpioLow(const int value) const
   {
      rt::ByteBuffer cmd(32);
      cmd.put(CMD_SET_BITS_ADBUS).put(value).put(mode.trisl).flip();
      return ftdiSend(cmd);
   }

   int ftdiGpioHigh(const int value) const
   {
      rt::ByteBuffer cmd(32);
      cmd.put(CMD_SET_BITS_ACBUS).put(value).put(mode.trish).flip();
      return ftdiSend(cmd);
   }

   int ftdiLoopback(const bool enable) const
   {
      rt::ByteBuffer cmd(32);
      cmd.put(enable ? LOOPBACK_START : LOOPBACK_END).flip();
      return ftdiSend(cmd);
   }

   bool ftdiSend(const rt::ByteBuffer &data) const
   {
      LOG_DEBUG(log, "FTDI TX: {x}", {data.copy()});

      if (const auto res = ftdi_write_data(ftdi, data.ptr(), static_cast<int>(data.remaining())); res != data.remaining())
         return false;

      return true;
   }

   bool ftdiRecv(rt::ByteBuffer &data, const int timeout = -1) const
   {
      int r = 0;

      // set FTDI USB timeout
      if (timeout >= 0)
         ftdi->usb_read_timeout = timeout;

      // always restore timeout on finish
      rt::Finally restore([&] { ftdi->usb_read_timeout = USB_TIMEOUT; });

      while (data.remaining() > 0)
      {
         // read next data block
         if (r = ftdi_read_data(ftdi, data.ptr(), static_cast<int>(data.remaining())); r < 0)
            return false;

         data.skip(r);
      }

      data.flip();

      LOG_DEBUG(log, "FTDI RX: {x}", {data.copy()});

      return true;
   }

   std::string deviceName() const
   {
      if (!profile)
         return "";

      return {profile->description};
   }

   std::string ftdiError() const
   {
      if (!ftdi)
         return "ftdi library initialization error";

      if (!profile)
         return "no device opened";

      return {ftdi_get_error_string(ftdi)};
   }

};

MPSSE::MPSSE() : impl(std::make_shared<Impl>())
{
}

bool MPSSE::open(const Protocol protocol, const unsigned int clock, Endianess endianess)
{
   return impl->open(protocol, clock, endianess);
}

void MPSSE::close()
{
   return impl->close();
}

bool MPSSE::start() const
{
   return impl->start();
}

bool MPSSE::stop() const
{
   return impl->stop();
}

bool MPSSE::read(rt::ByteBuffer &data, int timeout) const
{
   return impl->read(data, timeout);
}

bool MPSSE::write(const rt::ByteBuffer &data) const
{
   return impl->write(data);
}

bool MPSSE::queue(std::function<void(Queue *ops)> &batch) const
{
   return false;
}

int MPSSE::getGpio(const GPIO gpio) const
{
   return impl->getGpio(gpio);
}

bool MPSSE::setGpio(const GPIO gpio, const int value) const
{
   return impl->setGpio(gpio, value);
}

int MPSSE::getClock() const
{
   return impl->clock;
}

bool MPSSE::setClock(unsigned int clock) const
{
   return impl->setClock(clock);
}

std::string MPSSE::deviceName() const
{
   return impl->deviceName();
}

std::string MPSSE::errorString() const
{
   return impl->ftdiError();
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
