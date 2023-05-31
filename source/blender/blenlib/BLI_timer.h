/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h"

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \return A value of:
 * - <  0: the timer will be removed.
 * - >= 0: the timer will be called again in this number of seconds.
 */
typedef double (*BLI_timer_func)(uintptr_t uuid, void *user_data);
typedef void (*BLI_timer_data_free)(uintptr_t uuid, void *user_data);

/* `func(...) < 0`: The timer will be removed.
 * `func(...) >= 0`: The function will be called again in that many seconds. */
void BLI_timer_register(uintptr_t uuid,
                        BLI_timer_func func,
                        void *user_data,
                        BLI_timer_data_free user_data_free,
                        double first_interval,
                        bool persistent);

bool BLI_timer_is_registered(uintptr_t uuid);

/** Returns False when the timer does not exist (anymore). */
bool BLI_timer_unregister(uintptr_t uuid);

/** Execute all registered functions that are due. */
void BLI_timer_execute(void);

void BLI_timer_free(void);

/* This function is to be called next to BKE_CB_EVT_LOAD_PRE, to make sure the module
 * is properly configured for the new file. */
void BLI_timer_on_file_load(void);

#ifdef __cplusplus
}
#endif
