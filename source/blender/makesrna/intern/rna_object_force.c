/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_object_types.h"
#include "DNA_object_force.h"

#ifdef RNA_RUNTIME

#else

static void rna_def_pointcache(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "PointCache", NULL);
	RNA_def_struct_ui_text(srna, "Point Cache", "Point cache for physics simulations.");
}

static void rna_def_collision(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "CollisionSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_ui_text(srna, "Collision Settings", "Collision settings for object in physics simulation.");
}

static void rna_def_field(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "FieldSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_ui_text(srna, "Field Settings", "Field settings for an object in physics simulation.");
}

static void rna_def_game_softbody(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "GameSoftBodySettings", NULL);
	RNA_def_struct_sdna(srna, "BulletSoftBody");
	RNA_def_struct_ui_text(srna, "Game Soft Body Settings", "Soft body simulation settings for an object in the game engine.");
}

static void rna_def_softbody(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SoftBodySettings", NULL);
	RNA_def_struct_sdna(srna, "SoftBody");
	RNA_def_struct_ui_text(srna, "Soft Body Settings", "Soft body simulation settings for an object.");
}

void RNA_def_object_force(BlenderRNA *brna)
{
	rna_def_pointcache(brna);
	rna_def_collision(brna);
	rna_def_field(brna);
	rna_def_game_softbody(brna);
	rna_def_softbody(brna);
}

#endif

