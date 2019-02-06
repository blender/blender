/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup bli
 */

#include "BLI_timer.h"
#include "BLI_listbase.h"
#include "BLI_callbacks.h"

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
	bool file_load_cb_registered;
} TimerContainer;

static TimerContainer GlobalTimer = {{0}};

static void ensure_callback_is_registered(void);

void BLI_timer_register(
        uintptr_t uuid,
        BLI_timer_func func,
        void *user_data,
        BLI_timer_data_free user_data_free,
        double first_interval,
        bool persistent)
{
	ensure_callback_is_registered();

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
	LISTBASE_FOREACH(TimedFunction *, timed_func, &GlobalTimer.funcs) {
		if (timed_func->uuid == uuid) {
			if (timed_func->tag_removal) {
				return false;
			}
			else {
				timed_func->tag_removal = true;
				clear_user_data(timed_func);
				return true;
			}
		}
	}
	return false;
}

bool BLI_timer_is_registered(uintptr_t uuid)
{
	LISTBASE_FOREACH(TimedFunction *, timed_func, &GlobalTimer.funcs) {
		if (timed_func->uuid == uuid && !timed_func->tag_removal) {
			return true;
		}
	}
	return false;
}

static void execute_functions_if_necessary(void)
{
	double current_time = GET_TIME();

	LISTBASE_FOREACH(TimedFunction *, timed_func, &GlobalTimer.funcs) {
		if (timed_func->tag_removal) continue;
		if (timed_func->next_time > current_time) continue;

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
	for (TimedFunction *timed_func = GlobalTimer.funcs.first; timed_func; ) {
		TimedFunction *next = timed_func->next;
		if (timed_func->tag_removal) {
			clear_user_data(timed_func);
			BLI_freelinkN(&GlobalTimer.funcs, timed_func);
		}
		timed_func = next;
	}
}

void BLI_timer_execute()
{
	execute_functions_if_necessary();
	remove_tagged_functions();
}

void BLI_timer_free()
{
	LISTBASE_FOREACH(TimedFunction *, timed_func, &GlobalTimer.funcs) {
		timed_func->tag_removal = true;
	}

	remove_tagged_functions();
}

struct ID;
struct Main;
static void remove_non_persistent_functions(struct Main *UNUSED(_1), struct ID *UNUSED(_2), void *UNUSED(_3))
{
	LISTBASE_FOREACH(TimedFunction *, timed_func, &GlobalTimer.funcs) {
		if (!timed_func->persistent) {
			timed_func->tag_removal = true;
		}
	}
}

static bCallbackFuncStore load_post_callback = {
	NULL, NULL, /* next, prev */
	remove_non_persistent_functions, /* func */
	NULL, /* arg */
	0 /* alloc */
};

static void ensure_callback_is_registered()
{
	if (!GlobalTimer.file_load_cb_registered) {
		BLI_callback_add(&load_post_callback, BLI_CB_EVT_LOAD_POST);
		GlobalTimer.file_load_cb_registered = true;
	}
}
