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

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <mutex>
#include <queue>
#include <list>
#include <thread>
#include <condition_variable>

#include <rt/Logger.h>
#include <rt/BlockingQueue.h>
#include <rt/Executor.h>

namespace rt {

struct Executor::Impl
{
   Logger *log = Logger::getLogger("rt.Executor");

   struct Job
   {
      std::shared_ptr<Task> task;
      Priority priority;

      bool operator==(const Job &other) const
      {
         return task == other.task;
      }
   };

   // max number of tasks in pool (waiting + running)
   int poolSize;

   // running threads
   std::list<std::thread> threadList;

   // waiting group
   std::condition_variable threadSync;

   // waiting tasks pool
   BlockingQueue<Job> waitingJobs;

   // current running tasks
   BlockingQueue<Job> runningJobs;

   // shutdown flag
   std::atomic<bool> shutdown;

   // sync mutex
   std::mutex syncMutex;

   Impl(const int poolSize, int coreSize) : poolSize(poolSize), shutdown(false)
   {
      log->info("executor service starting with {} threads", {coreSize});

      // create new thread group
      for (int i = 0; i < coreSize; i++)
      {
         threadList.emplace_back([this] { this->exec(); });
      }
   }

   void exec()
   {
      // get current thread id
      std::thread::id id = std::this_thread::get_id();

      // main thread loop
      while (!shutdown)
      {
         if (auto next = waitingJobs.get())
         {
            const auto &job = next.value();
            const auto task = job.task;

            runningJobs.add(job);

            // restore priority
            setPriority(job.priority);

            try
            {
               log->info("task {} started in thread {} with priority {}", {task->name(), id, priorityName(job.priority)});

               task->run();
            }
            catch (std::exception &e)
            {
               log->error("##################################################");
               log->error("exception in {}: {}", {task->name(), std::string(e.what())});
               log->error("##################################################");
            }
            catch (...)
            {
               log->error("##################################################");
               log->error("unhandled exception in {}", {task->name()});
               log->error("##################################################");
            }

            log->info("task {} finished in thread {}", {task->name(), id});

            // restore priority
            setPriority(PRIORITY_NORMAL);

            // on shutdown process do not remove from list to avoid concurrent modification
            if (!shutdown)
               runningJobs.remove(job);
         }
         else if (!shutdown)
         {
            // lock mutex before wait in condition variable
            std::unique_lock lock(syncMutex);

            // stop thread until is notified
            threadSync.wait(lock);
         }
      }

      log->info("executor thread {} terminated", {id});
   }

   void submit(Task *task, Priority priority)
   {
      if (!shutdown)
      {
         // add task to wait pool
         waitingJobs.add({std::shared_ptr<Task>(task), priority});

         // notify waiting threads
         threadSync.notify_all();
      }
      else
      {
         log->warn("submit task rejected, shutdown in progress...");
      }
   }

   void terminate(int timeout)
   {
      log->info("stopping threads of the executor service, timeout {}", {timeout});

      // signal executor shutdown
      shutdown = true;

      // terminate running tasks
      while (const auto job = runningJobs.get())
      {
         log->debug("send terminate request for task {}", {job->task->name()});

         job->task->terminate();
      }

      // notify waiting threads
      threadSync.notify_all();

      log->info("now waiting for completion of all executor threads");

      // joint all threads
      for (auto &thread: threadList)
      {
         if (thread.joinable())
         {
            log->debug("joint on thread {}", {thread.get_id()});

            thread.join();
         }
      }

      // finally remove waiting tasks
      waitingJobs.clear();

      log->info("all threads terminated, executor service shutdown completed!");
   }

   void setPriority(Priority priority)
   {
#ifdef _WIN32
      int p = THREAD_PRIORITY_NORMAL;

      switch (priority)
      {
         case PRIORITY_LOWEST:
            p = THREAD_PRIORITY_LOWEST;
            break;
         case PRIORITY_HIGHEST:
            p = THREAD_PRIORITY_HIGHEST;
            break;
         case PRIORITY_REALTIME:
            p = THREAD_PRIORITY_TIME_CRITICAL;
            break;
      }

      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#else
      sched_param param {10};

      switch (priority)
      {
         case PRIORITY_LOWEST:
            param = {0};
            break;
         case PRIORITY_HIGHEST:
            param = {0};
            break;
         case PRIORITY_REALTIME:
            param = {25};
            break;
      }

      pthread_setschedparam(pthread_self(), priority > PRIORITY_NORMAL ? SCHED_RR : SCHED_OTHER, &param);
#endif
   }

   static std::string priorityName(const Priority priority)
   {
      // priority names
      static const std::map<Priority, std::string> names = {
         {PRIORITY_LOWEST, "LOWEST"},
         {PRIORITY_NORMAL, "NORMAL"},
         {PRIORITY_HIGHEST, "HIGHEST"},
         {PRIORITY_REALTIME, "REALTIME"},
      };

      if (const auto it = names.find(priority); it != names.end())
         return it->second;

      return "UNKNOWN";
   }
};

Executor::Executor(int poolSize, int coreSize) : impl(std::make_shared<Impl>(poolSize, coreSize))
{
}

Executor::~Executor()
{
   impl->terminate(0);
}

void Executor::submit(Task *task, Priority priority)
{
   impl->submit(task, priority);
}

void Executor::shutdown()
{
   impl->terminate(0);
}

}
