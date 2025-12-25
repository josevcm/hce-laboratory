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

// https://github.com/google/casimir/blob/main/src/nci_packets.pdl
// https://github.com/ElectronicCats/ElectronicCats-PN7150
// https://github.com/Strooom/PN7160/blob/main/test/generic/test_nci/test.cpp
// https://community.nxp.com/t5/NFC/What-is-the-response-to-a-CORE-RESET-CMD-command-with-the-PN7150/m-p/1514768#M9630

#include <unistd.h>
#include <windows.h>

#include <rt/Logger.h>
#include <rt/Finally.h>

#include <hw/proto/MPSSE.h>

#include <hw/ic/PN7160.h>

// PN7160 timings in microseconds
#define PN7160_T_WL_DWL   5000  // T_VDD_DWL time (in datasheet 0us)
#define PN7160_T_WL_VDD   5000  // T_VDD_VEN time (in datasheet 0us)
#define PN7160_T_WL_VEN   5000  // T_WL_VEN time (in datasheet 10us)
#define PN7160_T_BOOT     5000  // T-BOOT time (in datasheet 2.5ms)

#define PN7160_DEFAULT_TIMEOUT 500

// PN7160 default pins to FT232H board
#define PN7160_FT232H_IRQ_PIN hw::MPSSE::GPIOL1
#define PN7160_FT232H_DWL_PIN hw::MPSSE::GPIOH2
#define PN7160_FT232H_VEN_PIN hw::MPSSE::GPIOH3

// NCI message types
#define NCI_MT_DATA                    0x00
#define NCI_MT_EVENT_CORE              0x60
#define NCI_MT_EVENT_RF                0x61
#define NCI_MT_EVENT_NFCEE             0x62
#define NCI_MT_EVENT_NFCC              0x63
#define NCI_MT_EVENT_TEST              0x64

// NCI status types
#define NCI_STATUS_OK                  0x00

// NCI operations
#define NCI_OP_CORE_CONN_CREDITS_NTF   0x06

#define NCI_OP_RF_DISCOVERY_NTF        0x03
#define NCI_OP_RF_INTF_ACTIVATED_NTF   0x05
#define NCI_OP_RF_DEACTIVATE_NTF       0x06
#define NCI_OP_RF_FIELD_INFO_NTF       0x07
#define NCI_OP_RF_T3T_POLLING_NTF      0x08

#ifdef interface
#undef interface
#endif

namespace hw {

enum CorePower
{
   POWER_MODE_FULL = 0x00,
   POWER_MODE_STANDBY = 0x01,
   POWER_MODE_AUTONOMOUS = 0x02
};

enum RfDiscovery
{
   RF_DISCOVERY_POLL_PASSIVE_NFC_A = 0x00,
   RF_DISCOVERY_POLL_PASSIVE_NFC_B = 0x01,
   RF_DISCOVERY_POLL_PASSIVE_NFC_F = 0x02,
   RF_DISCOVERY_POLL_PASSIVE_NFC_V = 0x06,
   RF_DISCOVERY_POLL_ACTIVE = 0x03,
   RF_DISCOVERY_LISTEN_PASSIVE_NFC_A = 0x80,
   RF_DISCOVERY_LISTEN_PASSIVE_NFC_B = 0x81,
   RF_DISCOVERY_LISTEN_PASSIVE_NFC_F = 0x82,
   RF_DISCOVERY_LISTEN_ACTIVE = 0x83,
};

enum RfMode
{
   RF_MODE_NONE = 0x00,
   RF_MODE_POLL = 0x01,
   RF_MODE_LISTEN = 0x02,
};

enum RfTechnology
{
   RF_TECHNOLOGY_A = 0x00,
   RF_TECHNOLOGY_B = 0x01,
   RF_TECHNOLOGY_F = 0x02,
   RF_TECHNOLOGY_V = 0x03,
};

enum RfProtocol
{
   RF_PROTOCOL_UNDETERMINED = 0x00,
   RF_PROTOCOL_T1T = 0x01,
   RF_PROTOCOL_T2T = 0x02,
   RF_PROTOCOL_T3T = 0x03,
   RF_PROTOCOL_ISO_DEP = 0x04,
   RF_PROTOCOL_NFC_DEP = 0x05,
   RF_PROTOCOL_NDEF = 0x07
};

enum RfInterface
{
   RF_INTERFACE_NFCEE = 0x00,
   RF_INTERFACE_FRAME = 0x01,
   RF_INTERFACE_ISO_DEP = 0x02,
   RF_INTERFACE_NFC_DEP = 0x03,
   RF_INTERFACE_NDEF = 0x06
};

enum RfRouting
{
   RF_ROUTING_TECH = 0x00,
   RF_ROUTING_PROTO = 0x01,
   RF_ROUTING_AID = 0x02,
   RF_ROUTING_SYSTEM = 0x03,
   RF_ROUTING_APDU = 0x04
};

struct DiscoveryMode
{
   unsigned int mode;
   unsigned int period;
};

struct DiscoveryMap
{
   unsigned int protocol;
   unsigned int mode;
   unsigned int interface;
};

struct ListenRouting
{
   unsigned int type;
   rt::ByteBuffer value;
};

struct PN7160::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hw.PN7160");

   // NCI data
   const rt::ByteBuffer NCI_DATA_CMD {0x00, 0x00};

   // NCI core commands
   const rt::ByteBuffer NCI_CORE_RESET_CMD {0x20, 0x00};
   const rt::ByteBuffer NCI_CORE_INIT_CMD {0x20, 0x01};
   const rt::ByteBuffer NCI_CORE_SET_CONF_CMD {0x20, 0x02};
   const rt::ByteBuffer NCI_CORE_GET_CONF_CMD {0x20, 0x03};

   // NCI core extension commands
   const rt::ByteBuffer NCI_CORE_SET_POWER_MODE_CMD {0x2F, 0x00};

   // NCI RF
   const rt::ByteBuffer NCI_RF_DISCOVER_MAP_CMD {0x21, 0x00};
   const rt::ByteBuffer NCI_RF_SET_LISTEN_MODE_ROUTING_CMD {0x21, 0x01};
   const rt::ByteBuffer NCI_RF_DISCOVER_CMD {0x21, 0x03};
   const rt::ByteBuffer NCI_RF_DEACTIVATE_CMD {0x21, 0x06};

   // TEST commands
   const rt::ByteBuffer NCI_TEST_ANTENNA_CMD {0x2F, 0x3D};

   // DEFAULT CONFIG
   const std::vector<rt::ByteBuffer> NXP_CONF_CORE {
      {0x00, 0x02, 0xFE, 0x01}, // TOTAL_DURATION
   };

   const std::vector<rt::ByteBuffer> NXP_CONF_CORE_EXT {
      {0xA0, 0x40, 0x01, 0x00}, // TAG_DETECTOR_CFG
      {0xA0, 0x95, 0x01, 0x00}, // NXP_T4T_NFCEE DISABLE (CE host)
      {0xA0, 0x03, 0x01, 0x08}, // CLOCK_SEL_CFG (internal XTAL)
   };

   const std::vector<rt::ByteBuffer> NXP_CONF_TVDD {
      {0xA0, 0x0E, 0x0B, 0x11, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0xD0, 0x0C} // PMU_CFG (external 5V is used to generate the VDDTX)
   };

   const std::vector<rt::ByteBuffer> NXP_CONF_RF {

      // RF settings for OM27160 board
      {0xA0, 0x0D, 0x03, 0x78, 0x0D, 0x02},
      {0xA0, 0x0D, 0x03, 0x78, 0x14, 0x02},
      {0xA0, 0x0D, 0x06, 0x4C, 0x44, 0x65, 0x09, 0x00, 0x00},
      {0xA0, 0x0D, 0x06, 0x4C, 0x2D, 0x05, 0x35, 0x1E, 0x01},
      {0xA0, 0x0D, 0x06, 0x82, 0x4A, 0x55, 0x07, 0x00, 0x07},
      {0xA0, 0x0D, 0x06, 0x44, 0x44, 0x03, 0x04, 0xC4, 0x00},
      {0xA0, 0x0D, 0x06, 0x46, 0x30, 0x50, 0x00, 0x18, 0x00},
      {0xA0, 0x0D, 0x06, 0x48, 0x30, 0x50, 0x00, 0x18, 0x00},
      {0xA0, 0x0D, 0x06, 0x4A, 0x30, 0x50, 0x00, 0x08, 0x00},

      // disable DLMA and set LMA mode 1
      // {0xA0, 0xAF, 0x0C, 0x83, 0xC0, 0x80, 0xA0, 0x00, 0x83, 0xC0, 0x80, 0xA0, 0x00, 0x00, 0x08}, // NXP_DLMA_CTRL enable DLMA
      {0xA0, 0xAF, 0x0C, 0x03, 0xC0, 0x80, 0xA0, 0x00, 0x03, 0xC0, 0x80, 0xA0, 0x00, 0x00, 0x08}, // NXP_DLMA_CTRL disable DLMA
      // {0xA0, 0x3A, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (0º)
      // {0xA0, 0x3A, 0x08, 0x2D, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x2D, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (45º)
      // {0xA0, 0x3A, 0x08, 0x5A, 0x00, 0x5A, 0x00, 0x5A, 0x00, 0x5A, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (90º)
      // {0xA0, 0x3A, 0x08, 0x87, 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (135º)
      {0xA0, 0x3A, 0x08, 0xB4, 0x00, 0xB4, 0x00, 0xB4, 0x00, 0xB4, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (180º)
      // {0xA0, 0x3A, 0x08, 0xE1, 0x00, 0xE1, 0x00, 0xE1, 0x00, 0xE1, 0x00}, // CLOCK_CONFIG_DLL_ALM, initial phase (225º)
      // {0xA0, 0x3A, 0x08, 0x0E, 0x01, 0x0E, 0x01, 0x0E, 0x01, 0x0E, 0x01}, // CLOCK_CONFIG_DLL_ALM, initial phase (270º)
      // {0xA0, 0x3A, 0x08, 0x63, 0x01, 0x63, 0x01, 0x63, 0x01, 0x63, 0x01}, // CLOCK_CONFIG_DLL_ALM, initial phase (365º)

      // RF settings for CE mode
      {0xA0, 0x0D, 0x06, 0x08, 0x37, 0x28, 0x76, 0x00, 0x00}, // CLIF_TX_CONTROL_REG (mode 1=0x28, mode2=0x08, mode3=0x48)
      {0xA0, 0x0D, 0x06, 0x08, 0x42, 0x00, 0x02, 0xF9, 0xFF}, // CLIF_ANA_TX_AMPLITUDE_REG
      {0xA0, 0x0D, 0x06, 0x08, 0x44, 0x04, 0x04, 0xC4, 0x00}, // CLIF_ANA_RX_REG
      {0xA0, 0x0D, 0x06, 0xC2, 0x35, 0x00, 0x3E, 0x00, 0x03}, // CLIF_AGC_INPUT_REG
      {0xA0, 0x0D, 0x03, 0x24, 0x03, 0x7F}, // CLIF_TRANSCEIVE_CONTROL_REG

      // RF settings for Reader mode
      // {0xA0, 0x0D, 0x03, 0x72, 0x16, 0x17},                  // CLIF_ANA_TX_UNDERSHOOT_CONFIG_REG
      // {0xA0, 0x0D, 0x06, 0x72, 0x4A, 0x57, 0x07, 0x00, 0x1B} // CLIF_ANA_TX_SHAPE_CONTROL_REG
   };

   MPSSE mpsse;

   Protocol protocol;
   unsigned char i2cAddress;
   Status status = STATUS_CLOSED;

   std::string device;

   Impl(const Protocol protocol, const unsigned char addr) : protocol(protocol), i2cAddress(addr)
   {
   }

   ~Impl()
   {
      close();
   }

   bool isOpen() const
   {
      return status != STATUS_CLOSED;
   }

   /*
    * open communication context and initialize device
    */
   bool open(const std::string &config)
   {
      close();

      if (!mpsse.open(protocol == SPI ? MPSSE::SPI0 : MPSSE::I2C, protocol == SPI ? MPSSE::CLK_1MHZ : MPSSE::CLK_100KHZ))
      {
         log->error("open failed: {}", {mpsse.errorString()});
         return false;
      }

      LOG_INFO(log, "{} initialized at {}Hz ({})", {mpsse.deviceName(), mpsse.getClock(), (protocol == SPI ? "SPI" : "I2C")});

      // set DWL = 0 to disable DOWNLOAD mode
      mpsse.setGpio(PN7160_FT232H_DWL_PIN, 0);
      usleep(PN7160_T_WL_DWL);

      // trigger VEN low pulse to reset PN7160
      mpsse.setGpio(PN7160_FT232H_VEN_PIN, 1);
      usleep(PN7160_T_WL_VDD);
      mpsse.setGpio(PN7160_FT232H_VEN_PIN, 0);
      usleep(PN7160_T_WL_VEN);
      mpsse.setGpio(PN7160_FT232H_VEN_PIN, 1);
      usleep(PN7160_T_BOOT);

      // execute initialization sequence
      while (true)
      {
         if (!coreReset(true))
         {
            log->error("core reset failed");
            break;
         }

         if (!nciSetConfig(NXP_CONF_CORE))
         {
            log->error("set NXP_CONF_CORE parameters failed");
            return false;
         }

         if (!nciSetConfig(NXP_CONF_CORE_EXT))
         {
            log->error("set NXP_CONF_CORE_EXT parameters failed");
            return false;
         }

         if (!nciSetConfig(NXP_CONF_TVDD))
         {
            log->error("set NXP_CONF_TVDD parameters failed");
            return false;
         }

         if (!nciSetConfig(NXP_CONF_RF))
         {
            log->error("set NXP_CONF_RF parameters failed");
            return false;
         }

         if (!coreReset(false))
         {
            log->error("core reset failed");
            break;
         }

         if (!nciSetPowerMode(POWER_MODE_FULL))
         {
            log->error("set power mode failed");
            break;
         }

         log->info("initialization successfully!");

         device = mpsse.deviceName();
         status = STATUS_OPENED;

         return true;
      }

      // at this point someone has fail...
      mpsse.close();

      return false;
   }

   /*
    * close communication context
    */
   void close()
   {
      mpsse.close();
      device = mpsse.deviceName();
      status = STATUS_CLOSED;
   }

   /*
    * enables discovery in LISTEN mode (CE-MODE)
    */
   bool startDiscoveryModeListen(const std::vector<Parameter> &parameters)
   {
      log->info("start discovery in listen mode");

      /*
       * step 1, send CORE_SET_CONF_CMD
       */
      // set configuration
      if (!setParameters(parameters))
      {
         log->error("set core parameters failed");
         return false;
      }

      /*
       * step 1, send RF_DISCOVER_MAP_CMD
       */
      const std::vector<DiscoveryMap> discoveryMaps({
         {RF_PROTOCOL_ISO_DEP, RF_MODE_LISTEN, RF_INTERFACE_ISO_DEP},
      });

      // set discovery map
      if (!nciRfDiscoveryMap(discoveryMaps))
      {
         log->error("set rf discovery map failed");
         return false;
      }

      /*
       * step 3, send RF_SET_LISTEN_MODE_ROUTING_CMD
       */
      const std::vector<ListenRouting> listenRoutings({
         {RF_ROUTING_PROTO, {0x00, 0x01, RF_PROTOCOL_ISO_DEP}},
         {RF_ROUTING_TECH, {0x00, 0x01, RF_TECHNOLOGY_A}},
      });

      // set listen mode routing
      if (!nciRfListenRouting(listenRoutings))
      {
         log->error("set listen routing failed");
         return false;
      }

      /*
       * step 4, send RF_DISCOVER_CMD to start listen loop
       */
      const std::vector<DiscoveryMode> discoveryModes({
         {RF_DISCOVERY_LISTEN_PASSIVE_NFC_A, 0x01}
      });

      // enable RF discovery listen
      if (!nciRfDiscoveryStart(discoveryModes))
      {
         log->error("set core parameters failed");
         return false;
      }

      // update status
      status = STATUS_LISTENING;

      return true;
   }

   /*
    * enables discovery in POLL mode
    */
   bool startDiscoveryModePoll(const std::vector<Parameter> &parameters)
   {
      LOG_INFO(log, "start discovery in poll mode");

      /*
       * step 1, send CORE_SET_CONFIG_CMD
       */
      // set configuration
      if (!setParameters(parameters))
      {
         log->error("set core parameters failed");
         return false;
      }

      /*
       * step 2, send RF_DISCOVER_MAP_CMD
       */
      const std::vector<DiscoveryMap> discoveryMaps({
         {RF_PROTOCOL_ISO_DEP, RF_MODE_POLL, RF_INTERFACE_ISO_DEP},
      });

      // set discovery map
      if (!nciRfDiscoveryMap(discoveryMaps))
      {
         log->error("set rf discovery map failed");
         return false;
      }

      /*
       * step 3, send RF_DISCOVER_CMD to start listen loop
       */
      const std::vector<DiscoveryMode> discoveryModes({
         {RF_DISCOVERY_POLL_PASSIVE_NFC_A, 0x01}
      });

      // enable RF discovery listen
      if (!nciRfDiscoveryStart(discoveryModes))
      {
         log->error("discovery start failed");
         return false;
      }

      // update status
      status = STATUS_POLLING;

      return true;
   }

   bool stopDiscoveryMode()
   {
      LOG_INFO(log, "stop discovery mode");

      // enable RF discovery listen
      if (!nciRfDiscoveryStop())
      {
         log->error("discovery stop failed");
         return false;
      }

      // update status
      status = STATUS_OPENED;

      return true;
   }

   /*
    * Wait to receive event
    */
   int waitEvent(rt::ByteBuffer &data, const int timeout) const
   {
      LOG_DEBUG(log, "wait for event, timeout: {}ms", {timeout});

      rt::ByteBuffer event(256);

      // read next message
      if (!nciRecv(event, timeout))
         return EVENT_TIMEOUT;

      const int mt = event.get() & 0xEF;
      const int op = event.get() & 0x3F;
      const int len = event.get();

      // get message payload if any...
      rt::ByteBuffer payload = event.getBuffer(len);

      // add payload to data
      data.put(payload).flip();

      // check message type
      switch (mt)
      {
         case NCI_MT_DATA:
            return EVENT_DATA;

         case NCI_MT_EVENT_CORE:
         {
            if (op == NCI_OP_CORE_CONN_CREDITS_NTF)
            {
               LOG_DEBUG(log, "notify CORE_CONN_CREDITS_NTF");

               const int entries = payload.get();

               for (int i = 0; i < entries; i++)
               {
                  LOG_DEBUG(log, "   connId:0x{02x}", {payload.get()});
                  LOG_DEBUG(log, "   credits:0x{02x}", {payload.get()});
               }

               return EVENT_CREDITS;
            }

            return EVENT_UNKNOW;
         }

         case NCI_MT_EVENT_RF:
         {
            if (op == NCI_OP_RF_INTF_ACTIVATED_NTF)
            {
               LOG_DEBUG(log, "notify RF_INTF_ACTIVATED_NTF");
               LOG_DEBUG(log, "   RF discovery ID:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF interface:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF protocol:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF activation mode:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF max payload size:{}", {payload.get()});
               LOG_DEBUG(log, "   RF initial credits:{}", {payload.get()});

               unsigned int l = payload.get();

               if (l > 0)
                  LOG_DEBUG(log, "   RF tech params:{x}", {payload.getBuffer(l)});

               LOG_DEBUG(log, "   RF exchange mode:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF transmit bit rate:0x{02x}", {payload.get()});
               LOG_DEBUG(log, "   RF receive bit rate:0x{02x}", {payload.get()});

               l = payload.get();

               if (l > 0)
                  LOG_DEBUG(log, "   RF activation params:{x}", {payload.getBuffer(l)});

               return EVENT_ACTIVATED;
            }

            if (op == NCI_OP_RF_DEACTIVATE_NTF)
            {
               LOG_DEBUG(log, "notify RF_DEACTIVATE_NTF, type {} reason {}", {payload.get(), payload.get()});
               return EVENT_DEACTIVATED;
            }

            if (op == NCI_OP_RF_FIELD_INFO_NTF)
            {
               LOG_DEBUG(log, "notify RF_FIELD_INFO_NTF, RF {}", {payload.get() ? "ON" : "OFF"});
               return EVENT_FIELD_INFO;
            }

            return EVENT_UNKNOW;
         }

         default:
            return EVENT_UNKNOW;
      }
   }

   /*
    * Wait to receive data message, skip other events
    */
   bool recvData(rt::ByteBuffer &data, const int timeout) const
   {
      LOG_DEBUG(log, "recv data, timeout: {}ms", {timeout});

      int remaining = timeout;

      // take start time
      const auto startTime = std::chrono::steady_clock::now();

      // while until data has received, filtering out all other messages
      while (const int event = waitEvent(data, remaining))
      {
         if (event == EVENT_DATA)
            return true;

         // clear buffer for next fetch
         data.clear();

         // check if has infinite timeout
         if (timeout < 0)
            continue;

         // check if has remaining time
         if (!remaining)
            return false;

         // finally update timeout and exit if expired...
         if (remaining = timeout - static_cast<int>((std::chrono::steady_clock::now() - startTime).count()); remaining <= 0)
            return false;
      }

      return false;
   }

   /*
    * Send data
    */
   bool sendData(const rt::ByteBuffer &data) const
   {
      LOG_DEBUG(log, "send data: {x}", {data});

      rt::ByteBuffer cmd(256);

      // build set config command
      cmd.put(NCI_DATA_CMD).put(data.elements()).put(data).flip();

      // send control command
      if (!nciSend(cmd))
      {
         log->error("nci data send error");
         return false;
      }

      return true;
   }

   /*
    * perform reset procedure of PN7160
    */
   bool coreReset(const bool resetConfig) const
   {
      LOG_INFO(log, "perform core reset, keep config:{}", {resetConfig ? "NO" : "YES"});

      // reset to apply configuration
      if (!nciCoreReset(resetConfig))
      {
         log->error("core reset failed");
         return false;
      }

      // init to apply configuration
      if (!nciCoreInit())
      {
         log->error("core init failed");
         return false;
      }

      return true;
   }

   /*
    * get parameters values
    */
   bool getParameters(std::vector<Parameter> &parameters) const
   {
      LOG_INFO(log, "get core parameters");

      if (parameters.empty())
      {
         LOG_DEBUG(log, "not parameters to get!");
         return true;
      }

      // build parameters payload
      rt::ByteBuffer payload(256);

      // number of parameters
      payload.put(parameters.size());

      for (const auto &[tag, value]: parameters)
      {
         // add TAG id, 2 byte extended
         if (tag & 0xA000)
            payload.putInt(tag, 2, rt::ByteBuffer::BigEndian);

            // add TAG id, 1 byte
         else
            payload.putInt(tag, 1);
      }

      payload.flip();

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_CORE_GET_CONF_CMD).put(payload.elements()).put(payload).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      rsp.skip(4);

      // parse response and extract parameter values
      const unsigned int num = rsp.getInt(1); // number of parameters received

      for (int i = 0; i < num; i++)
      {
         // get tag id
         unsigned int tag = rsp.get();

         // get tag id (extended)
         if ((tag & 0xA0) == 0xA0)
            tag = (tag << 8) | rsp.get();

         // get tag length
         const unsigned int length = rsp.getInt(1);

         // get value
         rt::ByteBuffer value = rsp.getBuffer(length);

         // find tag in input / output vector
         auto it = std::find_if(parameters.begin(), parameters.end(), [&](const Parameter &item) {
            return item.tag == tag;
         });

         // if tag is foud set value, otherwise add to output
         if (it != parameters.end())
            it->value = value;
         else
            parameters.push_back({tag, value});

         LOG_DEBUG(log, "   [{02x}]: {x}", {tag, value});
      }

      return true;
   }

   /*
    * set parameters values
    */
   bool setParameters(const std::vector<Parameter> &parameters) const
   {
      LOG_INFO(log, "send core parameters");

      if (parameters.empty())
      {
         LOG_DEBUG(log, "not parameters to set!");
         return true;
      }

      // build parameters payload
      std::vector<rt::ByteBuffer> list;

      for (const auto &[tag, value]: parameters)
      {
         LOG_DEBUG(log, "   [{02x}]: {x}", {tag, value});

         if (value.isEmpty())
         {
            log->error("empty parameter [{02x}]", {tag});
            return false;
         }

         rt::ByteBuffer entry(255);

         // add TAG id, 2 byte extended / 1 byte
         if (tag & 0xA000)
            entry.putInt(tag, 2, rt::ByteBuffer::BigEndian);
         else
            entry.putInt(tag, 1);

         // add tag length and value
         entry.putInt(value.elements(), 1).put(value).flip();

         // add to parameter list
         list.push_back(entry);
      }

      return nciSetConfig(list);
   }

   // bool selfTest()
   // {
   //    rt::ByteBuffer cmd(256);
   //    rt::ByteBuffer rsp(256);
   //
   //    // // build set config command
   //    // cmd.put(TEST_ANTENNA_CMD).put(0x02).put({0x01, 0x80}).flip();
   //    //
   //    // // send control command
   //    // if (!nciControl(cmd, rsp))
   //    //    return false;
   //
   //    // build set config command
   //    cmd.clear().put(TEST_ANTENNA_CMD).put(0x02).put({0x20, 0x01}).flip();
   //
   //    // send control command
   //    if (!nciControl(cmd, rsp))
   //       return false;
   //
   //    // build set config command
   //    cmd.clear().put(TEST_ANTENNA_CMD).put(0x02).put({0x20, 0x00}).flip();
   //
   //    // send control command
   //    if (!nciControl(cmd, rsp))
   //       return false;
   //
   //    return true;
   // }

   /*
    * NCI_CORE_RESET_CMD command
    */
   bool nciCoreReset(bool resetConfig) const
   {
      LOG_DEBUG(log, "send NCI_CORE_RESET_CMD, resetConfig: {}", {resetConfig});

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // prepare NCI_CORE_RESET_CMD
      cmd.put(NCI_CORE_RESET_CMD).put(0x01).put(resetConfig ? 1 : 0).flip();

      if (!nciControl(cmd, rsp))
      {
         log->error("send NCI_CORE_RESET_CMD failed");
         return false;
      }

      rsp.clear();

      // read NCI_CORE_RESET_NFT
      if (!nciRecv(rsp, 1000))
      {
         log->error("read NCI_CORE_RESET_NTF failed");
         return false;
      }

      // skip header
      rsp.skip(4);

      const unsigned int confMode = rsp.getInt(1);
      const unsigned int nciVersion = rsp.getInt(1);
      const unsigned int manufacturerId = rsp.getInt(1);

      LOG_DEBUG(log, "   configuration: {}", {confMode ? "reset" : "keep"});
      LOG_DEBUG(log, "   NCI version: {}", {nciVersion == 0x20 ? "2.0" : "1.0"});
      LOG_DEBUG(log, "   manufacturer code: 0x{02x}", {manufacturerId});

      // read Manufacturer Specific Information
      if (rsp.getInt(1) == 4)
      {
         unsigned int hardwareVersion = rsp.getInt(1);
         unsigned int romCodeVersion = rsp.getInt(1);
         unsigned int flashMajorVersion = rsp.getInt(1);
         unsigned int flashMinorVersion = rsp.getInt(1);

         LOG_DEBUG(log, "   ROM code version: {}", {romCodeVersion});
         LOG_DEBUG(log, "   hardware version: {}", {hardwareVersion});
         LOG_DEBUG(log, "   firmware version: {}.{}", {flashMajorVersion, flashMinorVersion});
      }

      return true;
   }

   /*
    * NCI_CORE_INIT_CMD command
    */
   bool nciCoreInit() const
   {
      LOG_DEBUG(log, "send NCI_CORE_INIT_CMD");

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      cmd.put(NCI_CORE_INIT_CMD).put(0x02).put({0x00, 0x00}).flip();

      if (!nciControl(cmd, rsp))
      {
         log->error("send NCI_CORE_INIT_CMD failed");
         return false;
      }

      // skip header
      rsp.skip(4);

      LOG_DEBUG(log, "   NFCC features: {x}", {rsp.getBuffer(4)});
      LOG_DEBUG(log, "   max logical connections: {}", {rsp.getInt(1)});
      LOG_DEBUG(log, "   max routing table size: {x}", {rsp.getBuffer(2)});
      LOG_DEBUG(log, "   max control payload size : {}", {rsp.getInt(1)});
      LOG_DEBUG(log, "   max data payload size : {}", {rsp.getInt(1)});
      LOG_DEBUG(log, "   number of credits (static HCI): {}", {rsp.getInt(1)});
      LOG_DEBUG(log, "   max NFC-V RF frame size: {}", {rsp.getInt(2)});

      if (unsigned int supInf = rsp.getInt(1); supInf != 0)
      {
         LOG_DEBUG(log, "   number of supported RF interfaces: {}", {supInf});

         for (int i = 0; i < supInf; i++)
         {
            LOG_DEBUG(log, "      interface: 0x{04x}", {rsp.getInt(2, rt::ByteBuffer::BigEndian)});
         }
      }

      return true;
   }

   /*
    * NCI_CORE_SET_CONF_CMD command
    */
   bool nciSetConfig(const std::vector<rt::ByteBuffer> &parameters) const
   {
      LOG_DEBUG(log, "send NCI_CORE_SET_CONF_CMD");

      if (parameters.empty())
      {
         LOG_DEBUG(log, "not parameters to set!");
         return true;
      }

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);
      rt::ByteBuffer data(256);

      data.put(parameters.size());

      for (const auto &parameter: parameters)
         data.put(parameter);

      data.flip();

      // build set config command
      cmd.put(NCI_CORE_SET_CONF_CMD).put(data.elements()).put(data).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI_CORE_SET_POWER_MODE_CMD command
    */
   bool nciSetPowerMode(unsigned int mode) const
   {
      LOG_DEBUG(log, "send NCI_CORE_SET_POWER_MODE_CMD");

      // build parameters payload
      rt::ByteBuffer payload({static_cast<unsigned char>(mode)});
      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_CORE_SET_POWER_MODE_CMD).put(payload.elements()).put(payload).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI_RF_DISCOVER_CMD command
    */
   bool nciRfDiscoveryStart(const std::vector<DiscoveryMode> &discoveryModes)
   {
      LOG_DEBUG(log, "send NCI_RF_DISCOVER_CMD");

      // build parameters payload
      rt::ByteBuffer payload(256);

      // number of discovery configurations
      payload.put(discoveryModes.size());

      // add discovery tech modes
      for (const auto &[m, p]: discoveryModes)
      {
         LOG_DEBUG(log, "   mode:0x{02x}, period:0x{02x}", {m, p});
         payload.put(m).put(p);
      }

      payload.flip();

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_RF_DISCOVER_CMD).put(payload.elements()).put(payload).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI_RF_DEACTIVATE_CMD command
    */
   bool nciRfDiscoveryStop()
   {
      LOG_DEBUG(log, "send NCI_RF_DEACTIVATE_CMD");

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_RF_DEACTIVATE_CMD).put(0x01).put(0x00).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI_RF_DISCOVER_MAP_CMD command
    */
   bool nciRfDiscoveryMap(const std::vector<DiscoveryMap> &discoveryMaps) const
   {
      LOG_DEBUG(log, "send NCI_RF_DISCOVER_MAP_CMD");

      // build parameters payload
      rt::ByteBuffer payload(256);

      // number of maps
      payload.put(discoveryMaps.size());

      for (const auto &[protocol, mode, interface]: discoveryMaps)
      {
         LOG_DEBUG(log, "   map proto:0x{02x}, mode:0x{02x}, interface:0x{02x}", {protocol, mode, interface});
         payload.put(protocol).put(mode).put(interface);
      }

      payload.flip();

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_RF_DISCOVER_MAP_CMD).put(payload.elements()).put(payload).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI_RF_SET_LISTEN_MODE_ROUTING_CMD command
    */
   bool nciRfListenRouting(const std::vector<ListenRouting> &listenRoutings) const
   {
      LOG_DEBUG(log, "send NCI_RF_SET_LISTEN_MODE_ROUTING_CMD");

      // build parameters payload
      rt::ByteBuffer payload(256);

      // last message
      payload.put(0x00);

      // number of routes
      payload.put(listenRoutings.size());

      for (const auto &[type, value]: listenRoutings)
      {
         LOG_DEBUG(log, "   routing type:0x{02x}, value:{x}", {type, value});
         payload.put(type).put(value.remaining()).put(value);
      }

      payload.flip();

      rt::ByteBuffer cmd(256);
      rt::ByteBuffer rsp(256);

      // build set config command
      cmd.put(NCI_RF_SET_LISTEN_MODE_ROUTING_CMD).put(payload.elements()).put(payload).flip();

      // send control command
      if (!nciControl(cmd, rsp))
         return false;

      return true;
   }

   /*
    * NCI send control command
    */
   bool nciControl(const rt::ByteBuffer &cmd, rt::ByteBuffer &rsp) const
   {
      // send control command
      if (!nciSend(cmd))
      {
         log->error("nci control send error");
         return false;
      }

      // read control response
      if (!nciRecv(rsp, PN7160_DEFAULT_TIMEOUT))
      {
         log->error("nci control recv error");
         return false;
      }

      // check response status
      if (rsp[3] != NCI_STATUS_OK)
      {
         log->error("nci control recv status:0x{02}", {rsp[3]});
         return false;
      }

      return true;
   }

   /*
    * NCI generic send command
    */
   bool nciSend(const rt::ByteBuffer &cmd) const
   {
      const unsigned char target = protocol == I2C ? static_cast<char>(i2cAddress << 1) : 0x00;

      rt::ByteBuffer buffer(1 + cmd.elements());

      // prepare TX buffer, first byte I2C address or direction byte for SPI
      buffer.put(target).put(cmd).flip();

      LOG_TRACE(log, "TX: {x}", {cmd});

      // set START condition
      if (!mpsse.start())
      {
         log->error("nciSend start failed: {}", {mpsse.errorString()});
         return false;
      }

      {
         // set STOP condition
         rt::Finally stop([&] { mpsse.stop(); });

         // send data
         if (!mpsse.write(buffer))
         {
            log->error("nciSend data failed: {}", {mpsse.errorString()});
            return false;
         }
      }

      return true;
   }

   /*
    * NCI generic recv command
    */
   bool nciRecv(rt::ByteBuffer &res, const int timeout) const
   {
      // for I2C mode send address, or 0xFF in SPI
      const unsigned char request = static_cast<char>(protocol == I2C ? (i2cAddress << 1) | 1 : 0xff);

      // check if response buffer has enough capacity
      if (res.capacity() < 3)
      {
         log->error("read data failed: buffer capacity {} is less than minimum 3", {res.capacity()});
         return false;
      }

      // take start time
      const auto startTime = std::chrono::steady_clock::now();

      do
      {
         if (hasMessage())
         {
            // set START condition
            if (!mpsse.start())
            {
               log->error("nciRecv start failed: {}", {mpsse.errorString()});
               return false;
            }

            {
               rt::ByteBuffer hdr(3);

               // set STOP condition
               rt::Finally stop([&] { mpsse.stop(); });

               // send data request
               if (!mpsse.write({request}))
               {
                  log->error("nciRecv target failed: {}", {mpsse.errorString()});
                  return false;
               }

               // read NCI header
               if (!mpsse.read(hdr))
               {
                  log->error("nciRecv read failed: {}", {mpsse.errorString()});
                  return false;
               }

               // get response length
               unsigned int length = hdr[2];

               // check if response buffer has enough capacity
               if (res.capacity() < length)
               {
                  log->error("nciRecv failed: buffer capacity {} is less than required {}", {res.capacity(), length});
                  return false;
               }

               // data buffer to read
               rt::ByteBuffer data(length);

               // read data from bus
               if (!mpsse.read(data))
               {
                  log->error("nciRecv read failed: {}", {mpsse.errorString()});
                  return false;
               }

               // add data and commit output buffer
               res.put(hdr).put(data).flip();
            }

            LOG_TRACE(log, "RX: {x}", {res});

            return true;
         }
      }
      while (timeout < 0 || (std::chrono::steady_clock::now() - startTime) < std::chrono::milliseconds(timeout));

      LOG_TRACE(log, "RX: timeout!");

      return false;
   }

   /*
    * IRQ line poll to check if PN7160 has message
    */
   bool hasMessage() const
   {
      return mpsse.getGpio(PN7160_FT232H_IRQ_PIN);
   }
};

PN7160::PN7160(Protocol protocol, unsigned char addr) : impl(std::make_shared<Impl>(protocol, addr))
{
}

PN7160::operator bool() const
{
   return impl->isOpen();
}

PN7160::Status PN7160::status()
{
   return impl->status;
}

int PN7160::open(const std::string &config)
{
   return impl->open(config);
}

void PN7160::close()
{
   return impl->close();
}

bool PN7160::coreReset(bool resetConfig) const
{
   return impl->coreReset(resetConfig);
}

bool PN7160::startDiscovery(const std::vector<Parameter> &parameters, const int mode) const
{
   if (mode == DISCOVERY_LISTEN)
      return impl->startDiscoveryModeListen(parameters);

   if (mode == DISCOVERY_POLL)
      return impl->startDiscoveryModePoll(parameters);

   return false;
}

bool PN7160::stopDiscovery() const
{
   return impl->nciRfDiscoveryStop();
}

int PN7160::waitEvent(rt::ByteBuffer &data, const int timeout) const
{
   return impl->waitEvent(data, timeout);
}

bool PN7160::sendData(const rt::ByteBuffer &data) const
{
   return impl->sendData(data);
}

bool PN7160::recvData(rt::ByteBuffer &data, const int timeout) const
{
   return impl->recvData(data, timeout);
}

}
