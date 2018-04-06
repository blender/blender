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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/undo/undo_system_types.c
 *  \ingroup edundo
 */

#include <string.h>

#include "BLI_utildefines.h"


#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_lattice.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_paint.h"
#include "ED_particle.h"
#include "ED_sculpt.h"
#include "ED_text.h"
#include "ED_undo.h"
#include "undo_intern.h"

/* Keep last */
#include "BKE_undo_system.h"

void ED_undosys_type_init(void)
{
	/* Edit Modes */
	BKE_undosys_type_append(ED_armature_undosys_type);
	BKE_undosys_type_append(ED_curve_undosys_type);
	BKE_undosys_type_append(ED_font_undosys_type);
	BKE_undosys_type_append(ED_lattice_undosys_type);
	BKE_undosys_type_append(ED_mball_undosys_type);
	BKE_undosys_type_append(ED_mesh_undosys_type);

	/* Paint Modes */
	BKE_UNDOSYS_TYPE_IMAGE = BKE_undosys_type_append(ED_image_undosys_type);

	BKE_UNDOSYS_TYPE_SCULPT = BKE_undosys_type_append(ED_sculpt_undosys_type);

	BKE_UNDOSYS_TYPE_PARTICLE = BKE_undosys_type_append(ED_particle_undosys_type);

	BKE_UNDOSYS_TYPE_PAINTCURVE = BKE_undosys_type_append(ED_paintcurve_undosys_type);

	/* Text editor */
	BKE_UNDOSYS_TYPE_TEXT = BKE_undosys_type_append(ED_text_undosys_type);

	/* Keep global undo last (as a fallback). */
	BKE_UNDOSYS_TYPE_MEMFILE = BKE_undosys_type_append(ED_memfile_undosys_type);
}

void ED_undosys_type_free(void)
{
	BKE_undosys_type_free_all();
}
