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

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_actuator_types.h"
#include "DNA_view3d_types.h"

#include "DNA_genfile.h"

#include "BKE_colortools.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_screen.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "MEM_guardedalloc.h"

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

static void do_version_action_editor_properties_region(ListBase *regionbase)
{
	ARegion *ar;
	
	for (ar = regionbase->first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_UI) {
			/* already exists */
			return;
		}
		else if (ar->regiontype == RGN_TYPE_WINDOW) {
			/* add new region here */
			ARegion *arnew = MEM_callocN(sizeof(ARegion), "buttons for action");
			
			BLI_insertlinkbefore(regionbase, ar, arnew);
			
			arnew->regiontype = RGN_TYPE_UI;
			arnew->alignment = RGN_ALIGN_RIGHT;
			arnew->flag = RGN_FLAG_HIDDEN;
			
			return;
		}
	}
}

static void do_version_bones_super_bbone(ListBase *lb)
{
	for (Bone *bone = lb->first; bone; bone = bone->next) {
		bone->scaleIn = 1.0f;
		bone->scaleOut = 1.0f;
		
		do_version_bones_super_bbone(&bone->childbase);
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

	if (!MAIN_VERSION_ATLEAST(main, 273, 1)) {
#define	BRUSH_RAKE (1 << 7)
#define BRUSH_RANDOM_ROTATION (1 << 25)

		Brush *br;

		for (br = main->brush.first; br; br = br->id.next) {
			if (br->flag & BRUSH_RAKE) {
				br->mtex.brush_angle_mode |= MTEX_ANGLE_RAKE;
				br->mask_mtex.brush_angle_mode |= MTEX_ANGLE_RAKE;
			}
			else if (br->flag & BRUSH_RANDOM_ROTATION) {
				br->mtex.brush_angle_mode |= MTEX_ANGLE_RANDOM;
				br->mask_mtex.brush_angle_mode |= MTEX_ANGLE_RANDOM;
			}
			br->mtex.random_angle = 2.0 * M_PI;
			br->mask_mtex.random_angle = 2.0 * M_PI;
		}
	}

#undef BRUSH_RAKE
#undef BRUSH_RANDOM_ROTATION

	/* Customizable Safe Areas */
	if (!MAIN_VERSION_ATLEAST(main, 273, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Scene", "DisplaySafeAreas", "safe_areas")) {
			Scene *scene;

			for (scene = main->scene.first; scene; scene = scene->id.next) {
				copy_v2_fl2(scene->safe_areas.title, 3.5f / 100.0f, 3.5f / 100.0f);
				copy_v2_fl2(scene->safe_areas.action, 10.0f / 100.0f, 5.0f / 100.0f);
				copy_v2_fl2(scene->safe_areas.title_center, 17.5f / 100.0f, 5.0f / 100.0f);
				copy_v2_fl2(scene->safe_areas.action_center, 15.0f / 100.0f, 5.0f / 100.0f);
			}
		}
	}
	
	if (!MAIN_VERSION_ATLEAST(main, 273, 3)) {
		ParticleSettings *part;
		for (part = main->particle.first; part; part = part->id.next) {
			if (part->clumpcurve)
				part->child_flag |= PART_CHILD_USE_CLUMP_CURVE;
			if (part->roughcurve)
				part->child_flag |= PART_CHILD_USE_ROUGH_CURVE;
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 273, 6)) {
		if (!DNA_struct_elem_find(fd->filesdna, "ClothSimSettings", "float", "bending_damping")) {
			Object *ob;
			ModifierData *md;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Cloth) {
						ClothModifierData *clmd = (ClothModifierData *)md;
						clmd->sim_parms->bending_damping = 0.5f;
					}
					else if (md->type == eModifierType_ParticleSystem) {
						ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
						if (pmd->psys->clmd) {
							pmd->psys->clmd->sim_parms->bending_damping = 0.5f;
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "ParticleSettings", "float", "clump_noise_size")) {
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				part->clump_noise_size = 1.0f;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "ParticleSettings", "int", "kink_extra_steps")) {
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				part->kink_extra_steps = 4;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "MTex", "float", "kinkampfac")) {
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				int a;
				for (a = 0; a < MAX_MTEX; a++) {
					MTex *mtex = part->mtex[a];
					if (mtex) {
						mtex->kinkampfac = 1.0f;
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "HookModifierData", "char", "flag")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Hook) {
						HookModifierData *hmd = (HookModifierData *)md;
						hmd->falloff_type = eHook_Falloff_InvSquare;
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "NodePlaneTrackDeformData", "char", "flag")) {
			FOREACH_NODETREE(main, ntree, id) {
				if (ntree->type == NTREE_COMPOSIT) {
					bNode *node;
					for (node = ntree->nodes.first; node; node = node->next) {
						if (ELEM(node->type, CMP_NODE_PLANETRACKDEFORM)) {
							NodePlaneTrackDeformData *data = node->storage;
							data->flag = 0;
							data->motion_blur_samples = 16;
							data->motion_blur_shutter = 0.5f;
						}
					}
				}
			}
			FOREACH_NODETREE_END
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Camera", "GPUDOFSettings", "gpu_dof")) {
			Camera *ca;
			for (ca = main->camera.first; ca; ca = ca->id.next) {
				ca->gpu_dof.fstop = 128.0f;
				ca->gpu_dof.focal_length = 1.0f;
				ca->gpu_dof.focus_distance = 1.0f;
				ca->gpu_dof.sensor = 1.0f;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 273, 8)) {
		Object *ob;
		for (ob = main->object.first; ob != NULL; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.last; md != NULL; md = md->prev) {
				if (modifier_unique_name(&ob->modifiers, md)) {
					printf("Warning: Object '%s' had several modifiers with the "
					       "same name, renamed one of them to '%s'.\n",
					       ob->id.name + 2, md->name);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 273, 9)) {
		bScreen *scr;
		ScrArea *sa;
		SpaceLink *sl;
		ARegion *ar;

		/* Make sure sequencer preview area limits zoom */
		for (scr = main->screen.first; scr; scr = scr->id.next) {
			for (sa = scr->areabase.first; sa; sa = sa->next) {
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_SEQ) {
						for (ar = sl->regionbase.first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_PREVIEW) {
								ar->v2d.keepzoom |= V2D_LIMITZOOM;
								ar->v2d.minzoom = 0.001f;
								ar->v2d.maxzoom = 1000.0f;
								break;
							}
						}
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 274, 1)) {
		/* particle systems need to be forced to redistribute for jitter mode fix */
		{
			Object *ob;
			ParticleSystem *psys;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				for (psys = ob->particlesystem.first; psys; psys = psys->next) {
					if ((psys->pointcache->flag & PTCACHE_BAKED) == 0) {
						psys->recalc |= PSYS_RECALC_RESET;
					}
				}
			}
		}

		/* hysteresis setted to 10% but not actived */
		if (!DNA_struct_elem_find(fd->filesdna, "LodLevel", "int", "obhysteresis")) {
			Object *ob;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				LodLevel *level;
				for (level = ob->lodlevels.first; level; level = level->next) {
					level->obhysteresis = 10;
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "GameData", "int", "scehysteresis")) {
			Scene *scene;
			for (scene = main->scene.first; scene; scene = scene->id.next) {
				scene->gm.scehysteresis = 10;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 274, 2)) {
		FOREACH_NODETREE(main, ntree, id) {
			bNode *node;
			bNodeSocket *sock;

			for (node = ntree->nodes.first; node; node = node->next) {
				if (node->type == SH_NODE_MATERIAL) {
					for (sock = node->inputs.first; sock; sock = sock->next) {
						if (STREQ(sock->name, "Refl")) {
							BLI_strncpy(sock->name, "DiffuseIntensity", sizeof(sock->name));
						}
					}
				}
				else if (node->type == SH_NODE_MATERIAL_EXT) {
					for (sock = node->outputs.first; sock; sock = sock->next) {
						if (STREQ(sock->name, "Refl")) {
							BLI_strncpy(sock->name, "DiffuseIntensity", sizeof(sock->name));
						}
						else if (STREQ(sock->name, "Ray Mirror")) {
							BLI_strncpy(sock->name, "Reflectivity", sizeof(sock->name));
						}
					}
				}
			}
		} FOREACH_NODETREE_END
	}

	if (!MAIN_VERSION_ATLEAST(main, 274, 4)) {
		SceneRenderView *srv;
		wmWindowManager *wm;
		bScreen *screen;
		wmWindow *win;
		Scene *scene;
		Camera *cam;
		Image *ima;

		for (scene = main->scene.first; scene; scene = scene->id.next) {
			Sequence *seq;

			BKE_scene_add_render_view(scene, STEREO_LEFT_NAME);
			srv = scene->r.views.first;
			BLI_strncpy(srv->suffix, STEREO_LEFT_SUFFIX, sizeof(srv->suffix));

			BKE_scene_add_render_view(scene, STEREO_RIGHT_NAME);
			srv = scene->r.views.last;
			BLI_strncpy(srv->suffix, STEREO_RIGHT_SUFFIX, sizeof(srv->suffix));

			SEQ_BEGIN (scene->ed, seq)
			{
				seq->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Stereo Display 3d Format");

#define SEQ_USE_PROXY_CUSTOM_DIR (1 << 19)
#define SEQ_USE_PROXY_CUSTOM_FILE (1 << 21)
				if (seq->strip && seq->strip->proxy && !seq->strip->proxy->storage) {
					if (seq->flag & SEQ_USE_PROXY_CUSTOM_DIR)
						seq->strip->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_DIR;
					if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE)
						seq->strip->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_FILE;
				}
#undef SEQ_USE_PROXY_CUSTOM_DIR
#undef SEQ_USE_PROXY_CUSTOM_FILE

			}
			SEQ_END
		}

		for (screen = main->screen.first; screen; screen = screen->id.next) {
			ScrArea *sa;
			for (sa = screen->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;

				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					switch (sl->spacetype) {
						case SPACE_VIEW3D:
						{
							View3D *v3d = (View3D *)sl;
							v3d->stereo3d_camera = STEREO_3D_ID;
							v3d->stereo3d_flag |= V3D_S3D_DISPPLANE;
							v3d->stereo3d_convergence_alpha = 0.15f;
							v3d->stereo3d_volume_alpha = 0.05f;
							break;
						}
						case SPACE_IMAGE:
						{
							SpaceImage *sima = (SpaceImage *) sl;
							sima->iuser.flag |= IMA_SHOW_STEREO;
							break;
						}
					}
				}
			}
		}

		for (cam = main->camera.first; cam; cam = cam->id.next) {
			cam->stereo.interocular_distance = 0.065f;
			cam->stereo.convergence_distance = 30.0f * 0.065f;
		}

		for (ima = main->image.first; ima; ima = ima->id.next) {
			ima->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Image Stereo 3d Format");

			if (ima->packedfile) {
				ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image Packed File");
				BLI_addtail(&ima->packedfiles, imapf);

				imapf->packedfile = ima->packedfile;
				BLI_strncpy(imapf->filepath, ima->name, FILE_MAX);
				ima->packedfile = NULL;
			}
		}

		for (wm = main->wm.first; wm; wm = wm->id.next) {
			for (win = wm->windows.first; win; win = win->next) {
				win->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Stereo Display 3d Format");
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 274, 6)) {
		bScreen *screen;

		if (!DNA_struct_elem_find(fd->filesdna, "FileSelectParams", "int", "thumbnail_size")) {
			for (screen = main->screen.first; screen; screen = screen->id.next) {
				ScrArea *sa;

				for (sa = screen->areabase.first; sa; sa = sa->next) {
					SpaceLink *sl;

					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_FILE) {
							SpaceFile *sfile = (SpaceFile *)sl;

							if (sfile->params) {
								sfile->params->thumbnail_size = 128;
							}
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "short", "simplify_subsurf_render")) {
			Scene *scene;
			for (scene = main->scene.first; scene != NULL; scene = scene->id.next) {
				scene->r.simplify_subsurf_render = scene->r.simplify_subsurf;
				scene->r.simplify_particles_render = scene->r.simplify_particles;
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "DecimateModifierData", "float", "defgrp_factor")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Decimate) {
						DecimateModifierData *dmd = (DecimateModifierData *)md;
						dmd->defgrp_factor = 1.0f;
					}
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 275, 3)) {
		Brush *br;
#define BRUSH_TORUS (1 << 1)
		for (br = main->brush.first; br; br = br->id.next) {
			br->flag &= ~BRUSH_TORUS;
		}
#undef BRUSH_TORUS
	}

	if (!MAIN_VERSION_ATLEAST(main, 276, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "custom_scale")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				if (ob->pose) {
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
						pchan->custom_scale = 1.0f;
					}
				}
			}
		}

		{
			bScreen *screen;
#define RV3D_VIEW_PERSPORTHO	 7
			for (screen = main->screen.first; screen; screen = screen->id.next) {
				ScrArea *sa;
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					SpaceLink *sl;
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							ARegion *ar;
							ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
							for (ar = lb->first; ar; ar = ar->next) {
								if (ar->regiontype == RGN_TYPE_WINDOW) {
									if (ar->regiondata) {
										RegionView3D *rv3d = ar->regiondata;
										if (rv3d->view == RV3D_VIEW_PERSPORTHO) {
											rv3d->view = RV3D_VIEW_USER;
										}
									}
								}
							}
							break;
						}
					}
				}
			}
#undef RV3D_VIEW_PERSPORTHO
		}

		{
			Lamp *lamp;
#define LA_YF_PHOTON	5
			for (lamp = main->lamp.first; lamp; lamp = lamp->id.next) {
				if (lamp->type == LA_YF_PHOTON) {
					lamp->type = LA_LOCAL;
				}
			}
#undef LA_YF_PHOTON
		}

		{
			Object *ob;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				if (ob->body_type == OB_BODY_TYPE_CHARACTER && (ob->gameflag & OB_BOUNDS) && ob->collision_boundtype == OB_BOUND_TRIANGLE_MESH) {
					ob->boundtype = ob->collision_boundtype = OB_BOUND_BOX;
				}
			}
		}

	}

	if (!MAIN_VERSION_ATLEAST(main, 276, 3)) {
		if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "CurveMapping", "mblur_shutter_curve")) {
			Scene *scene;
			for (scene = main->scene.first; scene != NULL; scene = scene->id.next) {
				CurveMapping *curve_mapping = &scene->r.mblur_shutter_curve;
				curvemapping_set_defaults(curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f);
				curvemapping_initialize(curve_mapping);
				curvemap_reset(curve_mapping->cm,
				               &curve_mapping->clipr,
				               CURVE_PRESET_MAX,
				               CURVEMAP_SLOPE_POS_NEG);
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 276, 4)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			ToolSettings *ts = scene->toolsettings;
			
			if (ts->gp_sculpt.brush[0].size == 0) {
				GP_BrushEdit_Settings *gset = &ts->gp_sculpt;
				GP_EditBrush_Data *brush;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_SMOOTH];
				brush->size = 25;
				brush->strength = 0.3f;
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF | GP_EDITBRUSH_FLAG_SMOOTH_PRESSURE;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_THICKNESS];
				brush->size = 25;
				brush->strength = 0.5f;
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_GRAB];
				brush->size = 50;
				brush->strength = 0.3f;
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_PUSH];
				brush->size = 25;
				brush->strength = 0.3f;
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_TWIST];
				brush->size = 50;
				brush->strength = 0.3f; // XXX?
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_PINCH];
				brush->size = 50;
				brush->strength = 0.5f; // XXX?
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_RANDOMIZE];
				brush->size = 25;
				brush->strength = 0.5f;
				brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
				
				brush = &gset->brush[GP_EDITBRUSH_TYPE_CLONE];
				brush->size = 50;
				brush->strength = 1.0f;
			}
			
			if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "gpencil_v3d_align")) {
#if 0 /* XXX: Cannot do this, as we get random crashes... */
				if (scene->gpd) {
					bGPdata *gpd = scene->gpd;
					
					/* Copy over the settings stored in the GP datablock linked to the scene, for minimal disruption */
					ts->gpencil_v3d_align = 0;
					
					if (gpd->flag & GP_DATA_VIEWALIGN)    ts->gpencil_v3d_align |= GP_PROJECT_VIEWSPACE;
					if (gpd->flag & GP_DATA_DEPTH_VIEW)   ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_VIEW;
					if (gpd->flag & GP_DATA_DEPTH_STROKE) ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_STROKE;
					
					if (gpd->flag & GP_DATA_DEPTH_STROKE_ENDPOINTS)
						ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_STROKE_ENDPOINTS;
				}
				else {
					/* Default to cursor for all standard 3D views */
					ts->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
				}
#endif
				
				ts->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
				ts->gpencil_v2d_align = GP_PROJECT_VIEWSPACE;
				ts->gpencil_seq_align = GP_PROJECT_VIEWSPACE;
				ts->gpencil_ima_align = GP_PROJECT_VIEWSPACE;
			}
		}
		
		for (bGPdata *gpd = main->gpencil.first; gpd; gpd = gpd->id.next) {
			bool enabled = false;
			
			/* Ensure that the datablock's onionskinning toggle flag
			 * stays in sync with the status of the actual layers
			 */
			for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
				if (gpl->flag & GP_LAYER_ONIONSKIN) {
					enabled = true;
				}
			}
			
			if (enabled)
				gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
			else
				gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
		}

		if (!DNA_struct_elem_find(fd->filesdna, "Object", "unsigned char", "max_jumps")) {
			for (Object *ob = main->object.first; ob; ob = ob->id.next) {
				ob->max_jumps = 1;
			}
		}
	}
	if (!MAIN_VERSION_ATLEAST(main, 276, 5)) {
		ListBase *lbarray[MAX_LIBARRAY];
		int a;

		/* Important to clear all non-persistent flags from older versions here, otherwise they could collide
		 * with any new persistent flag we may add in the future. */
		a = set_listbasepointers(main, lbarray);
		while (a--) {
			for (ID *id = lbarray[a]->first; id; id = id->next) {
				id->flag &= LIB_FAKEUSER;
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 276, 7)) {
		Scene *scene;
		for (scene = main->scene.first; scene != NULL; scene = scene->id.next) {
			scene->r.bake.pass_filter = R_BAKE_PASS_FILTER_ALL;
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 277, 1)) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			ParticleEditSettings *pset = &scene->toolsettings->particle;
			for (int a = 0; a < PE_TOT_BRUSH; a++) {
				if (pset->brush[a].strength > 1.0f) {
					pset->brush[a].strength *= 0.01f;
				}
			}
		}

		/* init grease pencil smooth level iterations */
		for (bGPdata *gpd = main->gpencil.first; gpd; gpd = gpd->id.next) {
			for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
				if (gpl->draw_smoothlvl == 0) {
					gpl->draw_smoothlvl = 1;
				}
			}
		}

		for (bScreen *screen = main->screen.first; screen; screen = screen->id.next) {
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
					/* Bug: Was possible to add preview region to sequencer view by using AZones. */
					if (sl->spacetype == SPACE_SEQ) {
						SpaceSeq *sseq = (SpaceSeq *)sl;
						if (sseq->view == SEQ_VIEW_SEQUENCE) {
							for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
								/* remove preview region for sequencer-only view! */
								if (ar->regiontype == RGN_TYPE_PREVIEW) {
									ar->flag |= RGN_FLAG_HIDDEN;
									ar->alignment = RGN_ALIGN_NONE;
									break;
								}
							}
						}
					}
					/* Remove old deprecated region from filebrowsers */
					else if (sl->spacetype == SPACE_FILE) {
						for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_CHANNELS) {
								/* Free old deprecated 'channel' region... */
								BKE_area_region_free(NULL, ar);
								BLI_freelinkN(regionbase, ar);
								break;
							}
						}
					}
				}
			}
		}

		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			CurvePaintSettings *cps = &scene->toolsettings->curve_paint_settings;
			if (cps->error_threshold == 0) {
				cps->curve_type = CU_BEZIER;
				cps->flag |= CURVE_PAINT_FLAG_CORNERS_DETECT;
				cps->error_threshold = 8;
				cps->radius_max = 1.0f;
				cps->corner_angle = DEG2RADF(70.0f);
			}
		}

		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			Sequence *seq;

			SEQ_BEGIN (scene->ed, seq)
			{
				if (seq->type == SEQ_TYPE_TEXT) {
					TextVars *data = seq->effectdata;
					if (data->color[3] == 0.0f) {
						copy_v4_fl(data->color, 1.0f);
						data->shadow_color[3] = 1.0f;
					}
				}
			}
			SEQ_END
		}

		/* Adding "Properties" region to DopeSheet */
		for (bScreen *screen = main->screen.first; screen; screen = screen->id.next) {
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				/* handle pushed-back space data first */
				for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_ACTION) {
						SpaceAction *saction = (SpaceAction *)sl;
						do_version_action_editor_properties_region(&saction->regionbase);
					}
				}
				
				/* active spacedata info must be handled too... */
				if (sa->spacetype == SPACE_ACTION) {
					do_version_action_editor_properties_region(&sa->regionbase);
				}
			}
		}
	}

	if (!MAIN_VERSION_ATLEAST(main, 277, 2)) {
		if (!DNA_struct_elem_find(fd->filesdna, "Bone", "float", "scaleIn")) {
			for (bArmature *arm = main->armature.first; arm; arm = arm->id.next) {
				do_version_bones_super_bbone(&arm->bonebase);
			}
		}
		if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "scaleIn")) {
			for (Object *ob = main->object.first; ob; ob = ob->id.next) {
				if (ob->pose) {
					for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
						/* see do_version_bones_super_bbone()... */
						pchan->scaleIn = 1.0f;
						pchan->scaleOut = 1.0f;
						
						/* also make sure some legacy (unused for over a decade) flags are unset,
						 * so that we can reuse them for stuff that matters now...
						 * (i.e. POSE_IK_MAT, (unknown/unused x 4), POSE_HAS_IK)
						 *
						 * These seem to have been runtime flags used by the IK solver, but that stuff
						 * should be able to be recalculated automatically anyway, so it should be fine.
						 */
						pchan->flag &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
					}
				}
			}
		}

		for (Camera *camera = main->camera.first; camera != NULL; camera = camera->id.next) {
			if (camera->stereo.pole_merge_angle_from == 0.0f &&
			    camera->stereo.pole_merge_angle_to == 0.0f)
			{
				camera->stereo.pole_merge_angle_from = DEG2RAD(60.0f);
				camera->stereo.pole_merge_angle_to = DEG2RAD(75.0f);
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "NormalEditModifierData", "float", "mix_limit")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_NormalEdit) {
						NormalEditModifierData *nemd = (NormalEditModifierData *)md;
						nemd->mix_limit = DEG2RADF(180.0f);
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "BooleanModifierData", "float", "double_threshold")) {
			Object *ob;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Boolean) {
						BooleanModifierData *bmd = (BooleanModifierData *)md;
						bmd->double_threshold = 1e-6f;
					}
				}
			}
		}
	}
}
