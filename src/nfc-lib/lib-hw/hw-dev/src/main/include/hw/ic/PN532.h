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

#ifndef DEV_PN532_H
#define DEV_PN532_H

#include <functional>

#include <rt/ByteBuffer.h>

namespace hw {

class PN532
{
   struct Impl;

   public:

      typedef std::function<int(const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout)> TransmitFunction;

      enum Register
      {
         CIU_Mode = 0x6301,
         CIU_TxMode = 0x6302,
         CIU_RxMode = 0x6303,
         CIU_TxControl = 0x6304,
         CIU_TxAuto = 0x6305,
         CIU_TxSel = 0x6306,
         CIU_RxSel = 0x6307,
         CIU_RxThreshold = 0x6308,
         CIU_Demod = 0x6309,
         CIU_FelNFC1 = 0x630A,
         CIU_FelNFC2 = 0x630B,
         CIU_MifNFC = 0x630C,
         CIU_ManualRCV = 0x630D,
         CIU_TypeB = 0x630E,
         CIU_CRCResultMSB = 0x6311,
         CIU_CRCResultLSB = 0x6312,
         CIU_GsNOff = 0x6313,
         CIU_ModWidth = 0x6314,
         CIU_TxBitPhase = 0x6315,
         CIU_RFCfg = 0x6316,
         CIU_GsNOn = 0x6317,
         CIU_CWGsP = 0x6318,
         CIU_ModGsP = 0x6319,
         CIU_TMode = 0x631A,
         CIU_TPrescaler = 0x631B,
         CIU_TReloadVal_hi = 0x631C,
         CIU_TReloadVal_lo = 0x631D,
         CIU_TCounterVal_hi = 0x631E,
         CIU_TCounterVal_lo = 0x631F,
         CIU_TestSel1 = 0x6321,
         CIU_TestSel2 = 0x6322,
         CIU_TestPinEn = 0x6323,
         CIU_TestPinValue = 0x6324,
         CIU_TestBus = 0x6325,
         CIU_AutoTest = 0x6326,
         CIU_Version = 0x6327,
         CIU_AnalogTest = 0x6328,
         CIU_TestDAC1 = 0x6329,
         CIU_TestDAC2 = 0x632A,
         CIU_TestADC = 0x632B,
         CIU_RFlevelDet = 0x632F,
         SIC_CLK = 0x6330,
         CIU_Command = 0x6331,
         CIU_CommIEn = 0x6332,
         CIU_DivIEn = 0x6333,
         CIU_CommIrq = 0x6334,
         CIU_DivIrq = 0x6335,
         CIU_Error = 0x6336,
         CIU_Status1 = 0x6337,
         CIU_Status2 = 0x6338,
         CIU_FIFOData = 0x6339,
         CIU_FIFOLevel = 0x633A,
         CIU_WaterLevel = 0x633B,
         CIU_Control = 0x633C,
         CIU_BitFraming = 0x633D,
         CIU_Coll = 0x633E
      };

      struct FwVersion
      {
         int ic;
         int ver;
         int rev;
         int support;
      };

      struct GeneralStatus
      {
         int err;
         int field;
         int nbTg;
         int tg1Id;
         int tg1BrRx;
         int tg1BrTx;
         int tg1Type;
         int tg2Id;
         int tg2BrRx;
         int tg2BrTx;
         int tg2Type;
         int sam;
      };

   public:

      PN532(TransmitFunction transmitFunction);

      // Miscellaneous commands

      int diagnose();

      int getFirmwareVersion(FwVersion &version);

      int getGeneralStatus(GeneralStatus &status);

      int readRegister(int reg, int &value);

      int writeRegister(int reg, int value);

      int readGPIO();

      int writeGPIO();

      int setSerialBaudRate();

      int setParameters(int value);

      int setSAMConfiguration(int mode, int timeout = 0, int irq = 1);

      int powerDown(int wakeUpEnable, int triggerIrq = 0);

      // tag emulation commands
      int tgInitAsTarget(int &mode, rt::ByteBuffer &init);

      int tgResponseToInitiator(rt::ByteBuffer &data, int &status);

      int tgGetData(rt::ByteBuffer &data, int &status);

      int tgSetData(const rt::ByteBuffer &data, int &status);

   private:

      std::shared_ptr<Impl> impl;
};

}
#endif //DEV_PN532_H
