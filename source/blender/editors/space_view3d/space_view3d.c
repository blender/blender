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

/** \file blender/editors/space_view3d/space_view3d.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "GPU_extensions.h"
#include "GPU_material.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "RNA_access.h"

#include "UI_resources.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "view3d_intern.h"  /* own include */

/* ******************** manage regions ********************* */

ARegion *view3d_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar) return ar;
	
	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "buttons for view3d");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

ARegion *view3d_has_tools_region(ScrArea *sa)
{
	ARegion *ar, *artool = NULL, *arprops = NULL, *arhead;
	
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_TOOLS)
			artool = ar;
		if (ar->regiontype == RGN_TYPE_TOOL_PROPS)
			arprops = ar;
	}
	
	/* tool region hide/unhide also hides props */
	if (arprops && artool) return artool;
	
	if (artool == NULL) {
		/* add subdiv level; after header */
		for (arhead = sa->regionbase.first; arhead; arhead = arhead->next)
			if (arhead->regiontype == RGN_TYPE_HEADER)
				break;
		
		/* is error! */
		if (arhead == NULL) return NULL;
		
		artool = MEM_callocN(sizeof(ARegion), "tools for view3d");
		
		BLI_insertlinkafter(&sa->regionbase, arhead, artool);
		artool->regiontype = RGN_TYPE_TOOLS;
		artool->alignment = RGN_ALIGN_LEFT;
		artool->flag = RGN_FLAG_HIDDEN;
	}

	if (arprops == NULL) {
		/* add extra subdivided region for tool properties */
		arprops = MEM_callocN(sizeof(ARegion), "tool props for view3d");
		
		BLI_insertlinkafter(&sa->regionbase, artool, arprops);
		arprops->regiontype = RGN_TYPE_TOOL_PROPS;
		arprops->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
	}
	
	return artool;
}

/* ****************************************************** */

/* function to always find a regionview3d context inside 3D window */
RegionView3D *ED_view3d_context_rv3d(bContext *C)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	
	if (rv3d == NULL) {
		ScrArea *sa = CTX_wm_area(C);
		if (sa && sa->spacetype == SPACE_VIEW3D) {
			ARegion *ar = BKE_area_find_region_active_win(sa);
			if (ar) {
				rv3d = ar->regiondata;
			}
		}
	}
	return rv3d;
}

/* ideally would return an rv3d but in some cases the region is needed too
 * so return that, the caller can then access the ar->regiondata */
bool ED_view3d_context_user_region(bContext *C, View3D **r_v3d, ARegion **r_ar)
{
	ScrArea *sa = CTX_wm_area(C);

	*r_v3d = NULL;
	*r_ar = NULL;

	if (sa && sa->spacetype == SPACE_VIEW3D) {
		ARegion *ar = CTX_wm_region(C);
		View3D *v3d = (View3D *)sa->spacedata.first;

		if (ar) {
			RegionView3D *rv3d = ar->regiondata;
			if (rv3d && (rv3d->viewlock & RV3D_LOCKED) == 0) {
				*r_v3d = v3d;
				*r_ar = ar;
				return true;
			}
			else {
				ARegion *ar_unlock_user = NULL;
				ARegion *ar_unlock = NULL;
				for (ar = sa->regionbase.first; ar; ar = ar->next) {
					/* find the first unlocked rv3d */
					if (ar->regiondata && ar->regiontype == RGN_TYPE_WINDOW) {
						rv3d = ar->regiondata;
						if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
							ar_unlock = ar;
							if (rv3d->persp == RV3D_PERSP || rv3d->persp == RV3D_CAMOB) {
								ar_unlock_user = ar;
								break;
							}
						}
					}
				}

				/* camera/perspective view get priority when the active region is locked */
				if (ar_unlock_user) {
					*r_v3d = v3d;
					*r_ar = ar_unlock_user;
					return true;
				}

				if (ar_unlock) {
					*r_v3d = v3d;
					*r_ar = ar_unlock;
					return true;
				}
			}
		}
	}

	return false;
}

/* Most of the time this isn't needed since you could assume the view matrix was
 * set while drawing, however when functions like mesh_foreachScreenVert are
 * called by selection tools, we can't be sure this object was the last.
 *
 * for example, transparent objects are drawn after editmode and will cause
 * the rv3d mat's to change and break selection.
 *
 * 'ED_view3d_init_mats_rv3d' should be called before
 * view3d_project_short_clip and view3d_project_short_noclip in cases where
 * these functions are not used during draw_object
 */
void ED_view3d_init_mats_rv3d(struct Object *ob, struct RegionView3D *rv3d)
{
	/* local viewmat and persmat, to calculate projections */
	mul_m4_m4m4(rv3d->viewmatob, rv3d->viewmat, ob->obmat);
	mul_m4_m4m4(rv3d->persmatob, rv3d->persmat, ob->obmat);

	/* initializes object space clipping, speeds up clip tests */
	ED_view3d_clipping_local(rv3d, ob->obmat);
}

void ED_view3d_init_mats_rv3d_gl(struct Object *ob, struct RegionView3D *rv3d)
{
	ED_view3d_init_mats_rv3d(ob, rv3d);

	/* we have to multiply instead of loading viewmatob to make
	 * it work with duplis using displists, otherwise it will
	 * override the dupli-matrix */
	glMultMatrixf(ob->obmat);
}

#ifdef DEBUG
/* ensure we correctly initialize */
void ED_view3d_clear_mats_rv3d(struct RegionView3D *rv3d)
{
	zero_m4(rv3d->viewmatob);
	zero_m4(rv3d->persmatob);
}

void ED_view3d_check_mats_rv3d(struct RegionView3D *rv3d)
{
	BLI_ASSERT_ZERO_M4(rv3d->viewmatob);
	BLI_ASSERT_ZERO_M4(rv3d->persmatob);
}
#endif

static void view3d_stop_render_preview(wmWindowManager *wm, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->render_engine) {
#ifdef WITH_PYTHON
		BPy_BEGIN_ALLOW_THREADS;
#endif

		WM_jobs_kill_type(wm, ar, WM_JOB_TYPE_RENDER_PREVIEW);

#ifdef WITH_PYTHON
		BPy_END_ALLOW_THREADS;
#endif

		if (rv3d->render_engine->re)
			RE_Database_Free(rv3d->render_engine->re);
		RE_engine_free(rv3d->render_engine);
		rv3d->render_engine = NULL;
	}
}

void ED_view3d_shade_update(Main *bmain, View3D *v3d, ScrArea *sa)
{
	wmWindowManager *wm = bmain->wm.first;

	if (v3d->drawtype != OB_RENDER) {
		ARegion *ar;

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiondata)
				view3d_stop_render_preview(wm, ar);
		}
	}
}

/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *view3d_new(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar;
	View3D *v3d;
	RegionView3D *rv3d;
	
	v3d = MEM_callocN(sizeof(View3D), "initview3d");
	v3d->spacetype = SPACE_VIEW3D;
	v3d->blockscale = 0.7f;
	v3d->lay = v3d->layact = 1;
	if (scene) {
		v3d->lay = v3d->layact = scene->lay;
		v3d->camera = scene->camera;
	}
	v3d->scenelock = true;
	v3d->grid = 1.0f;
	v3d->gridlines = 16;
	v3d->gridsubdiv = 10;
	v3d->drawtype = OB_SOLID;

	v3d->gridflag = V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_FLOOR;
	
	v3d->flag = V3D_SELECT_OUTLINE;
	v3d->flag2 = V3D_SHOW_RECONSTRUCTION | V3D_SHOW_GPENCIL;
	
	v3d->lens = 35.0f;
	v3d->near = 0.01f;
	v3d->far = 1000.0f;

	v3d->twflag |= U.tw_flag & V3D_USE_MANIPULATOR;
	v3d->twtype = V3D_MANIP_TRANSLATE;
	v3d->around = V3D_CENTROID;
	
	v3d->bundle_size = 0.2f;
	v3d->bundle_drawtype = OB_PLAINAXES;
	
	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for view3d");
	
	BLI_addtail(&v3d->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* tool shelf */
	ar = MEM_callocN(sizeof(ARegion), "toolshelf for view3d");
	
	BLI_addtail(&v3d->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOLS;
	ar->alignment = RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* tool properties */
	ar = MEM_callocN(sizeof(ARegion), "tool properties for view3d");
	
	BLI_addtail(&v3d->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOL_PROPS;
	ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* buttons/list view */
	ar = MEM_callocN(sizeof(ARegion), "buttons for view3d");
	
	BLI_addtail(&v3d->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for view3d");
	
	BLI_addtail(&v3d->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	ar->regiondata = MEM_callocN(sizeof(RegionView3D), "region view3d");
	rv3d = ar->regiondata;
	rv3d->viewquat[0] = 1.0f;
	rv3d->persp = RV3D_PERSP;
	rv3d->view = RV3D_VIEW_PERSPORTHO;
	rv3d->dist = 10.0;
	
	return (SpaceLink *)v3d;
}

/* not spacelink itself */
static void view3d_free(SpaceLink *sl)
{
	View3D *vd = (View3D *) sl;
	BGpic *bgpic;

	for (bgpic = vd->bgpicbase.first; bgpic; bgpic = bgpic->next) {
		if (bgpic->source == V3D_BGPIC_IMAGE) {
			id_us_min((ID *)bgpic->ima);
		}
		else if (bgpic->source == V3D_BGPIC_MOVIE) {
			id_us_min((ID *)bgpic->clip);
		}
	}
	BLI_freelistN(&vd->bgpicbase);

	if (vd->localvd) MEM_freeN(vd->localvd);
	
	if (vd->properties_storage) MEM_freeN(vd->properties_storage);
	
	/* matcap material, its preview rect gets freed via icons */
	if (vd->defmaterial) {
		if (vd->defmaterial->gpumaterial.first)
			GPU_material_free(vd->defmaterial);
		BKE_previewimg_free(&vd->defmaterial->preview);
		MEM_freeN(vd->defmaterial);
	}
}


/* spacetype; init callback */
static void view3d_init(wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *view3d_duplicate(SpaceLink *sl)
{
	View3D *v3do = (View3D *)sl;
	View3D *v3dn = MEM_dupallocN(sl);
	BGpic *bgpic;
	
	/* clear or remove stuff from old */
	
// XXX	BIF_view3d_previewrender_free(v3do);
	
	if (v3dn->localvd) {
		v3dn->localvd = NULL;
		v3dn->properties_storage = NULL;
		v3dn->lay = v3do->localvd->lay & 0xFFFFFF;
	}

	if (v3dn->drawtype == OB_RENDER)
		v3dn->drawtype = OB_SOLID;
	
	/* copy or clear inside new stuff */

	v3dn->defmaterial = NULL;

	BLI_duplicatelist(&v3dn->bgpicbase, &v3do->bgpicbase);
	for (bgpic = v3dn->bgpicbase.first; bgpic; bgpic = bgpic->next) {
		if (bgpic->source == V3D_BGPIC_IMAGE) {
			id_us_plus((ID *)bgpic->ima);
		}
		else if (bgpic->source == V3D_BGPIC_MOVIE) {
			id_us_plus((ID *)bgpic->clip);
		}
	}

	v3dn->properties_storage = NULL;
	
	return (SpaceLink *)v3dn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *lb;
	wmKeyMap *keymap;

	/* object ops. */
	
	/* important to be before Pose keymap since they can both be enabled at once */
	keymap = WM_keymap_find(wm->defaultconf, "Face Mask", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	
	keymap = WM_keymap_find(wm->defaultconf, "Weight Paint Vertex Selection", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	/* pose is not modal, operator poll checks for this */
	keymap = WM_keymap_find(wm->defaultconf, "Pose", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "Object Mode", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Image Paint", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Vertex Paint", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Weight Paint", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Sculpt", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "Mesh", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "Curve", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "Armature", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Pose", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Metaball", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "Lattice", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Particle", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	/* editfont keymap swallows all... */
	keymap = WM_keymap_find(wm->defaultconf, "Font", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Object Non-modal", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Frames", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	/* own keymap, last so modes can override it */
	keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "3D View", SPACE_VIEW3D, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	/* add drop boxes */
	lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);
	
	WM_event_add_dropbox_handler(&ar->handlers, lb);
	
}

static void view3d_main_area_exit(wmWindowManager *wm, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;

	view3d_stop_render_preview(wm, ar);

	if (rv3d->gpuoffscreen) {
		GPU_offscreen_free(rv3d->gpuoffscreen);
		rv3d->gpuoffscreen = NULL;
	}
}

static int view3d_ob_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_OB)
			return 1;
	}
	return 0;
}

static int view3d_group_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_GR)
			return 1;
	}
	return 0;
}

static int view3d_mat_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_MA)
			return 1;
	}
	return 0;
}

static int view3d_ima_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_IM)
			return 1;
	}
	else if (drag->type == WM_DRAG_PATH) {
		if (ELEM(drag->icon, 0, ICON_FILE_IMAGE))   /* rule might not work? */
			return 1;
	}
	return 0;
}

static int view3d_ima_bg_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
	if (event->ctrl)
		return false;

	if (!ED_view3d_give_base_under_cursor(C, event->mval)) {
		return view3d_ima_drop_poll(C, drag, event);
	}
	return 0;
}

static int view3d_ima_empty_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
	Base *base = ED_view3d_give_base_under_cursor(C, event->mval);

	/* either holding and ctrl and no object, or dropping to empty */
	if (((base == NULL) && event->ctrl) ||
	    ((base != NULL) && base->object->type == OB_EMPTY))
	{
		return view3d_ima_drop_poll(C, drag, event);
	}

	return 0;
}

static int view3d_ima_mesh_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
	Base *base = ED_view3d_give_base_under_cursor(C, event->mval);

	if (base && base->object->type == OB_MESH)
		return view3d_ima_drop_poll(C, drag, event);
	return 0;
}

static void view3d_ob_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;

	RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_group_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;
	
	drop->opcontext = WM_OP_EXEC_DEFAULT;
	RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_id_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;
	
	RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_id_path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;
	
	if (id)
		RNA_string_set(drop->ptr, "name", id->name + 2);
	if (drag->path[0])
		RNA_string_set(drop->ptr, "filepath", drag->path);
}


/* region dropbox definition */
static void view3d_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);
	
	WM_dropbox_add(lb, "OBJECT_OT_add_named", view3d_ob_drop_poll, view3d_ob_drop_copy);
	WM_dropbox_add(lb, "OBJECT_OT_drop_named_material", view3d_mat_drop_poll, view3d_id_drop_copy);
	WM_dropbox_add(lb, "MESH_OT_drop_named_image", view3d_ima_mesh_drop_poll, view3d_id_path_drop_copy);
	WM_dropbox_add(lb, "OBJECT_OT_drop_named_image", view3d_ima_empty_drop_poll, view3d_id_path_drop_copy);
	WM_dropbox_add(lb, "VIEW3D_OT_background_image_add", view3d_ima_bg_drop_poll, view3d_id_path_drop_copy);
	WM_dropbox_add(lb, "OBJECT_OT_group_instance_add", view3d_group_drop_poll, view3d_group_drop_copy);
}



/* type callback, not region itself */
static void view3d_main_area_free(ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	
	if (rv3d) {
		if (rv3d->localvd) MEM_freeN(rv3d->localvd);
		if (rv3d->clipbb) MEM_freeN(rv3d->clipbb);

		if (rv3d->ri) {
			// XXX		BIF_view3d_previewrender_free(rv3d);
		}

		if (rv3d->render_engine)
			RE_engine_free(rv3d->render_engine);
		
		if (rv3d->depths) {
			if (rv3d->depths->depths) MEM_freeN(rv3d->depths->depths);
			MEM_freeN(rv3d->depths);
		}
		if (rv3d->sms) {
			MEM_freeN(rv3d->sms);
		}
		if (rv3d->gpuoffscreen) {
			GPU_offscreen_free(rv3d->gpuoffscreen);
		}

		MEM_freeN(rv3d);
		ar->regiondata = NULL;
	}
}

/* copy regiondata */
static void *view3d_main_area_duplicate(void *poin)
{
	if (poin) {
		RegionView3D *rv3d = poin, *new;
	
		new = MEM_dupallocN(rv3d);
		if (rv3d->localvd)
			new->localvd = MEM_dupallocN(rv3d->localvd);
		if (rv3d->clipbb)
			new->clipbb = MEM_dupallocN(rv3d->clipbb);
		
		new->depths = NULL;
		new->gpuoffscreen = NULL;
		new->ri = NULL;
		new->render_engine = NULL;
		new->sms = NULL;
		new->smooth_timer = NULL;
		
		return new;
	}
	return NULL;
}

static void view3d_recalc_used_layers(ARegion *ar, wmNotifier *wmn, Scene *scene)
{
	wmWindow *win = wmn->wm->winactive;
	ScrArea *sa;
	unsigned int lay_used = 0;
	Base *base;

	if (!win) return;

	base = scene->base.first;
	while (base) {
		lay_used |= base->lay & ((1 << 20) - 1); /* ignore localview */

		if (lay_used == (1 << 20) - 1)
			break;

		base = base->next;
	}

	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			if (BLI_findindex(&sa->regionbase, ar) != -1) {
				View3D *v3d = sa->spacedata.first;
				v3d->lay_used = lay_used;
				break;
			}
		}
	}
}

static void view3d_main_area_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	Scene *scene = sc->scene;
	View3D *v3d = sa->spacedata.first;
	
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			switch (wmn->data) {
				case ND_KEYFRAME_PROP:
				case ND_NLA_ACTCHANGE:
					ED_region_tag_redraw(ar);
					break;
				case ND_NLA:
				case ND_KEYFRAME:
					if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED))
						ED_region_tag_redraw(ar);
					break;
				case ND_ANIMCHAN:
					if (wmn->action == NA_SELECTED)
						ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_LAYER_CONTENT:
					if (wmn->reference)
						view3d_recalc_used_layers(ar, wmn, wmn->reference);
					ED_region_tag_redraw(ar);
					break;
				case ND_FRAME:
				case ND_TRANSFORM:
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
				case ND_OB_VISIBLE:
				case ND_LAYER:
				case ND_RENDER_OPTIONS:
				case ND_MARKERS:
				case ND_MODE:
					ED_region_tag_redraw(ar);
					break;
				case ND_WORLD:
					/* handled by space_view3d_listener() for v3d access */
					break;
			}
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_TRANSFORM:
				case ND_POSE:
				case ND_DRAW:
				case ND_MODIFIER:
				case ND_CONSTRAINT:
				case ND_KEYS:
				case ND_PARTICLE:
				case ND_LOD:
					ED_region_tag_redraw(ar);
					break;
			}
			switch (wmn->action) {
				case NA_ADDED:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_DATA:
				case ND_VERTEX_GROUP:
				case ND_SELECT:
					ED_region_tag_redraw(ar);
					break;
			}
			switch (wmn->action) {
				case NA_EDITED:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GROUP:
			/* all group ops for now */
			ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw_overlay(ar);
			break;
		case NC_MATERIAL:
			switch (wmn->data) {
				case ND_SHADING:
				case ND_NODES:
					if ((v3d->drawtype == OB_MATERIAL) ||
					    (v3d->drawtype == OB_TEXTURE &&
					         (scene->gm.matmode == GAME_MAT_GLSL ||
					          BKE_scene_use_new_shading_nodes(scene))))
					{
						ED_region_tag_redraw(ar);
					}
					break;
				case ND_SHADING_DRAW:
				case ND_SHADING_LINKS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_WORLD:
			switch (wmn->data) {
				case ND_WORLD_DRAW:
					/* handled by space_view3d_listener() for v3d access */
					break;
			}
			break;
		case NC_LAMP:
			switch (wmn->data) {
				case ND_LIGHTING:
					if ((v3d->drawtype == OB_MATERIAL) ||
					    (v3d->drawtype == OB_TEXTURE && (scene->gm.matmode == GAME_MAT_GLSL)))
					{
						ED_region_tag_redraw(ar);
					}
					break;
				case ND_LIGHTING_DRAW:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_IMAGE:
			/* this could be more fine grained checks if we had
			 * more context than just the region */
			ED_region_tag_redraw(ar);
			break;
		case NC_TEXTURE:
			/* same as above */
			ED_region_tag_redraw(ar);
			break;
		case NC_MOVIECLIP:
			if (wmn->data == ND_DISPLAY)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D) {
				if (wmn->subtype == NS_VIEW3D_GPU) {
					RegionView3D *rv3d = ar->regiondata;
					rv3d->rflag |= RV3D_GPULIGHT_UPDATE;
				}
				ED_region_tag_redraw(ar);
			}
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCREEN:
			switch (wmn->data) {
				case ND_ANIMPLAY:
				case ND_SKETCH:
					ED_region_tag_redraw(ar);
					break;
				case ND_SCREENBROWSE:
				case ND_SCREENDELETE:
				case ND_SCREENSET:
					/* screen was changed, need to update used layers due to NC_SCENE|ND_LAYER_CONTENT */
					/* updates used layers only for View3D in active screen */
					if (wmn->reference) {
						bScreen *sc_ref = wmn->reference;
						view3d_recalc_used_layers(ar, wmn, sc_ref->scene);
					}
					ED_region_tag_redraw(ar);
					break;
			}

			break;
		case NC_GPENCIL:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* concept is to retrieve cursor type context-less */
static void view3d_main_area_cursor(wmWindow *win, ScrArea *UNUSED(sa), ARegion *UNUSED(ar))
{
	Scene *scene = win->screen->scene;

	if (scene->obedit) {
		WM_cursor_set(win, CURSOR_EDIT);
	}
	else {
		WM_cursor_set(win, CURSOR_STD);
	}
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	ED_region_header_init(ar);
}

static void view3d_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void view3d_header_area_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
				case ND_OB_VISIBLE:
				case ND_MODE:
				case ND_LAYER:
				case ND_TOOLSETTINGS:
				case ND_LAYER_CONTENT:
				case ND_RENDER_OPTIONS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void view3d_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void view3d_buttons_area_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			switch (wmn->data) {
				case ND_KEYFRAME_PROP:
				case ND_NLA_ACTCHANGE:
					ED_region_tag_redraw(ar);
					break;
				case ND_NLA:
				case ND_KEYFRAME:
					if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED))
						ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
				case ND_OB_VISIBLE:
				case ND_MODE:
				case ND_LAYER:
				case ND_LAYER_CONTENT:
				case ND_TOOLSETTINGS:
					ED_region_tag_redraw(ar);
					break;
			}
			switch (wmn->action) {
				case NA_EDITED:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_TRANSFORM:
				case ND_POSE:
				case ND_DRAW:
				case ND_KEYS:
				case ND_MODIFIER:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_DATA:
				case ND_VERTEX_GROUP:
				case ND_SELECT:
					ED_region_tag_redraw(ar);
					break;
			}
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
		case NC_TEXTURE:
		case NC_MATERIAL:
			/* for brush textures */
			ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		case NC_GPENCIL:
			if (wmn->data == ND_DATA || wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_tools_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void view3d_tools_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, CTX_data_mode_string(C), -1);
}

static void view3d_props_area_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_WM:
			if (wmn->data == ND_HISTORY)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if (wmn->data == ND_MODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D)
				ED_region_tag_redraw(ar);
			break;
	}
}

/*area (not region) level listener*/
static void space_view3d_listener(bScreen *UNUSED(sc), ScrArea *sa, struct wmNotifier *wmn)
{
	View3D *v3d = sa->spacedata.first;

	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_WORLD:
					if (v3d->flag2 & V3D_RENDER_OVERRIDE)
						ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
					break;
			}
			break;
		case NC_WORLD:
			switch (wmn->data) {
				case ND_WORLD_DRAW:
					if (v3d->flag2 & V3D_RENDER_OVERRIDE)
						ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
					break;
			}
			break;
		case NC_MATERIAL:
			switch (wmn->data) {
				case ND_NODES:
					if (v3d->drawtype == OB_TEXTURE)
						ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
					break;
			}
			break;
	}

	/* removed since BKE_image_user_frame_calc is now called in view3d_draw_bgpic because screen_ops doesnt call the notifier. */
#if 0
	if (wmn->category == NC_SCENE && wmn->data == ND_FRAME) {
		View3D *v3d = area->spacedata.first;
		BGpic *bgpic = v3d->bgpicbase.first;

		for (; bgpic; bgpic = bgpic->next) {
			if (bgpic->ima) {
				Scene *scene = wmn->reference;
				BKE_image_user_frame_calc(&bgpic->iuser, scene->r.cfra, 0);
			}
		}
	}
#endif
}

const char *view3d_context_dir[] = {
	"selected_objects", "selected_bases", "selected_editable_objects",
	"selected_editable_bases", "visible_objects", "visible_bases", "selectable_objects", "selectable_bases",
	"active_base", "active_object", NULL
};

static int view3d_context(const bContext *C, const char *member, bContextDataResult *result)
{
	/* fallback to the scene layer, allows duplicate and other object operators to run outside the 3d view */

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, view3d_context_dir);
	}
	else if (CTX_data_equals(member, "selected_objects") || CTX_data_equals(member, "selected_bases")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		Base *base;
		const bool selected_objects = CTX_data_equals(member, "selected_objects");

		for (base = scene->base.first; base; base = base->next) {
			if ((base->flag & SELECT) && (base->lay & lay)) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0) {
					if (selected_objects)
						CTX_data_id_list_add(result, &base->object->id);
					else
						CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "selected_editable_objects") || CTX_data_equals(member, "selected_editable_bases")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		Base *base;
		const bool selected_editable_objects = CTX_data_equals(member, "selected_editable_objects");

		for (base = scene->base.first; base; base = base->next) {
			if ((base->flag & SELECT) && (base->lay & lay)) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0) {
					if (0 == BKE_object_is_libdata(base->object)) {
						if (selected_editable_objects)
							CTX_data_id_list_add(result, &base->object->id);
						else
							CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
					}
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "visible_objects") || CTX_data_equals(member, "visible_bases")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		Base *base;
		const bool visible_objects = CTX_data_equals(member, "visible_objects");

		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & lay) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0) {
					if (visible_objects)
						CTX_data_id_list_add(result, &base->object->id);
					else
						CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "selectable_objects") || CTX_data_equals(member, "selectable_bases")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		Base *base;
		const bool selectable_objects = CTX_data_equals(member, "selectable_objects");

		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & lay) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0 && (base->object->restrictflag & OB_RESTRICT_SELECT) == 0) {
					if (selectable_objects)
						CTX_data_id_list_add(result, &base->object->id);
					else
						CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "active_base")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		if (scene->basact && (scene->basact->lay & lay)) {
			Object *ob = scene->basact->object;
			/* if hidden but in edit mode, we still display, can happen with animation */
			if ((ob->restrictflag & OB_RESTRICT_VIEW) == 0 || (ob->mode & OB_MODE_EDIT))
				CTX_data_pointer_set(result, &scene->id, &RNA_ObjectBase, scene->basact);
		}
		
		return 1;
	}
	else if (CTX_data_equals(member, "active_object")) {
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		const unsigned int lay = v3d ? v3d->lay : scene->lay;
		if (scene->basact && (scene->basact->lay & lay)) {
			Object *ob = scene->basact->object;
			if ((ob->restrictflag & OB_RESTRICT_VIEW) == 0 || (ob->mode & OB_MODE_EDIT))
				CTX_data_id_pointer_set(result, &scene->basact->object->id);
		}
		
		return 1;
	}
	else {
		return 0; /* not found */
	}

	return -1; /* found but not available */
}


/* only called once, from space/spacetypes.c */
void ED_spacetype_view3d(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype view3d");
	ARegionType *art;
	
	st->spaceid = SPACE_VIEW3D;
	strncpy(st->name, "View3D", BKE_ST_MAXNAME);
	
	st->new = view3d_new;
	st->free = view3d_free;
	st->init = view3d_init;
	st->listener = space_view3d_listener;
	st->duplicate = view3d_duplicate;
	st->operatortypes = view3d_operatortypes;
	st->keymap = view3d_keymap;
	st->dropboxes = view3d_dropboxes;
	st->context = view3d_context;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d main region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_GPENCIL;
	art->draw = view3d_main_area_draw;
	art->init = view3d_main_area_init;
	art->exit = view3d_main_area_exit;
	art->free = view3d_main_area_free;
	art->duplicate = view3d_main_area_duplicate;
	art->listener = view3d_main_area_listener;
	art->cursor = view3d_main_area_cursor;
	art->lock = 1;   /* can become flag, see BKE_spacedata_draw_locks */
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d buttons region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 180; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = view3d_buttons_area_listener;
	art->init = view3d_buttons_area_init;
	art->draw = view3d_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);

	view3d_buttons_register(art);

	/* regions: tool(bar) */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tools region");
	art->regionid = RGN_TYPE_TOOLS;
	art->prefsizex = 160; /* XXX */
	art->prefsizey = 50; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = view3d_buttons_area_listener;
	art->init = view3d_tools_area_init;
	art->draw = view3d_tools_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
#if 0
	/* unfinished still */
	view3d_toolshelf_register(art);
#endif

	/* regions: tool properties */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tool properties region");
	art->regionid = RGN_TYPE_TOOL_PROPS;
	art->prefsizex = 0;
	art->prefsizey = 120;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = view3d_props_area_listener;
	art->init = view3d_tools_area_init;
	art->draw = view3d_tools_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
	view3d_tool_props_register(art);
	
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d header region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = view3d_header_area_listener;
	art->init = view3d_header_area_init;
	art->draw = view3d_header_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

