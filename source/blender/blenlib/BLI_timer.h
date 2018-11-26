/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_TIMER_H__
#define __BLI_TIMER_H__

#include "BLI_sys_types.h"

/** \file BLI_timer.h
 *  \ingroup BLI
 */

/* ret < 0: the timer will be removed.
 * ret >= 0: the timer will be called again in ret seconds */
typedef double (*BLI_timer_func)(uintptr_t uuid, void *user_data);
typedef void (*BLI_timer_data_free)(uintptr_t uuid, void *user_data);

/* `func(...) < 0`: The timer will be removed.
 * `func(...) >= 0`: The function will be called again in that many seconds. */
void BLI_timer_register(
        uintptr_t uuid,
        BLI_timer_func func,
        void *user_data,
        BLI_timer_data_free user_data_free,
        double first_interval,
        bool persistent);

bool BLI_timer_is_registered(uintptr_t uuid);

/* Returns False when the timer does not exist (anymore). */
bool BLI_timer_unregister(uintptr_t uuid);

/* Execute all registered functions that are due. */
void BLI_timer_execute(void);

void BLI_timer_free(void);

#endif  /* __BLI_TIMER_H__ */
