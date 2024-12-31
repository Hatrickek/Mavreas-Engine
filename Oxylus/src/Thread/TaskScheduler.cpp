#include "TaskScheduler.hpp"

#include <TaskScheduler.h>

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
void TaskScheduler::init() {
  OX_SCOPED_ZONE;
  task_scheduler = create_unique<enki::TaskScheduler>();
  task_scheduler->Initialize();
  task_sets.reserve(100);

  OX_LOG_INFO("TaskScheduler initalized.");
}

void TaskScheduler::deinit() {
  task_scheduler->ShutdownNow();
}

void TaskScheduler::wait_for_all() {
  task_scheduler->WaitforAll();

  task_sets.clear();
}
}
