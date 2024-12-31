#pragma once
#include <TaskScheduler.h>

#include "Core/ESystem.hpp"

namespace ox {
using TaskSet = enki::TaskSet;
using ITaskSet = enki::ITaskSet;
using IPinnedTask = enki::IPinnedTask;
using ICompleteableTask = enki::ICompletable;
using AsyncTask = enki::TaskSet;
using TaskSetPartition = enki::TaskSetPartition;
using TaskFunction = enki::TaskSetFunction;

class TaskScheduler : public ESystem {
public:
  TaskScheduler() = default;

  void init() override;
  void deinit() override;

  Unique<enki::TaskScheduler>& get() { return task_scheduler; }

  template <typename func>
  void add_task(func function) {
    const enki::TaskSetFunction f = [function](enki::TaskSetPartition, uint32_t) mutable { function(); };
    task_scheduler->AddTaskSetToPipe(task_sets.emplace_back(create_unique<TaskSet>(f)).get());
  }

  void schedule_task(ITaskSet* set) const { task_scheduler->AddTaskSetToPipe(set); }

  void schedule_task(IPinnedTask* set) const { task_scheduler->AddPinnedTask(set); }

  void wait_task(const ITaskSet* set) const { task_scheduler->WaitforTask(set); }

  void wait_for_all();

private:
  Unique<enki::TaskScheduler> task_scheduler;
  std::vector<Unique<TaskSet>> task_sets = {};
};
} // namespace ox
