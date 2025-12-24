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

#ifndef DEV_PN7160_H
#define DEV_PN7160_H

#include <string>

#include <rt/ByteBuffer.h>

namespace hw {

class PN7160
{
   struct Impl;

   public:

      typedef std::function<int(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)> TransmitFunction;

      enum Protocol
      {
         I2C = 0,
         SPI = 1
      };

      enum Status
      {
         STATUS_CLOSED = 0,
         STATUS_OPENED = 1,
         STATUS_LISTENING = 2,
         STATUS_POLLING = 4,
      };

      enum Event
      {
         EVENT_UNKNOW = -1,
         EVENT_TIMEOUT = 0,
         EVENT_FIELD_INFO = 1,
         EVENT_ACTIVATED = 2,
         EVENT_DEACTIVATED = 3,
         EVENT_CREDITS = 4,
         EVENT_DATA = 5,
      };

      enum Discovery
      {
         DISCOVERY_LISTEN = 0,
         DISCOVERY_POLL = 1,
      };

      enum ParamId
      {
         // Common Discovery Parameters
         PARAM_TOTAL_DURATION = 0x00,
         PARAM_CON_DISCOVERY_PARAM = 0x02,
         PARAM_POWER_STATE = 0x03,

         // Poll Mode – NFC-A Discovery Parameters
         PARAM_PA_BAIL_OUT = 0x08,
         PARAM_PA_DEVICES_LIMIT = 0x09,

         // Poll Mode – NFC-B Discovery Parameters
         PARAM_PB_AFI = 0x10,
         PARAM_PB_BAIL_OUT = 0x11,
         PARAM_PB_ATTRIB_PARAM1 = 0x12,
         PARAM_PB_SENSB_REQ_PARAM = 0x13,
         PARAM_PB_DEVICES_LIMIT = 0x14,

         // Poll Mode – NFC-F Discovery Parameters
         PARAM_PF_BIT_RATE = 0x18,
         PARAM_PF_BAIL_OUT = 0x19,
         PARAM_PF_DEVICES_LIMIT = 0x1A,

         // Poll Mode – ISO-DEP Discovery Parameters
         PARAM_PI_B_H_INFO = 0x20,
         PARAM_PI_BIT_RATE = 0x21,

         // Poll Mode – NFC-DEP Discovery Parameters
         PARAM_PN_NFC_DEP_PSL = 0x28,
         PARAM_PN_ATR_REQ_GEN_BYTES = 0x29,
         PARAM_PN_ATR_REQ_CONFIG = 0x2A,

         // Poll Mode – NFC-V Discovery Parameters
         PARAM_PV_DEVICES_LIMIT = 0x2F,

         // Listen Mode – NFC-A Discovery Parameters
         PARAM_LA_BIT_FRAME_SDD = 0x30,
         PARAM_LA_PLATFORM_CONFIG = 0x31,
         PARAM_LA_SEL_INFO = 0x32,
         PARAM_LA_NFCID1 = 0x33,

         // Listen Mode – NFC-B Discovery Parameters
         PARAM_LB_SENSB_INFO = 0x38,
         PARAM_LB_NFCID0 = 0x39,
         PARAM_LB_APPLICATION_DATA = 0x3A,
         PARAM_LB_SFGI = 0x3B,
         PARAM_LB_FWI_ADC_FO = 0x3C,
         PARAM_LB_BIT_RATE = 0x3E,

         // Listen Mode – T3T Discovery Parameters
         PARAM_LF_T3T_IDENTIFIERS_1 = 0x40,
         PARAM_LF_T3T_IDENTIFIERS_2 = 0x41,
         PARAM_LF_T3T_IDENTIFIERS_3 = 0x42,
         PARAM_LF_T3T_IDENTIFIERS_4 = 0x43,
         PARAM_LF_T3T_IDENTIFIERS_5 = 0x44,
         PARAM_LF_T3T_IDENTIFIERS_6 = 0x45,
         PARAM_LF_T3T_IDENTIFIERS_7 = 0x46,
         PARAM_LF_T3T_IDENTIFIERS_8 = 0x47,
         PARAM_LF_T3T_IDENTIFIERS_9 = 0x48,
         PARAM_LF_T3T_IDENTIFIERS_10 = 0x49,
         PARAM_LF_T3T_IDENTIFIERS_11 = 0x4A,
         PARAM_LF_T3T_IDENTIFIERS_12 = 0x4B,
         PARAM_LF_T3T_IDENTIFIERS_13 = 0x4C,
         PARAM_LF_T3T_IDENTIFIERS_14 = 0x4D,
         PARAM_LF_T3T_IDENTIFIERS_15 = 0x4E,
         PARAM_LF_T3T_IDENTIFIERS_16 = 0x4E,
         PARAM_LF_T3T_MAX = 0x52,
         PARAM_LF_T3T_FLAGS = 0x53,
         PARAM_LF_T3T_RD_ALLOWED = 0x55,

         // Listen Mode – NFC-F Discovery Parameters
         PARAM_LF_PROTOCOL_TYPE = 0x50,

         // Listen Mode – ISO-DEP Discovery Parameters
         PARAM_LI_A_RATS_TB1 = 0x58,
         PARAM_LI_A_HIST_BY = 0x59,
         PARAM_LI_B_H_INFO_RESP = 0x5A,
         PARAM_LI_A_BIT_RATE = 0x5B,
         PARAM_LI_A_RATS_TC1 = 0x5C,

         // Listen Mode – NFC-DEP Discovery Parameters
         PARAM_LN_WT = 0x60,
         PARAM_LN_ATR_RES_GEN_BYTES = 0x61,
         PARAM_LN_ATR_RES_CONFIG = 0x62,

         // Active Poll Mode Parameters
         PARAM_PACM_BIT_RATE = 0x68,

         // Other Parameters
         PARAM_RF_FIELD_INFO = 0x80,
         PARAM_RF_NFCEE_ACTION = 0x81,
      };

      struct Parameter
      {
         unsigned int tag;
         rt::ByteBuffer value;
      };

   public:

      explicit PN7160(Protocol protocol, unsigned char addr = 0x28);

      operator bool() const;

      Status status();

      int open(const std::string &config = std::string());

      void close();

      bool coreReset(bool resetConfig = true) const;

      bool startDiscovery(const std::vector<Parameter> &parameters, int mode) const;

      bool stopDiscovery() const;

      int waitEvent(rt::ByteBuffer &data, int timeout = -1) const;

      bool recvData(rt::ByteBuffer &data, int timeout = -1) const;

      bool sendData(const rt::ByteBuffer &data) const;

   private:

      std::shared_ptr<Impl> impl;

};

}
#endif //DEV_PN7160_H
