/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;

/**
 * Called from #do_versions() in `readfile.c` to convert the old 'IPO/adrcode' system
 * to the new 'Animato/RNA' system.
 *
 * The basic method used here, is to loop over data-blocks which have IPO-data,
 * and add those IPO's to new AnimData blocks as Actions.
 * Action/NLA data only works well for Objects, so these only need to be checked for there.
 *
 * Data that has been converted should be freed immediately, which means that it is immediately
 * clear which data-blocks have yet to be converted, and also prevent freeing errors when we exit.
 *
 * \note Currently done after all file reading.
 */
void do_versions_ipos_to_animato(struct Main *main);

/* --------------------- xxx stuff ------------------------ */

#ifdef __cplusplus
};
#endif
