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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2008,2009  Joshua Leung (Animation Cleanup, Animation Systme Recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_IPO_H__
#define __BKE_IPO_H__

/** \file BKE_ipo.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 *  \author Joshua Leung
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Ipo;

void do_versions_ipos_to_animato(struct Main *main);

/* --------------------- xxx stuff ------------------------ */

void BKE_ipo_free(struct Ipo *ipo);

#ifdef __cplusplus
};
#endif

#endif

