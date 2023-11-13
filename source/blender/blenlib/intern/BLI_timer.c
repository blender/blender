/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_timer.h"
#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#define GET_TIME() PIL_check_seconds_timer()

typedef struct TimedFunction {
  struct TimedFunction *next, *prev;
  BLI_timer_func func;
  BLI_timer_data_free user_data_free;
  void *user_data;
  double next_time;
  uintptr_t uuid;
  bool tag_removal;
  bool persistent;
} TimedFunction;

typedef struct TimerContainer {
  ListBase funcs;
} TimerContainer;

static TimerContainer GlobalTimer = {{0}};

void BLI_timer_register(uintptr_t uuid,
                        BLI_timer_func func,
                        void *user_data,
                        BLI_timer_data_free user_data_free,
                        double first_interval,
                        bool persistent)
{
  TimedFunction *timed_func = MEM_callocN(sizeof(TimedFunction), __func__);
  timed_func->func = func;
  timed_func->user_data_free = user_data_free;
  timed_func->user_data = user_data;
  timed_func->next_time = GET_TIME() + first_interval;
  timed_func->tag_removal = false;
  timed_func->persistent = persistent;
  timed_func->uuid = uuid;

  BLI_addtail(&GlobalTimer.funcs, timed_func);
}

static void clear_user_data(TimedFunction *timed_func)
{
  if (timed_func->user_data_free) {
    timed_func->user_data_free(timed_func->uuid, timed_func->user_data);
    timed_func->user_data_free = NULL;
  }
}

bool BLI_timer_unregister(uintptr_t uuid)
{
  LISTBASE_FOREACH (TimedFunction *, timed_func, &GlobalTimer.funcs) {
    if (timed_func->uuid == uuid && !timed_func->tag_removal) {
      timed_func->tag_removal = true;
      clear_user_data(timed_func);
      return true;
    }
  }
  return false;
}

bool BLI_timer_is_registered(uintptr_t uuid)
{
  LISTBASE_FOREACH (TimedFunction *, timed_func, &GlobalTimer.funcs) {
    if (timed_func->uuid == uuid && !timed_func->tag_removal) {
      return true;
    }
  }
  return false;
}

static void execute_functions_if_necessary(void)
{
  double current_time = GET_TIME();

  LISTBASE_FOREACH (TimedFunction *, timed_func, &GlobalTimer.funcs) {
    if (timed_func->tag_removal) {
      continue;
    }
    if (timed_func->next_time > current_time) {
      continue;
    }

    double ret = timed_func->func(timed_func->uuid, timed_func->user_data);

    if (ret < 0) {
      timed_func->tag_removal = true;
    }
    else {
      timed_func->next_time = current_time + ret;
    }
  }
}

static void remove_tagged_functions(void)
{
  for (TimedFunction *timed_func = GlobalTimer.funcs.first; timed_func;) {
    TimedFunction *next = timed_func->next;
    if (timed_func->tag_removal) {
      clear_user_data(timed_func);
      BLI_freelinkN(&GlobalTimer.funcs, timed_func);
    }
    timed_func = next;
  }
}

void BLI_timer_execute(void)
{
  execute_functions_if_necessary();
  remove_tagged_functions();
}

void BLI_timer_free(void)
{
  LISTBASE_FOREACH (TimedFunction *, timed_func, &GlobalTimer.funcs) {
    timed_func->tag_removal = true;
  }

  remove_tagged_functions();
}

static void remove_non_persistent_functions(void)
{
  LISTBASE_FOREACH (TimedFunction *, timed_func, &GlobalTimer.funcs) {
    if (!timed_func->persistent) {
      timed_func->tag_removal = true;
    }
  }
}

void BLI_timer_on_file_load(void)
{
  remove_non_persistent_functions();
}
