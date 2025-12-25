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

#include <cmath>
#include <chrono>

#include <hw/ic/PN7160.h>

#include <hce/Frame.h>

#include <hce/targets/T4T.h>

#include <hce/tasks/TargetListenerTask.h>

#include "AbstractTask.h"

namespace hce::tasks {

struct TargetListenerTask::Impl : TargetListenerTask, AbstractTask
{
   int listenerStatus = 0;

   hw::PN7160 pn7160;

   std::shared_ptr<Target> target;

   std::vector<hw::PN7160::Parameter> parameters;

   rt::Subject<Frame> *listenerFrameStream = nullptr;

   explicit Impl() : AbstractTask("worker.TargetListener", "target.listener"), pn7160(hw::PN7160::SPI)
   {
      // create frame stream subject
      listenerFrameStream = rt::Subject<Frame>::name("target.listener.frame");
   }

   void start() override
   {
      log->info("starting listener task");

      updateListenerStatus(Absent);
   }

   void stop() override
   {
      log->info("stopping listener task");
   }

   bool loop() override
   {
      /*
      * process pending commands
      */
      if (auto command = commandQueue.get())
      {
         log->debug("command [{}]", {command->code});

         switch (command->code)
         {
            case Start:
               startListening(command.value());
               break;

            case Stop:
               stopListening(command.value());
               break;

            case Configure:
               configureTarget(command.value());
               break;

            default:
               log->warn("unknown command {}", {command->code});
               command->reject(UnknownCommand);
               return true;
         }
      }

      if (pn7160)
      {
         process();
      }
      else
      {
         refresh();
      }

      return true;
   }

   void refresh()
   {
      // try to open...
      if (pn7160.open())
      {
         log->info("device PN7160 open success!");

         // setup(currentConfig);

         updateListenerStatus(Idle);
      }
      else
      {
         updateListenerStatus(Absent);

         wait(1000);
      }
   }

   void startListening(const rt::Event &command)
   {
      log->info("starting discovery");

      target = std::make_shared<targets::T4T>();

      const auto atqa = target->get<unsigned short>(Target::PARAM_ATQA);
      const auto sak = target->get<unsigned char>(Target::PARAM_SAK);
      const auto tb1 = target->get<unsigned char>(Target::PARAM_RATS_TB1);
      const auto tc1 = target->get<unsigned char>(Target::PARAM_RATS_TC1);
      const auto uid = target->get<rt::Buffer<unsigned char>>(Target::PARAM_UID);
      const auto hist = target->get<rt::Buffer<unsigned char>>(Target::PARAM_RATS_HB);

      const rt::ByteBuffer sn(uid.ptr(), uid.size());
      const rt::ByteBuffer hb(hist.ptr(), hist.size());

      parameters = {

         // Listen Mode – NFC-A Discovery Parameters
         {hw::PN7160::PARAM_LA_BIT_FRAME_SDD, {static_cast<unsigned char>(atqa >> 8)}}, // first byte of ATQA
         {hw::PN7160::PARAM_LA_PLATFORM_CONFIG, {static_cast<unsigned char>(atqa & 0xff)}}, // second byte of ATQA
         {hw::PN7160::PARAM_LA_SEL_INFO, {sak}}, // SAK
         {hw::PN7160::PARAM_LA_NFCID1, sn}, // UID

         // Listen Mode – ISO-DEP Discovery Parameters
         {hw::PN7160::PARAM_LI_A_BIT_RATE, {0x00}}, // only 106Kbps
         {hw::PN7160::PARAM_LI_A_RATS_TB1, {tb1}}, // FWT & SFGT
         {hw::PN7160::PARAM_LI_A_RATS_TC1, {tc1}}, //
         {hw::PN7160::PARAM_LI_A_HIST_BY, hb}, // historical bytes

         // Other Parameters
         {hw::PN7160::PARAM_RF_FIELD_INFO, {0x00}}, // notify when external field is detected
         {hw::PN7160::PARAM_RF_NFCEE_ACTION, {0x01}}, // notify activation / deactivation
      };

      // starting discovery
      if (!pn7160.startDiscovery(parameters, hw::PN7160::DISCOVERY_LISTEN))
      {
         log->warn("start discovery failed");
         return;
      }

      updateListenerStatus(Listening);
   }

   void stopListening(const rt::Event &command)
   {
      log->info("stop discovery");

      if (pn7160)
      {
         pn7160.stopDiscovery();
      }

      updateListenerStatus(Idle);
   }

   void configureTarget(const rt::Event &command)
   {
      if (auto data = command.get<std::string>("data"))
      {
         auto config = json::parse(data.value());

         log->debug("change config: {}", {config.dump()});

         command.resolve();

         updateListenerStatus(0);
      }
      else
      {
         log->warn("invalid config data");

         command.reject(InvalidConfig);
      }
   }

   void process()
   {
      if (listenerStatus != Listening)
         return;

      rt::ByteBuffer request(256);
      rt::ByteBuffer response(256);

      while (const int event = pn7160.waitEvent(request, 500))
      {
         Frame requestFrame(NfcATech, NfcRequestFrame, request, timeMs());

         // process received event
         if (event == hw::PN7160::EVENT_DATA)
         {
            // prepare response frame
            Frame responseFrame;

            // clear previous response
            response.clear();

            if (target)
            {
               // process data from reader
               if (target->process(request, response) == 0)
               {
                  responseFrame = Frame(NfcATech, NfcResponseFrame, response, timeMs() + 1);

                  // send response to reader
                  if (!pn7160.sendData(response))
                  {
                     log->warn("failed to send response to reader");
                  }
               }
               else
               {
                  log->warn("target failed to process command");
               }
            }

            if (requestFrame)
               listenerFrameStream->next(requestFrame);

            if (responseFrame)
               listenerFrameStream->next(responseFrame);
         }
         else if (event == hw::PN7160::EVENT_ACTIVATED)
         {
            if (target)
               target->select();

            Frame activateFrame(NfcATech, NfcActivateFrame, request, timeMs());
            listenerFrameStream->next(activateFrame);
         }
         else if (event == hw::PN7160::EVENT_DEACTIVATED)
         {
            if (target)
               target->deselect();

            Frame deactivateFrame(NfcATech, NfcDeactivateFrame, request, timeMs());
            listenerFrameStream->next(deactivateFrame);
         }

         // reset buffer for next read
         request.clear();
      }
   }

   void updateListenerStatus(const int status)
   {
      json data;

      listenerStatus = status;

      if (status == Idle)
         data["status"] = "idle";
      else if (status == Listening)
         data["status"] = "listening";
      else
         data["status"] = "disabled";

      updateStatus(status, data);
   }

   static unsigned long long timeMs()
   {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
   }
};

TargetListenerTask::TargetListenerTask() : Worker("TargetListener")
{
}

rt::Worker *TargetListenerTask::construct()
{
   return new Impl;
}

}
