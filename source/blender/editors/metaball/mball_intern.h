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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/metaball/mball_intern.h
 *  \ingroup edmeta
 */


#ifndef __MBALL_INTERN_H__
#define __MBALL_INTERN_H__

#include "DNA_object_types.h"

#include "DNA_windowmanager_types.h"

void MBALL_OT_hide_metaelems(struct wmOperatorType *ot);
void MBALL_OT_reveal_metaelems(struct wmOperatorType *ot);

void MBALL_OT_delete_metaelems(struct wmOperatorType *ot);
void MBALL_OT_duplicate_metaelems(struct wmOperatorType *ot);

void MBALL_OT_select_all(struct wmOperatorType *ot);
void MBALL_OT_select_random_metaelems(struct wmOperatorType *ot);

#endif

