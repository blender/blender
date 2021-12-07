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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
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
