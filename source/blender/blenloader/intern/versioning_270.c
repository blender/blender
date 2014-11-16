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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_270.c
 *  \ingroup blenloader
 */

#include "BLI_utildefines.h"
#include "BLI_compiler_attrs.h"

/* for MinGW32 definition of NULL, could use BLI_blenlib.h instead too */
#include <stddef.h>

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_brush_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_actuator_types.h"

#include "DNA_genfile.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_main.h"
#include "BKE_node.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BLO_readfile.h"

#include "readfile.h"


static void do_version_constraints_radians_degrees_270_1(ListBase *lb)
{
	bConstraint *con;

	for (con = lb->first; con; con = con->next) {
		if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
			bTransformConstraint *data = (bTransformConstraint *)con->data;
			const float deg_to_rad_f = DEG2RADF(1.0f);

			if (data->from == TRANS_ROTATION) {
				mul_v3_fl(data->from_min, deg_to_rad_f);
				mul_v3_fl(data->from_max, deg_to_rad_f);
			}

			if (data->to == TRANS_ROTATION) {
				mul_v3_fl(data->to_min, deg_to_rad_f);
				mul_v3_fl(data->to_max, deg_to_rad_f);
			}
		}
	}
}

static void do_version_constraints_radians_degrees_270_5(ListBase *lb)
{
	bConstraint *con;

	for (con = lb->first; con; con = con->next) {
		if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
			bTransformConstraint *data = (bTransformConstraint *)con->data;

			if (data->from == TRANS_ROTATION) {
				copy_v3_v3(data->from_min_rot, data->from_min);
				copy_v3_v3(data->from_max_rot, data->from_max);
			}
			else if (data->from == TRANS_SCALE) {
				copy_v3_v3(data->from_min_scale, data->from_min);
				copy_v3_v3(data->from_max_scale, data->from_max);
			}

			if (data->to == TRANS_ROTATION) {
				copy_v3_v3(data->to_min_rot, data->to_min);
				copy_v3_v3(data->to_max_rot, data->to_max);
			}
			else if (data->to == TRANS_SCALE) {
				copy_v3_v3(data->to_min_scale, data->to_min);
				copy_v3_v3(data->to_max_scale, data->to_max);
			}
		}
	}
}

static void do_version_constraints_stretch_to_limits(ListBase *lb)
{
	bConstraint *con;

	for (con = lb->first; con; con = con->next) {
		if (con->type == CONSTRAINT_TYPE_STRETCHTO) {
			bStretchToConstraint *data = (bStretchToConstraint *)con->data;
			data->bulge_min = 1.0f;
			data->bulge_max = 1.0f;
		}
	}
}

void blo_do_versions_270(FileData *fd, Library *UNUSED(lib), Main *main)
{
	if (!MAIN_VERSION_ATLEAST(main, 270, 0)) {

		if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "float", "profile")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Bevel) {
						BevelModifierData *bmd = (BevelModifierData *)md;
						bmd->profile = 0.5f;
						bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
					}
				}
			}
		}

		/* nodes don't use fixed node->id any more, clean up */
		FOREACH_NODETREE(main, ntree, id) {
			if (ntree->type == NTREE_COMPOSIT) {
				bNode *node;
				for (node = ntree->nodes.first; node; node = node->next) {
					if (ELEM(node->type, CMP_NODE_COMPOSITE, CMP_NODE_OUTPUT_FILE)) {
						node->id = NULL;
					}
				}
			}
		} FOREACH_NODETREE_END

		{
			bScreen *screen;

			for (screen = main->screen.first; screen; screen = screen->id.next) {
				ScrArea *area;
				for (area = screen->areabase.first; area; area = area->next) {
					SpaceLink *space_link;
					for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
						if (space_link->spacetype == SPACE_CLIP) {
							SpaceClip *space_clip = (SpaceClip *) space_link;
							if (space_clip->mode != SC_MODE_MASKEDIT) {
								space_clip->mode = SC_MODE_TRACKING;
							}
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingSettings", "float", "default_weight")) {
			MovieClip *clip;
			for (clip = main->movieclip.first; clip; clip = clip->id.next) {
				clip->tracking.settings.default_weight = 1.0f;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 270, 1)) {
		Scene *sce;
		Object *ob;

		/* Update Transform constraint (another deg -> rad stuff). */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			do_version_constraints_radians_degrees_270_1(&ob->constraints);

			if (ob->pose) {
				/* Bones constraints! */
				bPoseChannel *pchan;
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					do_version_constraints_radians_degrees_270_1(&pchan->constraints);
				}
			}
		}

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->r.raytrace_structure == R_RAYSTRUCTURE_BLIBVH) {
				sce->r.raytrace_structure = R_RAYSTRUCTURE_AUTO;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 270, 2)) {
		Mesh *me;

		/* Mesh smoothresh deg->rad. */
		for (me = main->mesh.first; me; me = me->id.next) {
			me->smoothresh = DEG2RADF(me->smoothresh);
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 270, 3)) {
		FreestyleLineStyle *linestyle;

		for (linestyle = main->linestyle.first; linestyle; linestyle = linestyle->id.next) {
			linestyle->flag |= LS_NO_SORTING;
			linestyle->sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA;
			linestyle->integration_type = LS_INTEGRATION_MEAN;
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 270, 4)) {
		/* ui_previews were not handled correctly when copying areas, leading to corrupted files (see T39847).
		 * This will always reset situation to a valid state.
		 */
		bScreen *sc;

		for (sc = main->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;

				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					ARegion *ar;
					ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

					for (ar = lb->first; ar; ar = ar->next) {
						BLI_listbase_clear(&ar->ui_previews);
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 270, 5)) {
		Object *ob;

		/* Update Transform constraint (again :|). */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			do_version_constraints_radians_degrees_270_5(&ob->constraints);

			if (ob->pose) {
				/* Bones constraints! */
				bPoseChannel *pchan;
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					do_version_constraints_radians_degrees_270_5(&pchan->constraints);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 271, 0)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Material", "int", "mode2")) {
			Material *ma;

			for (ma = main->mat.first; ma; ma = ma->id.next)
				ma->mode2 = MA_CASTSHADOW;
		}

		if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "BakeData", "bake")) {
			Scene *sce;

			for (sce = main->scene.first; sce; sce = sce->id.next) {
				sce->r.bake.flag = R_BAKE_CLEAR;
				sce->r.bake.width = 512;
				sce->r.bake.height = 512;
				sce->r.bake.margin = 16;
				sce->r.bake.normal_space = R_BAKE_SPACE_TANGENT;
				sce->r.bake.normal_swizzle[0] = R_BAKE_POSX;
				sce->r.bake.normal_swizzle[1] = R_BAKE_POSY;
				sce->r.bake.normal_swizzle[2] = R_BAKE_POSZ;
				BLI_strncpy(sce->r.bake.filepath, U.renderdir, sizeof(sce->r.bake.filepath));

				sce->r.bake.im_format.planes = R_IMF_PLANES_RGBA;
				sce->r.bake.im_format.imtype = R_IMF_IMTYPE_PNG;
				sce->r.bake.im_format.depth = R_IMF_CHAN_DEPTH_8;
				sce->r.bake.im_format.quality = 90;
				sce->r.bake.im_format.compress = 15;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "FreestyleLineStyle", "float", "texstep")) {
			FreestyleLineStyle *linestyle;

			for (linestyle = main->linestyle.first; linestyle; linestyle = linestyle->id.next) {
				linestyle->flag |= LS_TEXTURE;
				linestyle->texstep = 1.0;
			}
		}

		{
			Scene *scene;
			for (scene = main->scene.first; scene; scene = scene->id.next) {
				int num_layers = BLI_listbase_count(&scene->r.layers);
				scene->r.actlay = min_ff(scene->r.actlay, num_layers - 1);
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 271, 1)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Material", "float", "line_col[4]")) {
			Material *mat;

			for (mat = main->mat.first; mat; mat = mat->id.next) {
				mat->line_col[0] = mat->line_col[1] = mat->line_col[2] = 0.0f;
				mat->line_col[3] = mat->alpha;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "int", "preview_start_resolution")) {
			Scene *scene;
			for (scene = main->scene.first; scene; scene = scene->id.next) {
				scene->r.preview_start_resolution = 64;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 271, 2)) {
		/* init up & track axis property of trackto actuators */
		Object *ob;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			bActuator *act;
			for (act = ob->actuators.first; act; act = act->next) {
				if (act->type == ACT_EDIT_OBJECT) {
					bEditObjectActuator *eoact = act->data;
					eoact->trackflag = ob->trackflag;
					/* if trackflag is pointing +-Z axis then upflag should point Y axis.
					 * Rest of trackflag cases, upflag should be point z axis */
					if ((ob->trackflag == OB_POSZ) || (ob->trackflag == OB_NEGZ)) {
						eoact->upflag = 1;
					}
					else {
						eoact->upflag = 2;
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 271, 3)) {
		Brush *br;

		for (br = main->brush.first; br; br = br->id.next) {
			br->fill_threshold = 0.2f;
		}

		if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "int", "mat")) {
			Object *ob;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;

				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Bevel) {
						BevelModifierData *bmd = (BevelModifierData *)md;
						bmd->mat = -1;
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 271, 6)) {
		Object *ob;
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;

			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_ParticleSystem) {
					ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
					if (pmd->psys && pmd->psys->clmd) {
						pmd->psys->clmd->sim_parms->vel_damping = 1.0f;
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 272, 0)) {
		if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "int", "preview_start_resolution")) {
			Scene *scene;
			for (scene = main->scene.first; scene; scene = scene->id.next) {
				scene->r.preview_start_resolution = 64;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 272, 1)) {
		Brush *br;
		for (br = main->brush.first; br; br = br->id.next) {
			if ((br->ob_mode & OB_MODE_SCULPT) && ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK))
				br->alpha = 1.0f;
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 272, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Image", "float", "gen_color")) {
			Image *image;
			for (image = main->image.first; image != NULL; image = image->id.next) {
				image->gen_color[3] = 1.0f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "bStretchToConstraint", "float", "bulge_min")) {
			Object *ob;

			/* Update Transform constraint (again :|). */
			for (ob = main->object.first; ob; ob = ob->id.next) {
				do_version_constraints_stretch_to_limits(&ob->constraints);

				if (ob->pose) {
					/* Bones constraints! */
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
						do_version_constraints_stretch_to_limits(&pchan->constraints);
					}
				}
			}
		}
	}
}
