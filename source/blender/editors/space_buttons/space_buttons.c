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

/** \file blender/editors/space_buttons/space_buttons.c
 *  \ingroup spbuttons
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"

#include "GPU_glew.h"

#include "buttons_intern.h"  /* own include */

/* ******************** default callbacks for buttons space ***************** */

static SpaceLink *buttons_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
	ARegion *ar;
	SpaceButs *sbuts;

	sbuts = MEM_callocN(sizeof(SpaceButs), "initbuts");
	sbuts->spacetype = SPACE_BUTS;

	sbuts->mainb = sbuts->mainbuser = BCONTEXT_OBJECT;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for buts");

	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_TOP;

#if 0
	/* context region */
	ar = MEM_callocN(sizeof(ARegion), "context region for buts");
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype = RGN_TYPE_CHANNELS;
	ar->alignment = RGN_ALIGN_TOP;
#endif

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for buts");

	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *)sbuts;
}

/* not spacelink itself */
static void buttons_free(SpaceLink *sl)
{
	SpaceButs *sbuts = (SpaceButs *) sl;

	if (sbuts->path)
		MEM_freeN(sbuts->path);

	if (sbuts->texuser) {
		ButsContextTexture *ct = sbuts->texuser;
		BLI_freelistN(&ct->users);
		MEM_freeN(ct);
	}
}

/* spacetype; init callback */
static void buttons_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
}

static SpaceLink *buttons_duplicate(SpaceLink *sl)
{
	SpaceButs *sbutsn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */
	sbutsn->path = NULL;
	sbutsn->texuser = NULL;

	return (SpaceLink *)sbutsn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "Property Editor", SPACE_BUTS, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void buttons_main_region_layout_properties(const bContext *C, SpaceButs *sbuts, ARegion *ar)
{
	buttons_context_compute(C, sbuts);

	const char *contexts[2] = {NULL, NULL};

	switch (sbuts->mainb) {
		case BCONTEXT_SCENE:
			contexts[0] = "scene";
			break;
		case BCONTEXT_RENDER:
			contexts[0] = "render";
			break;
		case BCONTEXT_VIEW_LAYER:
			contexts[0] = "view_layer";
			break;
		case BCONTEXT_WORLD:
			contexts[0] = "world";
			break;
		case BCONTEXT_WORKSPACE:
			contexts[0] = "workspace";
			break;
		case BCONTEXT_OBJECT:
			contexts[0] = "object";
			break;
		case BCONTEXT_DATA:
			contexts[0] = "data";
			break;
		case BCONTEXT_MATERIAL:
			contexts[0] = "material";
			break;
		case BCONTEXT_TEXTURE:
			contexts[0] = "texture";
			break;
		case BCONTEXT_PARTICLE:
			contexts[0] = "particle";
			break;
		case BCONTEXT_PHYSICS:
			contexts[0] = "physics";
			break;
		case BCONTEXT_BONE:
			contexts[0] = "bone";
			break;
		case BCONTEXT_MODIFIER:
			contexts[0] = "modifier";
			break;
		case BCONTEXT_SHADERFX:
			contexts[0] = "shaderfx";
			break;
		case BCONTEXT_CONSTRAINT:
			contexts[0] = "constraint";
			break;
		case BCONTEXT_BONE_CONSTRAINT:
			contexts[0] = "bone_constraint";
			break;
		case BCONTEXT_TOOL:
			contexts[0] = "tool";
			break;
	}

	const bool vertical = true;
	ED_region_panels_layout_ex(C, ar, contexts, sbuts->mainb, vertical);
}

static void buttons_main_region_layout_tool(const bContext *C, ARegion *ar)
{
	const char *contexts[3] = {NULL};

	const WorkSpace *workspace = CTX_wm_workspace(C);
	const int mode = CTX_data_mode_enum(C);

	if (workspace->tools_space_type == SPACE_VIEW3D) {
		switch (mode) {
			case CTX_MODE_EDIT_MESH:
				ARRAY_SET_ITEMS(contexts, ".mesh_edit");
				break;
			case CTX_MODE_EDIT_CURVE:
				ARRAY_SET_ITEMS(contexts, ".curve_edit");
				break;
			case CTX_MODE_EDIT_SURFACE:
				ARRAY_SET_ITEMS(contexts, ".curve_edit");
				break;
			case CTX_MODE_EDIT_TEXT:
				ARRAY_SET_ITEMS(contexts, ".text_edit");
				break;
			case CTX_MODE_EDIT_ARMATURE:
				ARRAY_SET_ITEMS(contexts, ".armature_edit");
				break;
			case CTX_MODE_EDIT_METABALL:
				ARRAY_SET_ITEMS(contexts, ".mball_edit");
				break;
			case CTX_MODE_EDIT_LATTICE:
				ARRAY_SET_ITEMS(contexts, ".lattice_edit");
				break;
			case CTX_MODE_POSE:
				ARRAY_SET_ITEMS(contexts, ".posemode");
				break;
			case CTX_MODE_SCULPT:
				ARRAY_SET_ITEMS(contexts, ".paint_common", ".sculpt_mode");
				break;
			case CTX_MODE_PAINT_WEIGHT:
				ARRAY_SET_ITEMS(contexts, ".paint_common", ".weightpaint");
				break;
			case CTX_MODE_PAINT_VERTEX:
				ARRAY_SET_ITEMS(contexts, ".paint_common", ".vertexpaint");
				break;
			case CTX_MODE_PAINT_TEXTURE:
				ARRAY_SET_ITEMS(contexts, ".paint_common", ".imagepaint");
				break;
			case CTX_MODE_PARTICLE:
				ARRAY_SET_ITEMS(contexts, ".particlemode");
				break;
			case CTX_MODE_OBJECT:
				ARRAY_SET_ITEMS(contexts, ".objectmode");
				break;
			case CTX_MODE_GPENCIL_PAINT:
				ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
				break;
			case CTX_MODE_GPENCIL_SCULPT:
				ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
				break;
			case CTX_MODE_GPENCIL_WEIGHT:
				ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
				break;
		}
	}
	else if (workspace->tools_space_type == SPACE_IMAGE) {
		/* TODO */
	}

	/* for grease pencil we don't use tool system yet, so we need check outside
	 * workspace->tools_space_type because this value is not available
	 */
	switch (mode) {
		case CTX_MODE_GPENCIL_PAINT:
			ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
			break;
		case CTX_MODE_GPENCIL_SCULPT:
			ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
			break;
		case CTX_MODE_GPENCIL_WEIGHT:
			ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
			break;
		case CTX_MODE_GPENCIL_EDIT:
			ARRAY_SET_ITEMS(contexts, ".greasepencil_edit");
			break;
	}

	const bool vertical = true;
	ED_region_panels_layout_ex(C, ar, contexts, -1, vertical);
}

static void buttons_main_region_layout(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceButs *sbuts = CTX_wm_space_buts(C);

	if (sbuts->mainb == BCONTEXT_TOOL) {
		buttons_main_region_layout_tool(C, ar);
	}
	else {
		buttons_main_region_layout_properties(C, sbuts, ar);
	}

	sbuts->mainbo = sbuts->mainb;
}

static void buttons_main_region_listener(
        wmWindow *UNUSED(win), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn,
        const Scene *UNUSED(scene))
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (ELEM(wmn->data, ND_LAYER)) {
				ED_region_tag_redraw(ar);
			}
			break;
	}
}

static void buttons_operatortypes(void)
{
	WM_operatortype_append(BUTTONS_OT_context_menu);
	WM_operatortype_append(BUTTONS_OT_file_browse);
	WM_operatortype_append(BUTTONS_OT_directory_browse);
}

static void buttons_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Property Editor", SPACE_BUTS, 0);

	WM_keymap_add_item(keymap, "BUTTONS_OT_context_menu", RIGHTMOUSE, KM_PRESS, 0, 0);
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void buttons_header_region_draw(const bContext *C, ARegion *ar)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);

	/* Needed for RNA to get the good values! */
	buttons_context_compute(C, sbuts);

	ED_region_header(C, ar);
}

static void buttons_header_region_message_subscribe(
        const bContext *UNUSED(C),
        WorkSpace *UNUSED(workspace), Scene *UNUSED(scene),
        bScreen *UNUSED(screen), ScrArea *sa, ARegion *ar,
        struct wmMsgBus *mbus)
{
	SpaceButs *sbuts = sa->spacedata.first;
	wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
		.owner = ar,
		.user_data = ar,
		.notify = ED_region_do_msg_notify_tag_redraw,
	};

	/* Don't check for SpaceButs.mainb here, we may toggle between view-layers
	 * where one has no active object, so that available contexts changes. */
	WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);

	if (!ELEM(sbuts->mainb, BCONTEXT_RENDER, BCONTEXT_SCENE, BCONTEXT_WORLD)) {
		WM_msg_subscribe_rna_anon_prop(mbus, ViewLayer, name, &msg_sub_value_region_tag_redraw);
	}
}

/* draw a certain button set only if properties area is currently
 * showing that button set, to reduce unnecessary drawing. */
static void buttons_area_redraw(ScrArea *sa, short buttons)
{
	SpaceButs *sbuts = sa->spacedata.first;

	/* if the area's current button set is equal to the one to redraw */
	if (sbuts->mainb == buttons)
		ED_area_tag_redraw(sa);
}

/* reused! */
static void buttons_area_listener(
        wmWindow *UNUSED(win), ScrArea *sa, wmNotifier *wmn, Scene *UNUSED(scene))
{
	SpaceButs *sbuts = sa->spacedata.first;

	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_RENDER_OPTIONS:
					buttons_area_redraw(sa, BCONTEXT_RENDER);
					buttons_area_redraw(sa, BCONTEXT_VIEW_LAYER);
					break;
				case ND_WORLD:
					buttons_area_redraw(sa, BCONTEXT_WORLD);
					sbuts->preview = 1;
					break;
				case ND_FRAME:
					/* any buttons area can have animated properties so redraw all */
					ED_area_tag_redraw(sa);
					sbuts->preview = 1;
					break;
				case ND_OB_ACTIVE:
					ED_area_tag_redraw(sa);
					sbuts->preview = 1;
					break;
				case ND_KEYINGSET:
					buttons_area_redraw(sa, BCONTEXT_SCENE);
					break;
				case ND_RENDER_RESULT:
					break;
				case ND_MODE:
				case ND_LAYER:
				default:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_TRANSFORM:
					buttons_area_redraw(sa, BCONTEXT_OBJECT);
					buttons_area_redraw(sa, BCONTEXT_DATA); /* autotexpace flag */
					break;
				case ND_POSE:
					buttons_area_redraw(sa, BCONTEXT_DATA);
					break;
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
					buttons_area_redraw(sa, BCONTEXT_BONE);
					buttons_area_redraw(sa, BCONTEXT_BONE_CONSTRAINT);
					buttons_area_redraw(sa, BCONTEXT_DATA);
					break;
				case ND_MODIFIER:
					if (wmn->action == NA_RENAME)
						ED_area_tag_redraw(sa);
					else
						buttons_area_redraw(sa, BCONTEXT_MODIFIER);
					buttons_area_redraw(sa, BCONTEXT_PHYSICS);
					break;
				case ND_CONSTRAINT:
					buttons_area_redraw(sa, BCONTEXT_CONSTRAINT);
					buttons_area_redraw(sa, BCONTEXT_BONE_CONSTRAINT);
					break;
				case ND_PARTICLE:
					if (wmn->action == NA_EDITED)
						buttons_area_redraw(sa, BCONTEXT_PARTICLE);
					sbuts->preview = 1;
					break;
				case ND_DRAW:
					buttons_area_redraw(sa, BCONTEXT_OBJECT);
					buttons_area_redraw(sa, BCONTEXT_DATA);
					buttons_area_redraw(sa, BCONTEXT_PHYSICS);
					break;
				case ND_SHADING:
				case ND_SHADING_DRAW:
				case ND_SHADING_LINKS:
				case ND_SHADING_PREVIEW:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview = 1;
					break;
				default:
					/* Not all object RNA props have a ND_ notifier (yet) */
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_SELECT:
				case ND_DATA:
				case ND_VERTEX_GROUP:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MATERIAL:
			ED_area_tag_redraw(sa);
			switch (wmn->data) {
				case ND_SHADING:
				case ND_SHADING_DRAW:
				case ND_SHADING_LINKS:
				case ND_SHADING_PREVIEW:
				case ND_NODES:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview = 1;
					break;
			}
			break;
		case NC_WORLD:
			buttons_area_redraw(sa, BCONTEXT_WORLD);
			sbuts->preview = 1;
			break;
		case NC_LAMP:
			buttons_area_redraw(sa, BCONTEXT_DATA);
			sbuts->preview = 1;
			break;
		case NC_GROUP:
			buttons_area_redraw(sa, BCONTEXT_OBJECT);
			break;
		case NC_BRUSH:
			buttons_area_redraw(sa, BCONTEXT_TEXTURE);
			sbuts->preview = 1;
			break;
		case NC_TEXTURE:
		case NC_IMAGE:
			if (wmn->action != NA_PAINTING) {
				ED_area_tag_redraw(sa);
				sbuts->preview = 1;
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_PROPERTIES)
				ED_area_tag_redraw(sa);
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_area_tag_redraw(sa);
			break;
		case NC_ANIMATION:
			switch (wmn->data) {
				case ND_KEYFRAME:
					if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED))
						ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_GPENCIL:
			switch (wmn->data) {
				case ND_DATA:
					if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED))
						ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_NODE:
			if (wmn->action == NA_SELECTED) {
				ED_area_tag_redraw(sa);
				/* new active node, update texture preview */
				if (sbuts->mainb == BCONTEXT_TEXTURE)
					sbuts->preview = 1;
			}
			break;
		/* Listener for preview render, when doing an global undo. */
		case NC_WM:
			if (wmn->data == ND_UNDO) {
				ED_area_tag_redraw(sa);
				sbuts->preview = 1;
			}
			break;
#ifdef WITH_FREESTYLE
		case NC_LINESTYLE:
			ED_area_tag_redraw(sa);
			sbuts->preview = 1;
			break;
#endif
	}

	if (wmn->data == ND_KEYS)
		ED_area_tag_redraw(sa);
}

static void buttons_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
	SpaceButs *sbuts = (SpaceButs *)slink;

	if (sbuts->pinid == old_id) {
		sbuts->pinid = new_id;
		if (new_id == NULL) {
			sbuts->flag &= ~SB_PIN_CONTEXT;
		}
	}

	if (sbuts->path) {
		ButsContextPath *path = sbuts->path;
		int i;

		for (i = 0; i < path->len; i++) {
			if (path->ptr[i].id.data == old_id) {
				break;
			}
		}

		if (i == path->len) {
			/* pass */
		}
		else if (new_id == NULL) {
			if (i == 0) {
				MEM_SAFE_FREE(sbuts->path);
			}
			else {
				memset(&path->ptr[i], 0, sizeof(path->ptr[i]) * (path->len - i));
				path->len = i;
			}
		}
		else {
			RNA_id_pointer_create(new_id, &path->ptr[i]);
			/* There is no easy way to check/make path downwards valid, just nullify it.
			 * Next redraw will rebuild this anyway. */
			i++;
			memset(&path->ptr[i], 0, sizeof(path->ptr[i]) * (path->len - i));
			path->len = i;
		}
	}

	if (sbuts->texuser) {
		ButsContextTexture *ct = sbuts->texuser;
		if ((ID *)ct->texture == old_id) {
			ct->texture = (Tex *)new_id;
		}
		BLI_freelistN(&ct->users);
		ct->user = NULL;
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_buttons(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype buttons");
	ARegionType *art;

	st->spaceid = SPACE_BUTS;
	strncpy(st->name, "Buttons", BKE_ST_MAXNAME);

	st->new = buttons_new;
	st->free = buttons_free;
	st->init = buttons_init;
	st->duplicate = buttons_duplicate;
	st->operatortypes = buttons_operatortypes;
	st->keymap = buttons_keymap;
	st->listener = buttons_area_listener;
	st->context = buttons_context;
	st->id_remap = buttons_id_remap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = buttons_main_region_init;
	art->layout = buttons_main_region_layout;
	art->draw = ED_region_panels_draw;
	art->listener = buttons_main_region_listener;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	BLI_addhead(&st->regiontypes, art);

	buttons_context_register(art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

	art->init = buttons_header_region_init;
	art->draw = buttons_header_region_draw;
	art->message_subscribe = buttons_header_region_message_subscribe;
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}
