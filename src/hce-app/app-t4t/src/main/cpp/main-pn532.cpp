/*

  This file is part of NFC-EMULATION.

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
  along with NFC-EMULATION. If not, see <http://www.gnu.org/licenses/>.

*/

// https://stackoverflow.com/questions/5661808/acr122-card-emulation

#ifdef __WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include <cmath>
#include <iostream>
#include <utility>

#include <unistd.h>

#include <rt/Logger.h>

#include <hw/HSU.h>
#include <hw/PN532.h>
#include <hw/ACR122U.h>

#include <hce/desfire/Desfire.h>

struct Main
{
   rt::Logger *log = rt::Logger::getLogger("app.main");

   std::vector<std::string> args;

   explicit Main(std::vector<std::string> args) : args(std::move(args))
   {
   }

   void init() const
   {
      log->info("NFC emulator, 2025 Jose Vicente Campos Martinez");
   }

   void finish() const
   {
      log->info("NFC emulator finished");
   }

   int run() const
   {
      init();

      if (args.size() < 2)
      {
         showUsage();
         return -1;
      }

      // capture port
      std::string device = "";
      std::string port = "COM1";
      bool verbose = false;

      hw::HSU hsu;
      hw::ACR122U acr;
      hw::PN532 *pn532;

      for (size_t i = 1; i < args.size(); i++)
      {
         if (args[i] == "-h" || args[i] == "--help")
         {
            showUsage();
            return 0;
         }

         else if ((args[i] == "-d" || args[i] == "--device") && i + 1 < args.size())
            device = args[++i];
         else if ((args[i] == "-p" || args[i] == "--port") && i + 1 < args.size())
            port = args[++i];
         else if (args[i] == "-v" || args[i] == "--verbose")
            verbose = true;
      }

      if (device == "HSU")
      {
         if (hsu.open(port, "baud=115200 data=8 parity=N stop=1") != 0)
         {
            log->error("cannot open HSU port {}!", {port});
            return -1;
         }

         pn532 = new hw::PN532([&hsu](const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout) { return hsu.transmit(cmd, res, timeout); });
      }
      else if (device == "ACR")
      {
         if (acr.open(hw::PCSC::Direct) != 0)
         {
            log->error("cannot open ACR reader!");
            return -1;
         }

         // disable ACR122 polling
         if (acr.setParameters(0x00) != 0)
         {
            log->error("cannot set ACR parameters!");
            return -1;
         }

         pn532 = new hw::PN532([&acr](const rt::ByteBuffer &cmd, rt::ByteBuffer &res, int timeout) { return acr.transmit(cmd, res, timeout); });
      }
      else
      {
         log->error("missing device type: --arc|--hsu");
         return -1;
      }

      hw::PN532::FwVersion fwVersion {};

      if (pn532->getFirmwareVersion(fwVersion) != 0)
      {
         log->error("cannot get PN532 firmware version");
         return -1;
      }

      log->info("PN532 firmware version: IC=0x{02x} VER={}.{} SUPPORT=0x{02x}", {fwVersion.ic, fwVersion.ver, fwVersion.rev, fwVersion.support});

      hw::PN532::GeneralStatus generalStatus {};

      if (pn532->getGeneralStatus(generalStatus) != 0)
      {
         log->error("cannot get PN532 general status");
         return -1;
      }

      log->info("PN532 general status");
      log->info("  error: 0x{02x}", {generalStatus.err});
      log->info("  field: {}", {generalStatus.field});
      log->info("  sam: 0x{02x}", {generalStatus.sam});
      log->info("  nbTg: {}", {generalStatus.nbTg});

      if (generalStatus.nbTg > 0)
      {
         log->info("  tg1Id: 0x{02x}", {generalStatus.tg1Id});
         log->info("  tg1BrRx: 0x{02x}", {generalStatus.tg1BrRx});
         log->info("  tg1BrTx: 0x{02x}", {generalStatus.tg1BrTx});
         log->info("  tg1Type: 0x{02x}", {generalStatus.tg1Type});
      }

      if (generalStatus.nbTg > 1)
      {
         log->info("  tg2Id: 0x{02x}", {generalStatus.tg2Id});
         log->info("  tg2BrRx: 0x{02x}", {generalStatus.tg2BrRx});
         log->info("  tg2BrTx: 0x{02x}", {generalStatus.tg2BrTx});
         log->info("  tg2Type: 0x{02x}", {generalStatus.tg2Type});
      }

      // set
      if (pn532->setSAMConfiguration(0x01) != 0)
      {
         log->error("cannot set PN532 SAM configuration");
         return -1;
      }

      // configure for card emulation
      int ciuTxAuto;
      int ciuManualRCV;
      int ciuStatus2;
      int ciuTxControl;

      // read current configuration
      pn532->readRegister(hw::PN532::CIU_ManualRCV, ciuManualRCV);
      pn532->readRegister(hw::PN532::CIU_Status2, ciuStatus2);
      pn532->readRegister(hw::PN532::CIU_TxAuto, ciuTxAuto);
      pn532->readRegister(hw::PN532::CIU_TxControl, ciuTxControl);

      // configure for card emulation
      pn532->writeRegister(hw::PN532::CIU_TxMode, 0x80); // TxCRCEn=0x80
      pn532->writeRegister(hw::PN532::CIU_RxMode, 0x80); // RxCRCEn=0x80
      pn532->writeRegister(hw::PN532::CIU_TxControl, ciuTxControl & 0xFC); // CIU_TxControl &= ~(Tx1RFEn | Tx2RFEn)
      pn532->writeRegister(hw::PN532::CIU_TxAuto, ciuTxAuto | 0x04); // CIU_TxAuto |= InitialRFOn
      pn532->writeRegister(hw::PN532::CIU_ManualRCV, ciuManualRCV & 0xEF); // CIU_ManualRCV &= ~ParityDisable
      pn532->writeRegister(hw::PN532::CIU_Status2, ciuStatus2 & 0xF7); // CIU_Status2 &= ~MFCrypto1On

      // configure parameters
      if (pn532->setParameters(0x34) != 0) // set fISO14443-4_PICC=0x20 | fAutomaticRATS=0x10 | fAutomaticATR_RES=0x04
      {
         log->error("cannot set PN532 parameters");
         return -1;
      }

      // init as target
      int mode;
      int status;
      rt::ByteBuffer init(256);
      rt::ByteBuffer request(256);
      rt::ByteBuffer response(256);

      unsigned char uid[7] = {0x04, 0x51, 0x25, 0x7A, 0xE5, 0x48, 0x80};

      hce::targets::Desfire card(uid);

      do
      {
         // clear previous card initiator buffer
         init.clear();

         // wait until reader activates card emulation
         if (pn532->tgInitAsTarget(mode, init) == 0)
         {
            log->info("initiator mode 0x{02x}, data {x}", {mode, init});

            card.select();

            request.clear();

            // emulation loop
            while (pn532->tgGetData(request, status) == 0)
            {
               response.clear();

               if (status != 0)
               {
                  log->info("getting data status: {x}", {status});
                  break;
               }

               // process received command
               card.process(request, response);

               // send response to reader
               pn532->tgSetData(response, status);

               if (status != 0)
               {
                  log->info("setting data status: {x}", {status});
                  break;
               }

               request.clear();
            }

            card.deselect();
         }
      }
      while (true);

      finish();

      return 0;
   }

   static void showUsage()
   {
      std::cout << "NFC emulator, 2025 Jose Vicente Campos Martinez" << std::endl;
      std::cout << std::endl;
      std::cout << "Usage: app-hce [options]" << std::endl;
      std::cout << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  -h, --help           Show this help message and exit" << std::endl;
      std::cout << "  -d, --device ACR|HSU Selects ACR or HSU device interface" << std::endl;
      std::cout << "  -p, --port COM1      Sets COM1 as serial port for HSU" << std::endl;
      std::cout << "  -v, --verbose        Enable verbose logging" << std::endl;
      std::cout << std::endl;
   }

} *app;

#ifdef __WIN32

WINBOOL intHandler(DWORD sig)
{
   fprintf(stderr, "Terminate on signal %lu\n", sig);
   app->finish();
   return true;
}

#else
void intHandler(int sig)
{
   fprintf(stderr, "Terminate on signal %d\n", sig);
   app->finish();
}
#endif

int main(int argc, char *argv[])
{
   // send logging events to stderr
   rt::Logger::init(std::cout);

   // disable logging at all (can be enabled with -v option)
   rt::Logger::setRootLevel(rt::Logger::WARN_LEVEL);
   rt::Logger::setLoggerLevel("app.*", rt::Logger::INFO_LEVEL);
   rt::Logger::setLoggerLevel("hw.*", rt::Logger::INFO_LEVEL);
   rt::Logger::setLoggerLevel("hce.crypto.*", rt::Logger::INFO_LEVEL);
   rt::Logger::setLoggerLevel("hce.targets.*", rt::Logger::DEBUG_LEVEL);

   // register signals handlers
#ifdef __WIN32
   SetConsoleCtrlHandler(intHandler, TRUE);
   SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
   signal(SIGINT, intHandler);
   signal(SIGTERM, intHandler);
#endif

   // capture command line arguments
   std::vector<std::string> args(argv, argv + argc);

   // create main object
   Main main(args);

   // set global pointer for signal handlers
   app = &main;

   // and run
   const int res = main.run();

   // flush all pending log messages
   rt::Logger::flush();

   return res;
}
