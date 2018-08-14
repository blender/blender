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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subdiv_displacement.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

void BKE_subdiv_displacement_detach(Subdiv *subdiv)
{
	if (subdiv->displacement_evaluator == NULL) {
		return;
	}
	if (subdiv->displacement_evaluator->free != NULL) {
		subdiv->displacement_evaluator->free(subdiv->displacement_evaluator);
	}
	MEM_freeN(subdiv->displacement_evaluator);
	subdiv->displacement_evaluator = NULL;
}
