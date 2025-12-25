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

#include <filesystem>

#include <QDebug>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <rt/Event.h>
#include <rt/Subject.h>

#include <events/ListenerControlEvent.h>
#include <events/ListenerFrameEvent.h>
#include <events/ListenerStatusEvent.h>
#include <events/SystemShutdownEvent.h>
#include <events/SystemStartupEvent.h>

#include <hce/Frame.h>

#include <hce/tasks/TargetListenerTask.h>

#include "QtApplication.h"

#include "QtControl.h"

#include "events/ListenerControlEvent.h"

struct QtControl::Impl
{
   // configuration
   QSettings settings;

   // subjects
   rt::Subject<rt::Event> *listenerStatusStream = nullptr;
   rt::Subject<rt::Event> *listenerCommandStream = nullptr;
   rt::Subject<hce::Frame> *listenerFrameStream = nullptr;

   // subscriptions
   rt::Subject<rt::Event>::Subscription listenerStatusSubscription;
   rt::Subject<rt::Event>::Subscription listenerFrameSubscription;

   QString targetListenerStatus;

   explicit Impl()
   {
      // connect streams
      listenerStatusStream = rt::Subject<rt::Event>::name("target.listener.status");
      listenerCommandStream = rt::Subject<rt::Event>::name("target.listener.command");
      listenerFrameStream = rt::Subject<hce::Frame>::name("target.listener.frame");
   }

   /*
    * system event executed after startup
    */
   void systemStartupEvent(SystemStartupEvent *event)
   {
      // subscribe to status events
      listenerStatusSubscription = listenerStatusStream->subscribe([this](const rt::Event &params) {
         targetListenerStatusChange(params);
      });

      listenerFrameSubscription = listenerFrameStream->subscribe([this](const hce::Frame &frame) {
         listenerFrameEvent(frame);
      });
   }

   /*
    * system event executed before shutdown
    */
   void systemShutdownEvent(SystemShutdownEvent *event)
   {
   }

   /*
    * process listener control event
    */
   void listenerControlEvent(ListenerControlEvent *event)
   {
      switch (event->command())
      {
         case ListenerControlEvent::Start:
         {
            doStartListener(event);
            break;
         }
         case ListenerControlEvent::Stop:
         {
            doStopListener(event);
            break;
         }
         case ListenerControlEvent::Config:
         {
            doConfigureListener(event);
            break;
         }
      }
   }

   /*
    * start listener
    */
   void doStartListener(ListenerControlEvent *event)
   {
      qInfo() << "start listener";

      triggerListenerStart();
   }

   /*
    * stop listener
    */
   void doStopListener(ListenerControlEvent *event)
   {
      qInfo() << "stop listener";

      triggerListenerStop();
   }

   /*
   * configure listener
   */
   void doConfigureListener(ListenerControlEvent *event)
   {
      qInfo() << "configure listener";
   }

   /*
    * process target listener status event
    */
   void targetListenerStatusChange(const rt::Event &event)
   {
      if (const auto data = event.get<std::string>("data"))
      {
         QJsonObject status = QJsonDocument::fromJson(QByteArray::fromStdString(data.value())).object();

         // start device for first time
         // if (targetListenerStatus.isEmpty())
         //    triggerListenerStart();

         targetListenerStatus = status["status"].toString();

         QtApplication::post(ListenerStatusEvent::create(status));
      }
   }

   /*
    * process new received frame
   */
   void listenerFrameEvent(const hce::Frame &frame)
   {
      QtApplication::post(new ListenerFrameEvent(frame), Qt::HighEventPriority);
   }

   /*
    * start target listener task
    */
   void triggerListenerStart(const std::function<void()> &onComplete = nullptr, const std::function<void(int, const std::string &)> &onReject = nullptr) const
   {
      listenerCommandStream->next({hce::tasks::TargetListenerTask::Start, onComplete, onReject});
   }

   /*
    * start target listener task
    */
   void triggerListenerStop(const std::function<void()> &onComplete = nullptr, const std::function<void(int, const std::string &)> &onReject = nullptr) const
   {
      listenerCommandStream->next({hce::tasks::TargetListenerTask::Stop, onComplete, onReject});
   }
};

QtControl::QtControl() : impl(new Impl())
{
}

void QtControl::handleEvent(QEvent *event)
{
   if (event->type() == SystemStartupEvent::Type)
      impl->systemStartupEvent(dynamic_cast<SystemStartupEvent *>(event));
   else if (event->type() == SystemShutdownEvent::Type)
      impl->systemShutdownEvent(dynamic_cast<SystemShutdownEvent *>(event));
   else if (event->type() == ListenerControlEvent::Type)
      impl->listenerControlEvent(dynamic_cast<ListenerControlEvent *>(event));
}
