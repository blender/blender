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
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_250.c
 *  \ingroup blenloader
 */

#include "zlib.h"

#ifndef WIN32
#  include <unistd.h> // for read close
#else
#  include <io.h> // for open close read
#  include "winsock2.h"
#  include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_actuator_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim.h" // NT
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_smoke_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_colortools.h"
#include "BKE_global.h" // for G
#include "BKE_library.h" // for which_libbase
#include "BKE_main.h" // for Main
#include "BKE_mesh.h" // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h" // SWITCH_INT DATA ENDB DNA1 O_BINARY GLOB USER TEST REND
#include "BKE_sound.h"

#include "NOD_socket.h"

//XXX #include "BIF_butspace.h" // badlevel, for do_versions, patching event codes
//XXX #include "BIF_filelist.h" // badlevel too, where to move this? - elubie
//XXX #include "BIF_previewrender.h" // bedlelvel, for struct RenderInfo
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "RE_engine.h"

#include "readfile.h"

#include "PIL_time.h"

#include <errno.h>

/* 2.50 patch */
static void area_add_header_region(ScrArea *sa, ListBase *lb)
{
	ARegion *ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");

	BLI_addtail(lb, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	if (sa->headertype == HEADERDOWN)
		ar->alignment = RGN_ALIGN_BOTTOM;
	else
		ar->alignment = RGN_ALIGN_TOP;

	/* initialize view2d data for header region, to allow panning */
	/* is copy from ui_view2d.c */
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
	ar->v2d.keepofs = V2D_LOCKOFS_Y;
	ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
	ar->v2d.align = V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y;
	ar->v2d.flag = (V2D_PIXELOFS_X|V2D_PIXELOFS_Y);
}

static void sequencer_init_preview_region(ARegion* ar)
{
	// XXX a bit ugly still, copied from space_sequencer
	/* NOTE: if you change values here, also change them in space_sequencer.c, sequencer_new */
	ar->regiontype = RGN_TYPE_PREVIEW;
	ar->alignment = RGN_ALIGN_TOP;
	ar->flag |= RGN_FLAG_HIDDEN;
	ar->v2d.keepzoom = V2D_KEEPASPECT | V2D_KEEPZOOM;
	ar->v2d.minzoom = 0.00001f;
	ar->v2d.maxzoom = 100000.0f;
	ar->v2d.tot.xmin = -960.0f; /* 1920 width centered */
	ar->v2d.tot.ymin = -540.0f; /* 1080 height centered */
	ar->v2d.tot.xmax = 960.0f;
	ar->v2d.tot.ymax = 540.0f;
	ar->v2d.min[0] = 0.0f;
	ar->v2d.min[1] = 0.0f;
	ar->v2d.max[0] = 12000.0f;
	ar->v2d.max[1] = 12000.0f;
	ar->v2d.cur = ar->v2d.tot;
	ar->v2d.align = V2D_ALIGN_FREE; // (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y);
	ar->v2d.keeptot = V2D_KEEPTOT_FREE;
}

static void area_add_window_regions(ScrArea *sa, SpaceLink *sl, ListBase *lb)
{
	ARegion *ar;
	ARegion *ar_main;

	if (sl) {
		/* first channels for ipo action nla... */
		switch (sl->spacetype) {
			case SPACE_IPO:
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_CHANNELS;
				ar->alignment = RGN_ALIGN_LEFT;
				ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);

				/* for some reason, this doesn't seem to go auto like for NLA... */
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_RIGHT;
				ar->v2d.scroll = V2D_SCROLL_RIGHT;
				ar->v2d.flag = RGN_FLAG_HIDDEN;
				break;

			case SPACE_ACTION:
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_CHANNELS;
				ar->alignment = RGN_ALIGN_LEFT;
				ar->v2d.scroll = V2D_SCROLL_BOTTOM;
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				break;

			case SPACE_NLA:
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_CHANNELS;
				ar->alignment = RGN_ALIGN_LEFT;
				ar->v2d.scroll = V2D_SCROLL_BOTTOM;
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

				/* for some reason, some files still don't get this auto */
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_RIGHT;
				ar->v2d.scroll = V2D_SCROLL_RIGHT;
				ar->v2d.flag = RGN_FLAG_HIDDEN;
				break;

			case SPACE_NODE:
				ar = MEM_callocN(sizeof(ARegion), "nodetree area for node");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_LEFT;
				ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				/* temporarily hide it */
				ar->flag = RGN_FLAG_HIDDEN;
				break;
			case SPACE_FILE:
				ar = MEM_callocN(sizeof(ARegion), "nodetree area for node");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_CHANNELS;
				ar->alignment = RGN_ALIGN_LEFT;

				ar = MEM_callocN(sizeof(ARegion), "ui area for file");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_TOP;
				break;
			case SPACE_SEQ:
				ar_main = (ARegion*) lb->first;
				for (; ar_main; ar_main = ar_main->next) {
					if (ar_main->regiontype == RGN_TYPE_WINDOW)
						break;
				}
				ar = MEM_callocN(sizeof(ARegion), "preview area for sequencer");
				BLI_insertlinkbefore(lb, ar_main, ar);
				sequencer_init_preview_region(ar);
				break;
			case SPACE_VIEW3D:
				/* toolbar */
				ar = MEM_callocN(sizeof(ARegion), "toolbar for view3d");

				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_TOOLS;
				ar->alignment = RGN_ALIGN_LEFT;
				ar->flag = RGN_FLAG_HIDDEN;

				/* tool properties */
				ar = MEM_callocN(sizeof(ARegion), "tool properties for view3d");

				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_TOOL_PROPS;
				ar->alignment = RGN_ALIGN_BOTTOM|RGN_SPLIT_PREV;
				ar->flag = RGN_FLAG_HIDDEN;

				/* buttons/list view */
				ar = MEM_callocN(sizeof(ARegion), "buttons for view3d");

				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_RIGHT;
				ar->flag = RGN_FLAG_HIDDEN;
#if 0
			case SPACE_BUTS:
				/* context UI region */
				ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype = RGN_TYPE_UI;
				ar->alignment = RGN_ALIGN_RIGHT;

				break;
#endif
		}
	}

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");

	BLI_addtail(lb, ar);
	ar->winrct = sa->totrct;

	ar->regiontype = RGN_TYPE_WINDOW;

	if (sl) {
		/* if active spacetype has view2d data, copy that over to main region */
		/* and we split view3d */
		switch (sl->spacetype) {
			case SPACE_VIEW3D:
				blo_do_versions_view3d_split_250((View3D *)sl, lb);
				break;

			case SPACE_OUTLINER:
				{
					SpaceOops *soops = (SpaceOops *)sl;

					memcpy(&ar->v2d, &soops->v2d, sizeof(View2D));

					ar->v2d.scroll &= ~V2D_SCROLL_LEFT;
					ar->v2d.scroll |= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM_O);
					ar->v2d.align = (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_POS_Y);
					ar->v2d.keepzoom |= (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_KEEPASPECT);
					ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
					ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;
					//ar->v2d.flag |= V2D_IS_INITIALISED;
				}
				break;
			case SPACE_TIME:
				{
					SpaceTime *stime = (SpaceTime *)sl;
					memcpy(&ar->v2d, &stime->v2d, sizeof(View2D));

					ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
					ar->v2d.align |= V2D_ALIGN_NO_NEG_Y;
					ar->v2d.keepofs |= V2D_LOCKOFS_Y;
					ar->v2d.keepzoom |= V2D_LOCKZOOM_Y;
					ar->v2d.tot.ymin = ar->v2d.cur.ymin = -10.0;
					ar->v2d.min[1] = ar->v2d.max[1] = 20.0;
				}
				break;
			case SPACE_IPO:
				{
					SpaceIpo *sipo = (SpaceIpo *)sl;
					memcpy(&ar->v2d, &sipo->v2d, sizeof(View2D));

					/* init mainarea view2d */
					ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
					ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);

					ar->v2d.min[0] = FLT_MIN;
					ar->v2d.min[1] = FLT_MIN;

					ar->v2d.max[0] = MAXFRAMEF;
					ar->v2d.max[1] = FLT_MAX;

					//ar->v2d.flag |= V2D_IS_INITIALISED;
					break;
				}
			case SPACE_NLA:
				{
					SpaceNla *snla = (SpaceNla *)sl;
					memcpy(&ar->v2d, &snla->v2d, sizeof(View2D));

					ar->v2d.tot.ymin = (float)(-sa->winy)/3.0f;
					ar->v2d.tot.ymax = 0.0f;

					ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
					ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
					ar->v2d.align = V2D_ALIGN_NO_POS_Y;
					ar->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
					break;
				}
			case SPACE_ACTION:
				{
					SpaceAction *saction = (SpaceAction *) sl;

					/* we totally reinit the view for the Action Editor, as some old instances had some weird cruft set */
					ar->v2d.tot.xmin = -20.0f;
					ar->v2d.tot.ymin = (float)(-sa->winy)/3.0f;
					ar->v2d.tot.xmax = (float)((sa->winx > 120)? (sa->winx) : 120);
					ar->v2d.tot.ymax = 0.0f;

					ar->v2d.cur = ar->v2d.tot;

					ar->v2d.min[0] = 0.0f;
					ar->v2d.min[1] = 0.0f;

					ar->v2d.max[0] = MAXFRAMEF;
					ar->v2d.max[1] = FLT_MAX;

					ar->v2d.minzoom = 0.01f;
					ar->v2d.maxzoom = 50;
					ar->v2d.scroll = (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
					ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
					ar->v2d.keepzoom = V2D_LOCKZOOM_Y;
					ar->v2d.align = V2D_ALIGN_NO_POS_Y;
					ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

					/* for old files with ShapeKey editors open + an action set, clear the action as
					 * it doesn't make sense in the new system (i.e. violates concept that ShapeKey edit
					 * only shows ShapeKey-rooted actions only)
					 */
					if (saction->mode == SACTCONT_SHAPEKEY)
						saction->action = NULL;
					break;
				}
			case SPACE_SEQ:
				{
					SpaceSeq *sseq = (SpaceSeq *)sl;
					memcpy(&ar->v2d, &sseq->v2d, sizeof(View2D));

					ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
					ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
					ar->v2d.align = V2D_ALIGN_NO_NEG_Y;
					ar->v2d.flag |= V2D_IS_INITIALISED;
					break;
				}
			case SPACE_NODE:
				{
					SpaceNode *snode = (SpaceNode *)sl;
					memcpy(&ar->v2d, &snode->v2d, sizeof(View2D));

					ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
					ar->v2d.keepzoom = V2D_LIMITZOOM|V2D_KEEPASPECT;
					break;
				}
			case SPACE_BUTS:
				{
					SpaceButs *sbuts = (SpaceButs *)sl;
					memcpy(&ar->v2d, &sbuts->v2d, sizeof(View2D));

					ar->v2d.scroll |= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
					break;
				}
			case SPACE_FILE:
				{
					// SpaceFile *sfile = (SpaceFile *)sl;
					ar->v2d.tot.xmin = ar->v2d.tot.ymin = 0;
					ar->v2d.tot.xmax = ar->winx;
					ar->v2d.tot.ymax = ar->winy;
					ar->v2d.cur = ar->v2d.tot;
					ar->regiontype = RGN_TYPE_WINDOW;
					ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM_O);
					ar->v2d.align = (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_POS_Y);
					ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
					break;
				}
			case SPACE_TEXT:
				{
					SpaceText *st = (SpaceText *)sl;
					st->flags |= ST_FIND_WRAP;
				}
				//case SPACE_XXX: // FIXME... add other ones
				//	memcpy(&ar->v2d, &((SpaceXxx *)sl)->v2d, sizeof(View2D));
				//	break;
		}
	}
}

static void do_versions_windowmanager_2_50(bScreen *screen)
{
	ScrArea *sa;
	SpaceLink *sl;

	/* add regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		/* we keep headertype variable to convert old files only */
		if (sa->headertype)
			area_add_header_region(sa, &sa->regionbase);

		area_add_window_regions(sa, sa->spacedata.first, &sa->regionbase);

		/* space imageselect is deprecated */
		for (sl = sa->spacedata.first; sl; sl = sl->next) {
			if (sl->spacetype == SPACE_IMASEL)
				sl->spacetype = SPACE_EMPTY;	/* spacedata then matches */
		}

		/* space sound is deprecated */
		for (sl = sa->spacedata.first; sl; sl = sl->next) {
			if (sl->spacetype == SPACE_SOUND)
				sl->spacetype = SPACE_EMPTY;	/* spacedata then matches */
		}

		/* it seems to be possible in 2.5 to have this saved, filewindow probably */
		sa->butspacetype = sa->spacetype;

		/* pushed back spaces also need regions! */
		if (sa->spacedata.first) {
			sl = sa->spacedata.first;
			for (sl = sl->next; sl; sl = sl->next) {
				if (sa->headertype)
					area_add_header_region(sa, &sl->regionbase);
				area_add_window_regions(sa, sl, &sl->regionbase);
			}
		}
	}
}

static void versions_gpencil_add_main(ListBase *lb, ID *id, const char *name)
{
	BLI_addtail(lb, id);
	id->us = 1;
	id->flag = LIB_FAKEUSER;
	*( (short *)id->name )= ID_GD;

	new_id(lb, id, name);
	/* alphabetic insterion: is in new_id */

	if (G.debug & G_DEBUG)
		printf("Converted GPencil to ID: %s\n", id->name + 2);
}

static void do_versions_gpencil_2_50(Main *main, bScreen *screen)
{
	ScrArea *sa;
	SpaceLink *sl;

	/* add regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		for (sl = sa->spacedata.first; sl; sl = sl->next) {
			if (sl->spacetype == SPACE_VIEW3D) {
				View3D *v3d = (View3D*) sl;
				if (v3d->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)v3d->gpd, "GPencil View3D");
					v3d->gpd = NULL;
				}
			}
			else if (sl->spacetype == SPACE_NODE) {
				SpaceNode *snode = (SpaceNode *) sl;
				if (snode->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)snode->gpd, "GPencil Node");
					snode->gpd = NULL;
				}
			}
			else if (sl->spacetype == SPACE_SEQ) {
				SpaceSeq *sseq = (SpaceSeq *) sl;
				if (sseq->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)sseq->gpd, "GPencil Node");
					sseq->gpd = NULL;
				}
			}
			else if (sl->spacetype == SPACE_IMAGE) {
				SpaceImage *sima = (SpaceImage *) sl;
#if 0			/* see comment on r28002 */
				if (sima->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)sima->gpd, "GPencil Image");
					sima->gpd = NULL;
				}
#else
				sima->gpd = NULL;
#endif
			}
		}
	}
}

static void do_version_mtex_factor_2_50(MTex **mtex_array, short idtype)
{
	MTex *mtex;
	float varfac, colfac;
	int a, neg;

	if (!mtex_array)
		return;

	for (a = 0; a < MAX_MTEX; a++) {
		if (mtex_array[a]) {
			mtex = mtex_array[a];

			neg = mtex->maptoneg;
			varfac = mtex->varfac;
			colfac = mtex->colfac;

			if (neg & MAP_DISP) mtex->dispfac = -mtex->dispfac;
			if (neg & MAP_NORM) mtex->norfac = -mtex->norfac;
			if (neg & MAP_WARP) mtex->warpfac = -mtex->warpfac;

			mtex->colspecfac = (neg & MAP_COLSPEC)? -colfac: colfac;
			mtex->mirrfac = (neg & MAP_COLMIR)? -colfac: colfac;
			mtex->alphafac = (neg & MAP_ALPHA)? -varfac: varfac;
			mtex->difffac = (neg & MAP_REF)? -varfac: varfac;
			mtex->specfac = (neg & MAP_SPEC)? -varfac: varfac;
			mtex->emitfac = (neg & MAP_EMIT)? -varfac: varfac;
			mtex->hardfac = (neg & MAP_HAR)? -varfac: varfac;
			mtex->raymirrfac = (neg & MAP_RAYMIRR)? -varfac: varfac;
			mtex->translfac = (neg & MAP_TRANSLU)? -varfac: varfac;
			mtex->ambfac = (neg & MAP_AMB)? -varfac: varfac;
			mtex->colemitfac = (neg & MAP_EMISSION_COL)? -colfac: colfac;
			mtex->colreflfac = (neg & MAP_REFLECTION_COL)? -colfac: colfac;
			mtex->coltransfac = (neg & MAP_TRANSMISSION_COL)? -colfac: colfac;
			mtex->densfac = (neg & MAP_DENSITY)? -varfac: varfac;
			mtex->scatterfac = (neg & MAP_SCATTERING)? -varfac: varfac;
			mtex->reflfac = (neg & MAP_REFLECTION)? -varfac: varfac;

			mtex->timefac = (neg & MAP_PA_TIME)? -varfac: varfac;
			mtex->lengthfac = (neg & MAP_PA_LENGTH)? -varfac: varfac;
			mtex->clumpfac = (neg & MAP_PA_CLUMP)? -varfac: varfac;
			mtex->kinkfac = (neg & MAP_PA_KINK)? -varfac: varfac;
			mtex->roughfac = (neg & MAP_PA_ROUGH)? -varfac: varfac;
			mtex->padensfac = (neg & MAP_PA_DENS)? -varfac: varfac;
			mtex->lifefac = (neg & MAP_PA_LIFE)? -varfac: varfac;
			mtex->sizefac = (neg & MAP_PA_SIZE)? -varfac: varfac;
			mtex->ivelfac = (neg & MAP_PA_IVEL)? -varfac: varfac;

			mtex->shadowfac = (neg & LAMAP_SHAD)? -colfac: colfac;

			mtex->zenupfac = (neg & WOMAP_ZENUP)? -colfac: colfac;
			mtex->zendownfac = (neg & WOMAP_ZENDOWN)? -colfac: colfac;
			mtex->blendfac = (neg & WOMAP_BLEND)? -varfac: varfac;

			if (idtype == ID_MA)
				mtex->colfac = (neg & MAP_COL)? -colfac: colfac;
			else if (idtype == ID_LA)
				mtex->colfac = (neg & LAMAP_COL)? -colfac: colfac;
			else if (idtype == ID_WO)
				mtex->colfac = (neg & WOMAP_HORIZ)? -colfac: colfac;
		}
	}
}

static void do_version_mdef_250(Main *main)
{
	Object *ob;
	ModifierData *md;
	MeshDeformModifierData *mmd;

	for (ob = main->object.first; ob; ob = ob->id.next) {
		for (md = ob->modifiers.first; md; md = md->next) {
			if (md->type == eModifierType_MeshDeform) {
				mmd = (MeshDeformModifierData*) md;

				if (mmd->bindcos) {
					/* make bindcos NULL in order to trick older versions
					 * into thinking that the mesh was not bound yet */
					mmd->bindcagecos = mmd->bindcos;
					mmd->bindcos = NULL;

					modifier_mdef_compact_influences(md);
				}
			}
		}
	}
}

static void do_version_constraints_radians_degrees_250(ListBase *lb)
{
	bConstraint *con;

	for (con = lb->first; con; con = con->next) {
		if (con->type == CONSTRAINT_TYPE_RIGIDBODYJOINT) {
			bRigidBodyJointConstraint *data = con->data;
			data->axX *= (float)(M_PI / 180.0);
			data->axY *= (float)(M_PI / 180.0);
			data->axZ *= (float)(M_PI / 180.0);
		}
		else if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data = con->data;
			data->poleangle *= (float)(M_PI / 180.0);
		}
		else if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
			bRotLimitConstraint *data = con->data;

			data->xmin *= (float)(M_PI / 180.0);
			data->xmax *= (float)(M_PI / 180.0);
			data->ymin *= (float)(M_PI / 180.0);
			data->ymax *= (float)(M_PI / 180.0);
			data->zmin *= (float)(M_PI / 180.0);
			data->zmax *= (float)(M_PI / 180.0);
		}
	}
}

/* NOTE: this version patch is intended for versions < 2.52.2, but was initially introduced in 2.27 already */
static void do_versions_seq_unique_name_all_strips(Scene * sce, ListBase *seqbasep)
{
	Sequence * seq = seqbasep->first;

	while (seq) {
		seqbase_unique_name_recursive(&sce->ed->seqbase, seq);
		if (seq->seqbase.first) {
			do_versions_seq_unique_name_all_strips(sce, &seq->seqbase);
		}
		seq = seq->next;
	}
}

static void do_version_bone_roll_256(Bone *bone)
{
	Bone *child;
	float submat[3][3];

	copy_m3_m4(submat, bone->arm_mat);
	mat3_to_vec_roll(submat, NULL, &bone->arm_roll);

	for (child = bone->childbase.first; child; child = child->next)
		do_version_bone_roll_256(child);
}

static void do_versions_nodetree_dynamic_sockets(bNodeTree *ntree)
{
	bNodeSocket *sock;
	for (sock = ntree->inputs.first; sock; sock = sock->next)
		sock->flag |= SOCK_DYNAMIC;
	for (sock = ntree->outputs.first; sock; sock = sock->next)
		sock->flag |= SOCK_DYNAMIC;
}

void blo_do_versions_250(FileData *fd, Library *lib, Main *main)
{
	/* WATCH IT!!!: pointers from libdata have not been converted */

	if (main->versionfile < 250) {
		bScreen *screen;
		Scene *scene;
		Base *base;
		Material *ma;
		Camera *cam;
		Mesh *me;
		Curve *cu;
		Scene *sce;
		Tex *tx;
		ParticleSettings *part;
		Object *ob;
		//PTCacheID *pid;
		//ListBase pidlist;

		bSound *sound;
		Sequence *seq;
		bActuator *act;
		int a;

		for (sound = main->sound.first; sound; sound = sound->id.next) {
			if (sound->newpackedfile) {
				sound->packedfile = sound->newpackedfile;
				sound->newpackedfile = NULL;
			}
		}

		for (ob = main->object.first; ob; ob = ob->id.next) {
			for (act = ob->actuators.first; act; act = act->next) {
				if (act->type == ACT_SOUND) {
					bSoundActuator *sAct = (bSoundActuator*) act->data;
					if (sAct->sound) {
						sound = blo_do_versions_newlibadr(fd, lib, sAct->sound);
						sAct->flag = sound->flags & SOUND_FLAGS_3D ? ACT_SND_3D_SOUND : 0;
						sAct->pitch = sound->pitch;
						sAct->volume = sound->volume;
						sAct->sound3D.reference_distance = sound->distance;
						sAct->sound3D.max_gain = sound->max_gain;
						sAct->sound3D.min_gain = sound->min_gain;
						sAct->sound3D.rolloff_factor = sound->attenuation;
					}
					else {
						sAct->sound3D.reference_distance = 1.0f;
						sAct->volume = 1.0f;
						sAct->sound3D.max_gain = 1.0f;
						sAct->sound3D.rolloff_factor = 1.0f;
					}
					sAct->sound3D.cone_inner_angle = 360.0f;
					sAct->sound3D.cone_outer_angle = 360.0f;
					sAct->sound3D.max_distance = FLT_MAX;
				}
			}
		}

		for (scene = main->scene.first; scene; scene = scene->id.next) {
			if (scene->ed && scene->ed->seqbasep) {
				SEQ_BEGIN (scene->ed, seq)
				{
					if (seq->type == SEQ_TYPE_SOUND_HD) {
						char str[FILE_MAX];
						BLI_join_dirfile(str, sizeof(str), seq->strip->dir, seq->strip->stripdata->name);
						BLI_path_abs(str, main->name);
						seq->sound = sound_new_file(main, str);
					}
					/* don't know, if anybody used that this way, but just in case, upgrade to new way... */
					if ((seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) &&
					   !(seq->flag & SEQ_USE_PROXY_CUSTOM_DIR))
					{
						BLI_snprintf(seq->strip->proxy->dir, FILE_MAXDIR, "%s/BL_proxy", seq->strip->dir);
					}
				}
				SEQ_END
			}
		}

		for (screen = main->screen.first; screen; screen = screen->id.next) {
			do_versions_windowmanager_2_50(screen);
			do_versions_gpencil_2_50(main, screen);
		}

		/* shader, composite and texture node trees have id.name empty, put something in
		 * to have them show in RNA viewer and accessible otherwise.
		 */
		for (ma = main->mat.first; ma; ma = ma->id.next) {
			if (ma->nodetree && ma->nodetree->id.name[0] == '\0')
				strcpy(ma->nodetree->id.name, "NTShader Nodetree");

			/* which_output 0 is now "not specified" */
			for (a = 0; a < MAX_MTEX; a++) {
				if (ma->mtex[a]) {
					tx = blo_do_versions_newlibadr(fd, lib, ma->mtex[a]->tex);
					if (tx && tx->use_nodes)
						ma->mtex[a]->which_output++;
				}
			}
		}

		/* and composite trees */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->nodetree && sce->nodetree->id.name[0] == '\0')
				strcpy(sce->nodetree->id.name, "NTCompositing Nodetree");

			/* move to cameras */
			if (sce->r.mode & R_PANORAMA) {
				for (base = sce->base.first; base; base = base->next) {
					ob = blo_do_versions_newlibadr(fd, lib, base->object);

					if (ob->type == OB_CAMERA && !ob->id.lib) {
						cam = blo_do_versions_newlibadr(fd, lib, ob->data);
						cam->flag |= CAM_PANORAMA;
					}
				}

				sce->r.mode &= ~R_PANORAMA;
			}
		}

		/* and texture trees */
		for (tx = main->tex.first; tx; tx = tx->id.next) {
			bNode *node;

			if (tx->nodetree) {
				if (tx->nodetree->id.name[0] == '\0')
					strcpy(tx->nodetree->id.name, "NTTexture Nodetree");

				/* which_output 0 is now "not specified" */
				for (node = tx->nodetree->nodes.first; node; node = node->next)
					if (node->type == TEX_NODE_OUTPUT)
						node->custom1++;
			}
		}

		/* copy standard draw flag to meshes(used to be global, is not available here) */
		for (me = main->mesh.first; me; me = me->id.next) {
			me->drawflag = ME_DRAWEDGES|ME_DRAWFACES|ME_DRAWCREASES;
		}

		/* particle draw and render types */
		for (part = main->particle.first; part; part = part->id.next) {
			if (part->draw_as) {
				if (part->draw_as == PART_DRAW_DOT) {
					part->ren_as = PART_DRAW_HALO;
					part->draw_as = PART_DRAW_REND;
				}
				else if (part->draw_as <= PART_DRAW_AXIS) {
					part->ren_as = PART_DRAW_HALO;
				}
				else {
					part->ren_as = part->draw_as;
					part->draw_as = PART_DRAW_REND;
				}
			}
			part->path_end = 1.0f;
			part->clength = 1.0f;
		}

		/* set old pointcaches to have disk cache flag */
		for (ob = main->object.first; ob; ob = ob->id.next) {

			//BKE_ptcache_ids_from_object(&pidlist, ob);

			//for (pid = pidlist.first; pid; pid = pid->next)
			//	pid->cache->flag |= PTCACHE_DISK_CACHE;

			//BLI_freelistN(&pidlist);
		}

		/* type was a mixed flag & enum. move the 2d flag elsewhere */
		for (cu = main->curve.first; cu; cu = cu->id.next) {
			Nurb *nu;

			for (nu = cu->nurb.first; nu; nu = nu->next) {
				nu->flag |= (nu->type & CU_2D);
				nu->type &= CU_TYPE;
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 1)) {
		Object *ob;
		Material *ma;
		Tex *tex;
		Scene *sce;
		ToolSettings *ts;
		//PTCacheID *pid;
		//ListBase pidlist;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			//BKE_ptcache_ids_from_object(&pidlist, ob);

			//for (pid = pidlist.first; pid; pid = pid->next) {
			//	if (pid->ptcaches->first == NULL)
			//		pid->ptcaches->first = pid->ptcaches->last = pid->cache;
			//}

			//BLI_freelistN(&pidlist);

			if (ob->type == OB_MESH) {
				Mesh *me = blo_do_versions_newlibadr(fd, lib, ob->data);
				void *olddata = ob->data;
				ob->data = me;

				/* XXX - library meshes crash on loading most yoFrankie levels,
				 * the multires pointer gets invalid -  Campbell */
				if (me && me->id.lib == NULL && me->mr && me->mr->level_count > 1) {
					multires_load_old(ob, me);
				}

				ob->data = olddata;
			}

			if (ob->totcol && ob->matbits == NULL) {
				int a;

				ob->matbits = MEM_callocN(sizeof(char)*ob->totcol, "ob->matbits");
				for (a = 0; a < ob->totcol; a++)
					ob->matbits[a] = ob->colbits & (1<<a);
			}
		}

		/* texture filter */
		for (tex = main->tex.first; tex; tex = tex->id.next) {
			if (tex->afmax == 0)
				tex->afmax = 8;
		}

		for (ma = main->mat.first; ma; ma = ma->id.next) {
			int a;

			if (ma->mode & MA_WIRE) {
				ma->material_type = MA_TYPE_WIRE;
				ma->mode &= ~MA_WIRE;
			}

			if (ma->mode & MA_HALO) {
				ma->material_type = MA_TYPE_HALO;
				ma->mode &= ~MA_HALO;
			}

			if (ma->mode & (MA_ZTRANSP|MA_RAYTRANSP)) {
				ma->mode |= MA_TRANSP;
			}
			else {
				/* ma->mode |= MA_ZTRANSP; */ /* leave ztransp as is even if its not used [#28113] */
				ma->mode &= ~MA_TRANSP;
			}

			/* set new bump for unused slots */
			for (a = 0; a < MAX_MTEX; a++) {
				if (ma->mtex[a]) {
					tex = ma->mtex[a]->tex;
					if (!tex) {
						ma->mtex[a]->texflag |= MTEX_3TAP_BUMP;
						ma->mtex[a]->texflag |= MTEX_BUMP_OBJECTSPACE;
					}
					else {
						tex = (Tex*) blo_do_versions_newlibadr(fd, ma->id.lib, tex);
						if (tex && tex->type == 0) { /* invalid type */
							ma->mtex[a]->texflag |= MTEX_3TAP_BUMP;
							ma->mtex[a]->texflag |= MTEX_BUMP_OBJECTSPACE;
						}
					}
				}
			}

			/* volume rendering settings */
			if (ma->vol.stepsize < 0.0001f) {
				ma->vol.density = 1.0f;
				ma->vol.emission = 0.0f;
				ma->vol.scattering = 1.0f;
				ma->vol.emission_col[0] = ma->vol.emission_col[1] = ma->vol.emission_col[2] = 1.0f;
				ma->vol.density_scale = 1.0f;
				ma->vol.depth_cutoff = 0.01f;
				ma->vol.stepsize_type = MA_VOL_STEP_RANDOMIZED;
				ma->vol.stepsize = 0.2f;
				ma->vol.shade_type = MA_VOL_SHADE_SHADED;
				ma->vol.shadeflag |= MA_VOL_PRECACHESHADING;
				ma->vol.precache_resolution = 50;
			}
		}

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			ts = sce->toolsettings;
			if (ts->normalsize == 0.0f || !ts->uv_selectmode || ts->vgroup_weight == 0.0f) {
				ts->normalsize = 0.1f;
				ts->selectmode = SCE_SELECT_VERTEX;

				/* autokeying - setting should be taken from the user-prefs
				 * but the userprefs version may not have correct flags set
				 * (i.e. will result in blank box when enabled)
				 */
				ts->autokey_mode = U.autokey_mode;
				if (ts->autokey_mode == 0)
					ts->autokey_mode = 2; /* 'add/replace' but not on */
				ts->uv_selectmode = UV_SELECT_VERTEX;
				ts->vgroup_weight = 1.0f;
			}

			/* Game Settings */
			/* Dome */
			sce->gm.dome.angle = sce->r.domeangle;
			sce->gm.dome.mode = sce->r.domemode;
			sce->gm.dome.res = sce->r.domeres;
			sce->gm.dome.resbuf = sce->r.domeresbuf;
			sce->gm.dome.tilt = sce->r.dometilt;
			sce->gm.dome.warptext = sce->r.dometext;

			/* Stand Alone */
			sce->gm.playerflag |= (sce->r.fullscreen?GAME_PLAYER_FULLSCREEN:0);
			sce->gm.xplay = sce->r.xplay;
			sce->gm.yplay = sce->r.yplay;
			sce->gm.freqplay = sce->r.freqplay;
			sce->gm.depth = sce->r.depth;
			sce->gm.attrib = sce->r.attrib;

			/* Stereo */
			sce->gm.stereomode = sce->r.stereomode;
			/* reassigning stereomode NO_STEREO and DOME to a separeted flag*/
			if (sce->gm.stereomode == 1) { // 1 = STEREO_NOSTEREO
				sce->gm.stereoflag = STEREO_NOSTEREO;
				sce->gm.stereomode = STEREO_ANAGLYPH;
			}
			else if (sce->gm.stereomode == 8) { // 8 = STEREO_DOME
				sce->gm.stereoflag = STEREO_DOME;
				sce->gm.stereomode = STEREO_ANAGLYPH;
			}
			else
				sce->gm.stereoflag = STEREO_ENABLED;

			/* Framing */
			sce->gm.framing = sce->framing;
			sce->gm.xplay = sce->r.xplay;
			sce->gm.yplay = sce->r.yplay;
			sce->gm.freqplay = sce->r.freqplay;
			sce->gm.depth = sce->r.depth;

			/* Physic (previously stored in world) */
			sce->gm.gravity =9.8f;
			sce->gm.physicsEngine = WOPHY_BULLET; /* Bullet by default */
			sce->gm.mode = WO_DBVT_CULLING;	/* DBVT culling by default */
			sce->gm.occlusionRes = 128;
			sce->gm.ticrate = 60;
			sce->gm.maxlogicstep = 5;
			sce->gm.physubstep = 1;
			sce->gm.maxphystep = 5;
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 2)) {
		Scene *sce;
		Object *ob;

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (fd->fileflags & G_FILE_ENABLE_ALL_FRAMES)
				sce->gm.flag |= GAME_ENABLE_ALL_FRAMES;
			if (fd->fileflags & G_FILE_SHOW_DEBUG_PROPS)
				sce->gm.flag |= GAME_SHOW_DEBUG_PROPS;
			if (fd->fileflags & G_FILE_SHOW_FRAMERATE)
				sce->gm.flag |= GAME_SHOW_FRAMERATE;
			if (fd->fileflags & G_FILE_SHOW_PHYSICS)
				sce->gm.flag |= GAME_SHOW_PHYSICS;
			if (fd->fileflags & G_FILE_GLSL_NO_SHADOWS)
				sce->gm.flag |= GAME_GLSL_NO_SHADOWS;
			if (fd->fileflags & G_FILE_GLSL_NO_SHADERS)
				sce->gm.flag |= GAME_GLSL_NO_SHADERS;
			if (fd->fileflags & G_FILE_GLSL_NO_RAMPS)
				sce->gm.flag |= GAME_GLSL_NO_RAMPS;
			if (fd->fileflags & G_FILE_GLSL_NO_NODES)
				sce->gm.flag |= GAME_GLSL_NO_NODES;
			if (fd->fileflags & G_FILE_GLSL_NO_EXTRA_TEX)
				sce->gm.flag |= GAME_GLSL_NO_EXTRA_TEX;
			if (fd->fileflags & G_FILE_IGNORE_DEPRECATION_WARNINGS)
				sce->gm.flag |= GAME_IGNORE_DEPRECATION_WARNINGS;

			if (fd->fileflags & G_FILE_GAME_MAT_GLSL)
				sce->gm.matmode = GAME_MAT_GLSL;
			else if (fd->fileflags & G_FILE_GAME_MAT)
				sce->gm.matmode = GAME_MAT_MULTITEX;
			else
				sce->gm.matmode = GAME_MAT_TEXFACE;

			sce->gm.flag |= GAME_DISPLAY_LISTS;
		}

		for (ob = main->object.first; ob; ob = ob->id.next) {
			if (ob->flag & 8192) // OB_POSEMODE = 8192
				ob->mode |= OB_MODE_POSE;
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 4)) {
		Scene *sce;
		Object *ob;
		Material *ma;
		Lamp *la;
		World *wo;
		Tex *tex;
		ParticleSettings *part;
		int do_gravity = FALSE;

		for (sce = main->scene.first; sce; sce = sce->id.next)
			if (sce->unit.scale_length == 0.0f)
				sce->unit.scale_length = 1.0f;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			/* fluid-sim stuff */
			FluidsimModifierData *fluidmd = (FluidsimModifierData *) modifiers_findByType(ob, eModifierType_Fluidsim);
			if (fluidmd)
				fluidmd->fss->fmd = fluidmd;

			/* rotation modes were added, but old objects would now default to being 'quaternion based' */
			ob->rotmode = ROT_MODE_EUL;
		}

		for (ma = main->mat.first; ma; ma = ma->id.next) {
			if (ma->vol.reflection == 0.f) {
				ma->vol.reflection = 1.f;
				ma->vol.transmission_col[0] = ma->vol.transmission_col[1] = ma->vol.transmission_col[2] = 1.0f;
				ma->vol.reflection_col[0] = ma->vol.reflection_col[1] = ma->vol.reflection_col[2] = 1.0f;
			}

			do_version_mtex_factor_2_50(ma->mtex, ID_MA);
		}

		for (la = main->lamp.first; la; la = la->id.next)
			do_version_mtex_factor_2_50(la->mtex, ID_LA);

		for (wo = main->world.first; wo; wo = wo->id.next)
			do_version_mtex_factor_2_50(wo->mtex, ID_WO);

		for (tex = main->tex.first; tex; tex = tex->id.next)
			if (tex->vd)
				if (tex->vd->extend == 0)
					tex->vd->extend = TEX_CLIP;

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->audio.main == 0.0f)
				sce->audio.main = 1.0f;

			sce->r.ffcodecdata.audio_mixrate = sce->audio.mixrate;
			sce->r.ffcodecdata.audio_volume = sce->audio.main;
			sce->audio.distance_model = 2;
			sce->audio.doppler_factor = 1.0f;
			sce->audio.speed_of_sound = 343.3f;
		}

		/* Add default gravity to scenes */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if ((sce->physics_settings.flag & PHYS_GLOBAL_GRAVITY) == 0 &&
			    len_v3(sce->physics_settings.gravity) == 0.0f)
			{
				sce->physics_settings.gravity[0] = sce->physics_settings.gravity[1] = 0.0f;
				sce->physics_settings.gravity[2] = -9.81f;
				sce->physics_settings.flag = PHYS_GLOBAL_GRAVITY;
				do_gravity = TRUE;
			}
		}

		/* Assign proper global gravity weights for dynamics (only z-coordinate is taken into account) */
		if (do_gravity) {
			for (part = main->particle.first; part; part = part->id.next)
				part->effector_weights->global_gravity = part->acc[2]/-9.81f;
		}

		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;

			if (do_gravity) {
				for (md = ob->modifiers.first; md; md = md->next) {
					ClothModifierData *clmd = (ClothModifierData *) modifiers_findByType(ob, eModifierType_Cloth);
					if (clmd)
						clmd->sim_parms->effector_weights->global_gravity = clmd->sim_parms->gravity[2]/-9.81f;
				}

				if (ob->soft)
					ob->soft->effector_weights->global_gravity = ob->soft->grav/9.81f;
			}

			/* Normal wind shape is plane */
			if (ob->pd) {
				if (ob->pd->forcefield == PFIELD_WIND)
					ob->pd->shape = PFIELD_SHAPE_PLANE;

				if (ob->pd->flag & PFIELD_PLANAR)
					ob->pd->shape = PFIELD_SHAPE_PLANE;
				else if (ob->pd->flag & PFIELD_SURFACE)
					ob->pd->shape = PFIELD_SHAPE_SURFACE;

				ob->pd->flag |= PFIELD_DO_LOCATION;
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 6)) {
		Object *ob;
		Lamp *la;

		/* New variables for axis-angle rotations and/or quaternion rotations were added, and need proper initialization */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			/* new variables for all objects */
			ob->quat[0] = 1.0f;
			ob->rotAxis[1] = 1.0f;

			/* bones */
			if (ob->pose) {
				bPoseChannel *pchan;

				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					/* just need to initalise rotation axis properly... */
					pchan->rotAxis[1] = 1.0f;
				}
			}
		}

		for (la = main->lamp.first; la; la = la->id.next)
			la->compressthresh = 0.05f;
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 7)) {
		Mesh *me;
		Nurb *nu;
		Lattice *lt;
		Curve *cu;
		Key *key;
		float *data;
		int a, tot;

		/* shape keys are no longer applied to the mesh itself, but rather
		 * to the derivedmesh/displist, so here we ensure that the basis
		 * shape key is always set in the mesh coordinates. */
		for (me = main->mesh.first; me; me = me->id.next) {
			if ((key = blo_do_versions_newlibadr(fd, lib, me->key)) && key->refkey) {
				data = key->refkey->data;
				tot = MIN2(me->totvert, key->refkey->totelem);

				for (a = 0; a < tot; a++, data += 3)
					copy_v3_v3(me->mvert[a].co, data);
			}
		}

		for (lt = main->latt.first; lt; lt = lt->id.next) {
			if ((key = blo_do_versions_newlibadr(fd, lib, lt->key)) && key->refkey) {
				data = key->refkey->data;
				tot = MIN2(lt->pntsu*lt->pntsv*lt->pntsw, key->refkey->totelem);

				for (a = 0; a < tot; a++, data += 3)
					copy_v3_v3(lt->def[a].vec, data);
			}
		}

		for (cu = main->curve.first; cu; cu = cu->id.next) {
			if ((key = blo_do_versions_newlibadr(fd, lib, cu->key)) && key->refkey) {
				data = key->refkey->data;

				for (nu = cu->nurb.first; nu; nu = nu->next) {
					if (nu->bezt) {
						BezTriple *bezt = nu->bezt;

						for (a = 0; a < nu->pntsu; a++, bezt++) {
							copy_v3_v3(bezt->vec[0], data); data+=3;
							copy_v3_v3(bezt->vec[1], data); data+=3;
							copy_v3_v3(bezt->vec[2], data); data+=3;
							bezt->alfa = *data; data++;
						}
					}
					else if (nu->bp) {
						BPoint *bp = nu->bp;

						for (a = 0; a < nu->pntsu*nu->pntsv; a++, bp++) {
							copy_v3_v3(bp->vec, data); data += 3;
							bp->alfa = *data; data++;
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 8)) {
		{
			Scene *sce = main->scene.first;
			while (sce) {
				if (sce->r.frame_step == 0)
					sce->r.frame_step = 1;
				if (sce->r.mblur_samples == 0)
					sce->r.mblur_samples = sce->r.osa;

				if (sce->ed && sce->ed->seqbase.first) {
					do_versions_seq_unique_name_all_strips(sce, &sce->ed->seqbase);
				}

				sce = sce->id.next;
			}
		}

		{
			/* ensure all nodes have unique names */
			bNodeTree *ntree = main->nodetree.first;
			while (ntree) {
				bNode *node = ntree->nodes.first;

				while (node) {
					nodeUniqueName(ntree, node);
					node = node->next;
				}

				ntree = ntree->id.next;
			}
		}

		{
			Object *ob = main->object.first;
			while (ob) {
				/* shaded mode disabled for now */
				if (ob->dt == OB_MATERIAL)
					ob->dt = OB_TEXTURE;
				ob = ob->id.next;
			}
		}

		{
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;

			for (screen = main->screen.first; screen; screen = screen->id.next) {
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *) sl;
							if (v3d->drawtype == OB_MATERIAL)
								v3d->drawtype = OB_SOLID;
						}
					}
				}
			}
		}

		/* only convert old 2.50 files with color management */
		if (main->versionfile == 250) {
			Scene *sce = main->scene.first;
			Material *ma = main->mat.first;
			World *wo = main->world.first;
			Tex *tex = main->tex.first;
			int i, convert = 0;

			/* convert to new color management system:
			 * while previously colors were stored as srgb,
			 * now they are stored as linear internally,
			 * with screen gamma correction in certain places in the UI. */

			/* don't know what scene is active, so we'll convert if any scene has it enabled... */
			while (sce) {
				if (sce->r.color_mgt_flag & R_COLOR_MANAGEMENT)
					convert = 1;
				sce = sce->id.next;
			}

			if (convert) {
				while (ma) {
					if (ma->ramp_col) {
						ColorBand *band = (ColorBand *)ma->ramp_col;
						for (i = 0; i < band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}

					if (ma->ramp_spec) {
						ColorBand *band = (ColorBand *)ma->ramp_spec;
						for (i = 0; i < band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}

					srgb_to_linearrgb_v3_v3(&ma->r, &ma->r);
					srgb_to_linearrgb_v3_v3(&ma->specr, &ma->specr);
					srgb_to_linearrgb_v3_v3(&ma->mirr, &ma->mirr);
					srgb_to_linearrgb_v3_v3(ma->sss_col, ma->sss_col);
					ma = ma->id.next;
				}

				while (tex) {
					if (tex->coba) {
						ColorBand *band = (ColorBand *)tex->coba;
						for (i = 0; i < band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}
					tex = tex->id.next;
				}

				while (wo) {
					srgb_to_linearrgb_v3_v3(&wo->ambr, &wo->ambr);
					srgb_to_linearrgb_v3_v3(&wo->horr, &wo->horr);
					srgb_to_linearrgb_v3_v3(&wo->zenr, &wo->zenr);
					wo = wo->id.next;
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 9)) {
		Scene *sce;
		Mesh *me;
		Object *ob;

		for (sce = main->scene.first; sce; sce = sce->id.next)
			if (!sce->toolsettings->particle.selectmode)
				sce->toolsettings->particle.selectmode = SCE_SELECT_PATH;

		if (main->versionfile == 250 && main->subversionfile > 1) {
			for (me = main->mesh.first; me; me = me->id.next)
				multires_load_old_250(me);

			for (ob = main->object.first; ob; ob = ob->id.next) {
				MultiresModifierData *mmd = (MultiresModifierData *) modifiers_findByType(ob, eModifierType_Multires);

				if (mmd) {
					mmd->totlvl--;
					mmd->lvl--;
					mmd->sculptlvl = mmd->lvl;
					mmd->renderlvl = mmd->lvl;
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 10)) {
		Object *ob;

		/* properly initialize hair clothsim data on old files */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Cloth) {
					ClothModifierData *clmd = (ClothModifierData *)md;
					if (clmd->sim_parms->velocity_smooth < 0.01f)
						clmd->sim_parms->velocity_smooth = 0.f;
				}
			}
		}
	}

	/* fix bad area setup in subversion 10 */
	if (main->versionfile == 250 && main->subversionfile == 10) {
		/* fix for new view type in sequencer */
		bScreen *screen;
		ScrArea *sa;
		SpaceLink *sl;

		/* remove all preview window in wrong spaces */
		for (screen = main->screen.first; screen; screen = screen->id.next) {
			for (sa = screen->areabase.first; sa; sa = sa->next) {
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype != SPACE_SEQ) {
						ARegion *ar;
						ListBase *regionbase;

						if (sl == sa->spacedata.first) {
							regionbase = &sa->regionbase;
						}
						else {
							regionbase = &sl->regionbase;
						}

						for (ar = regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_PREVIEW)
								break;
						}

						if (ar && (ar->regiontype == RGN_TYPE_PREVIEW)) {
							SpaceType *st = BKE_spacetype_from_id(SPACE_SEQ);
							BKE_area_region_free(st, ar);
							BLI_freelinkN(regionbase, ar);
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 11)) {
		{
			/* fix for new view type in sequencer */
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;

			for (screen = main->screen.first; screen; screen = screen->id.next) {
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_SEQ) {
							ARegion *ar;
							ARegion *ar_main;
							ListBase *regionbase;
							SpaceSeq *sseq = (SpaceSeq *)sl;

							if (sl == sa->spacedata.first) {
								regionbase = &sa->regionbase;
							}
							else {
								regionbase = &sl->regionbase;
							}

							if (sseq->view == 0)
								sseq->view = SEQ_VIEW_SEQUENCE;
							if (sseq->mainb == 0)
								sseq->mainb = SEQ_DRAW_IMG_IMBUF;

							ar_main = (ARegion*)regionbase->first;
							for (; ar_main; ar_main = ar_main->next) {
								if (ar_main->regiontype == RGN_TYPE_WINDOW)
									break;
							}
							ar = MEM_callocN(sizeof(ARegion), "preview area for sequencer");
							BLI_insertlinkbefore(regionbase, ar_main, ar);
							sequencer_init_preview_region(ar);
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 12)) {
		Scene *sce;
		Object *ob;
		Brush *brush;
		Material *ma;

		/* game engine changes */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			sce->gm.eyeseparation = 0.10f;
		}

		/* anim viz changes */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			/* initialize object defaults */
			animviz_settings_init(&ob->avs);

			/* if armature, copy settings for pose from armature data
			 * performing initialization where appropriate
			 */
			if (ob->pose && ob->data) {
				bArmature *arm = blo_do_versions_newlibadr(fd, lib, ob->data);
				if (arm) { /* XXX - why does this fail in some cases? */
					bAnimVizSettings *avs = &ob->pose->avs;

					/* ghosting settings ---------------- */
						/* ranges */
					avs->ghost_bc = avs->ghost_ac = arm->ghostep;

					avs->ghost_sf = arm->ghostsf;
					avs->ghost_ef = arm->ghostef;
					if ((avs->ghost_sf == avs->ghost_ef) && (avs->ghost_sf == 0)) {
						avs->ghost_sf = 1;
						avs->ghost_ef = 100;
					}

						/* type */
					if (arm->ghostep == 0)
						avs->ghost_type = GHOST_TYPE_NONE;
					else
						avs->ghost_type = arm->ghosttype + 1;

						/* stepsize */
					avs->ghost_step = arm->ghostsize;
					if (avs->ghost_step == 0)
						avs->ghost_step = 1;

					/* path settings --------------------- */
						/* ranges */
					avs->path_bc = arm->pathbc;
					avs->path_ac = arm->pathac;
					if ((avs->path_bc == avs->path_ac) && (avs->path_bc == 0))
						avs->path_bc = avs->path_ac = 10;

					avs->path_sf = arm->pathsf;
					avs->path_ef = arm->pathef;
					if ((avs->path_sf == avs->path_ef) && (avs->path_sf == 0)) {
						avs->path_sf = 1;
						avs->path_ef = 250;
					}

						/* flags */
					if (arm->pathflag & ARM_PATH_FNUMS)
						avs->path_viewflag |= MOTIONPATH_VIEW_FNUMS;
					if (arm->pathflag & ARM_PATH_KFRAS)
						avs->path_viewflag |= MOTIONPATH_VIEW_KFRAS;
					if (arm->pathflag & ARM_PATH_KFNOS)
						avs->path_viewflag |= MOTIONPATH_VIEW_KFNOS;

						/* bake flags */
					if (arm->pathflag & ARM_PATH_HEADS)
						avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;

						/* type */
					if (arm->pathflag & ARM_PATH_ACFRA)
						avs->path_type = MOTIONPATH_TYPE_ACFRA;

						/* stepsize */
					avs->path_step = arm->pathsize;
					if (avs->path_step == 0)
						avs->path_step = 1;
				}
				else
					animviz_settings_init(&ob->pose->avs);
			}
		}

		/* brush texture changes */
		for (brush = main->brush.first; brush; brush = brush->id.next) {
			default_mtex(&brush->mtex);
		}

		for (ma = main->mat.first; ma; ma = ma->id.next) {
			if (ma->vol.ms_spread < 0.0001f) {
				ma->vol.ms_spread = 0.2f;
				ma->vol.ms_diff = 1.f;
				ma->vol.ms_intensity = 1.f;
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 13)) {
		/* NOTE: if you do more conversion, be sure to do it outside of this and
		 * increase subversion again, otherwise it will not be correct */
		Object *ob;

		/* convert degrees to radians for internal use */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			bPoseChannel *pchan;

			do_version_constraints_radians_degrees_250(&ob->constraints);

			if (ob->pose) {
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					pchan->limitmin[0] *= (float)(M_PI / 180.0);
					pchan->limitmin[1] *= (float)(M_PI / 180.0);
					pchan->limitmin[2] *= (float)(M_PI / 180.0);
					pchan->limitmax[0] *= (float)(M_PI / 180.0);
					pchan->limitmax[1] *= (float)(M_PI / 180.0);
					pchan->limitmax[2] *= (float)(M_PI / 180.0);

					do_version_constraints_radians_degrees_250(&pchan->constraints);
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 14)) {
		/* fix for bad View2D extents for Animation Editors */
		bScreen *screen;
		ScrArea *sa;
		SpaceLink *sl;

		for (screen = main->screen.first; screen; screen = screen->id.next) {
			for (sa = screen->areabase.first; sa; sa = sa->next) {
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					ListBase *regionbase;
					ARegion *ar;

					if (sl == sa->spacedata.first)
						regionbase = &sa->regionbase;
					else
						regionbase = &sl->regionbase;

					if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_NLA)) {
						for (ar = (ARegion*) regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								ar->v2d.cur.ymax = ar->v2d.tot.ymax = 0.0f;
								ar->v2d.cur.ymin = ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
							}
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 15)) {
		World *wo;
		Material *ma;

		/* ambient default from 0.5f to 1.0f */
		for (ma = main->mat.first; ma; ma = ma->id.next)
			ma->amb *= 2.0f;

		for (wo = main->world.first; wo; wo = wo->id.next) {
			/* ao splitting into ao/env/indirect */
			wo->ao_env_energy = wo->aoenergy;
			wo->aoenergy = 1.0f;

			if (wo->ao_indirect_bounces == 0)
				wo->ao_indirect_bounces = 1;
			else
				wo->mode |= WO_INDIRECT_LIGHT;

			if (wo->aomix == WO_AOSUB)
				wo->ao_env_energy = -wo->ao_env_energy;
			else if (wo->aomix == WO_AOADDSUB)
				wo->mode |= WO_AMB_OCC;

			wo->aomix = WO_AOMUL;

			/* ambient default from 0.5f to 1.0f */
			mul_v3_fl(&wo->ambr, 0.5f);
			wo->ao_env_energy *= 0.5f;
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 17)) {
		Scene *sce;
		Sequence *seq;
		Material *ma;

		/* initialize to sane default so toggling on border shows something */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->r.border.xmin == 0.0f && sce->r.border.ymin == 0.0f &&
			    sce->r.border.xmax == 0.0f && sce->r.border.ymax == 0.0f)
			{
				sce->r.border.xmin = 0.0f;
				sce->r.border.ymin = 0.0f;
				sce->r.border.xmax = 1.0f;
				sce->r.border.ymax = 1.0f;
			}

			if ((sce->r.ffcodecdata.flags & FFMPEG_MULTIPLEX_AUDIO) == 0)
				sce->r.ffcodecdata.audio_codec = 0x0; // CODEC_ID_NONE

			SEQ_BEGIN (sce->ed, seq)
			{
				seq->volume = 1.0f;
			}
			SEQ_END
		}

		/* particle brush strength factor was changed from int to float */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			ParticleEditSettings *pset = &sce->toolsettings->particle;
			int a;

			for (a = 0; a < PE_TOT_BRUSH; a++)
				pset->brush[a].strength /= 100.0f;
		}

		for (ma = main->mat.first; ma; ma = ma->id.next)
			if (ma->mode & MA_TRACEBLE)
				ma->shade_flag |= MA_APPROX_OCCLUSION;

		/* sequencer changes */
		{
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;

			for (screen = main->screen.first; screen; screen = screen->id.next) {
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_SEQ) {
							ARegion *ar_preview;
							ListBase *regionbase;

							if (sl == sa->spacedata.first) {
								regionbase = &sa->regionbase;
							}
							else {
								regionbase = &sl->regionbase;
							}

							ar_preview = (ARegion*) regionbase->first;
							for (; ar_preview; ar_preview = ar_preview->next) {
								if (ar_preview->regiontype == RGN_TYPE_PREVIEW)
									break;
							}
							if (ar_preview && (ar_preview->regiontype == RGN_TYPE_PREVIEW)) {
								sequencer_init_preview_region(ar_preview);
							}
						}
					}
				}
			}
		} /* sequencer changes */
	}

	if (main->versionfile <= 251) {	/* 2.5.1 had no subversions */
		bScreen *sc;

		/* Blender 2.5.2 - subversion 0 introduced a new setting: V3D_RENDER_OVERRIDE.
		 * This bit was used in the past for V3D_TRANSFORM_SNAP, which is now deprecated.
		 * Here we clear it for old files so they don't come in with V3D_RENDER_OVERRIDE set,
		 * which would cause cameras, lamps, etc to become invisible */
		for (sc = main->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D* v3d = (View3D *)sl;
						v3d->flag2 &= ~V3D_RENDER_OVERRIDE;
					}
				}
			}
		}
	}

	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 1)) {
		Brush *brush;
		Object *ob;
		Scene *scene;
		bNodeTree *ntree;

		for (brush = main->brush.first; brush; brush = brush->id.next) {
			if (brush->curve)
				brush->curve->preset = CURVE_PRESET_SMOOTH;
		}

		/* properly initialize active flag for fluidsim modifiers */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Fluidsim) {
					FluidsimModifierData *fmd = (FluidsimModifierData *) md;
					fmd->fss->flag |= OB_FLUIDSIM_ACTIVE;
					fmd->fss->flag |= OB_FLUIDSIM_OVERRIDE_TIME;
				}
			}
		}

		/* adjustment to color balance node values */
		for (scene = main->scene.first; scene; scene = scene->id.next) {
			if (scene->nodetree) {
				bNode *node = scene->nodetree->nodes.first;

				while (node) {
					if (node->type == CMP_NODE_COLORBALANCE) {
						NodeColorBalance *n = (NodeColorBalance *) node->storage;
						n->lift[0] += 1.f;
						n->lift[1] += 1.f;
						n->lift[2] += 1.f;
					}
					node = node->next;
				}
			}
		}
		/* check inside node groups too */
		for (ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
			bNode *node = ntree->nodes.first;

			while (node) {
				if (node->type == CMP_NODE_COLORBALANCE) {
					NodeColorBalance *n = (NodeColorBalance *) node->storage;
					n->lift[0] += 1.f;
					n->lift[1] += 1.f;
					n->lift[2] += 1.f;
				}

				node = node->next;
			}
		}
	}

	/* old-track -> constraints (this time we're really doing it!) */
	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 2)) {
		Object *ob;

		for (ob = main->object.first; ob; ob = ob->id.next)
			blo_do_version_old_trackto_to_constraints(ob);
	}

	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 5)) {
		bScreen *sc;

		/* Image editor scopes */
		for (sc = main->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;

			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;

				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_IMAGE) {
						SpaceImage *sima = (SpaceImage *) sl;
						scopes_new(&sima->scopes);
					}
				}
			}
		}
	}

	if (main->versionfile < 253) {
		Object *ob;
		Scene *scene;
		bScreen *sc;
		Tex *tex;
		Brush *brush;

		for (sc = main->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;

				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_NODE) {
						SpaceNode *snode = (SpaceNode *) sl;
						ListBase *regionbase;
						ARegion *ar;

						if (sl == sa->spacedata.first)
							regionbase = &sa->regionbase;
						else
							regionbase = &sl->regionbase;

						if (snode->v2d.minzoom > 0.09f)
							snode->v2d.minzoom = 0.09f;
						if (snode->v2d.maxzoom < 2.31f)
							snode->v2d.maxzoom = 2.31f;

						for (ar = regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								if (ar->v2d.minzoom > 0.09f)
									ar->v2d.minzoom = 0.09f;
								if (ar->v2d.maxzoom < 2.31f)
									ar->v2d.maxzoom = 2.31f;
							}
						}
					}
					else if (sl->spacetype == SPACE_TIME) {
						SpaceTime *stime = (SpaceTime *) sl;

						/* enable all cache display */
						stime->cache_display |= TIME_CACHE_DISPLAY;
						stime->cache_display |= (TIME_CACHE_SOFTBODY|TIME_CACHE_PARTICLES);
						stime->cache_display |= (TIME_CACHE_CLOTH|TIME_CACHE_SMOKE|TIME_CACHE_DYNAMICPAINT);
					}
				}
			}
		}

		do_version_mdef_250(main);

		/* parent type to modifier */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			if (ob->parent) {
				Object *parent = (Object *) blo_do_versions_newlibadr(fd, lib, ob->parent);
				if (parent) { /* parent may not be in group */
					if (parent->type == OB_ARMATURE && ob->partype == PARSKEL) {
						ArmatureModifierData *amd;
						bArmature *arm = (bArmature *) blo_do_versions_newlibadr(fd, lib, parent->data);

						amd = (ArmatureModifierData*) modifier_new(eModifierType_Armature);
						amd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, amd);
						amd->deformflag = arm->deformflag;
						ob->partype = PAROBJECT;
					}
					else if (parent->type == OB_LATTICE && ob->partype == PARSKEL) {
						LatticeModifierData *lmd;

						lmd = (LatticeModifierData*) modifier_new(eModifierType_Lattice);
						lmd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, lmd);
						ob->partype = PAROBJECT;
					}
					else if (parent->type == OB_CURVE && ob->partype == PARCURVE) {
						CurveModifierData *cmd;

						cmd = (CurveModifierData*) modifier_new(eModifierType_Curve);
						cmd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, cmd);
						ob->partype = PAROBJECT;
					}
				}
			}
		}

		/* initialize scene active layer */
		for (scene = main->scene.first; scene; scene = scene->id.next) {
			int i;
			for (i = 0; i < 20; i++) {
				if (scene->lay & (1<<i)) {
					scene->layact = 1<<i;
					break;
				}
			}
		}

		for (tex = main->tex.first; tex; tex = tex->id.next) {
			/* if youre picky, this isn't correct until we do a version bump
			 * since you could set saturation to be 0.0*/
			if (tex->saturation == 0.0f)
				tex->saturation = 1.0f;
		}

		{
			Curve *cu;
			for (cu = main->curve.first; cu; cu = cu->id.next) {
				cu->smallcaps_scale = 0.75f;
			}
		}

		for (scene = main->scene.first; scene; scene = scene->id.next) {
			if (scene) {
				Sequence *seq;
				SEQ_BEGIN (scene->ed, seq)
				{
					if (seq->sat == 0.0f) {
						seq->sat = 1.0f;
					}
				}
				SEQ_END
			}
		}

		/* GSOC 2010 Sculpt - New settings for Brush */

		for (brush = main->brush.first; brush; brush = brush->id.next) {
			/* Sanity Check */

			/* infinite number of dabs */
			if (brush->spacing == 0)
				brush->spacing = 10;

			/* will have no effect */
			if (brush->alpha == 0)
				brush->alpha = 0.5f;

			/* bad radius */
			if (brush->unprojected_radius == 0)
				brush->unprojected_radius = 0.125f;

			/* unusable size */
			if (brush->size == 0)
				brush->size = 35;

			/* can't see overlay */
			if (brush->texture_overlay_alpha == 0)
				brush->texture_overlay_alpha = 33;

			/* same as draw brush */
			if (brush->crease_pinch_factor == 0)
				brush->crease_pinch_factor = 0.5f;

			/* will sculpt no vertexes */
			if (brush->plane_trim == 0)
				brush->plane_trim = 0.5f;

			/* same as smooth stroke off */
			if (brush->smooth_stroke_radius == 0)
				brush->smooth_stroke_radius = 75;

			/* will keep cursor in one spot */
			if (brush->smooth_stroke_radius == 1)
				brush->smooth_stroke_factor = 0.9f;

			/* same as dots */
			if (brush->rate == 0)
				brush->rate = 0.1f;

			/* New Settings */
			if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 5)) {
				brush->flag |= BRUSH_SPACE_ATTEN; // explicitly enable adaptive space

				/* spacing was originally in pixels, convert it to percentage for new version
				 * size should not be zero due to sanity check above
				 */
				brush->spacing = (int)(100 * ((float)brush->spacing) / ((float) brush->size));

				if (brush->add_col[0] == 0 &&
					brush->add_col[1] == 0 &&
					brush->add_col[2] == 0)
				{
					brush->add_col[0] = 1.00f;
					brush->add_col[1] = 0.39f;
 					brush->add_col[2] = 0.39f;
				}

				if (brush->sub_col[0] == 0 &&
					brush->sub_col[1] == 0 &&
					brush->sub_col[2] == 0)
				{
					brush->sub_col[0] = 0.39f;
					brush->sub_col[1] = 0.39f;
					brush->sub_col[2] = 1.00f;
				}
			}
		}
	}

	/* GSOC Sculpt 2010 - Sanity check on Sculpt/Paint settings */
	if (main->versionfile < 253) {
		Scene *sce;
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->toolsettings->sculpt_paint_unified_alpha == 0)
				sce->toolsettings->sculpt_paint_unified_alpha = 0.5f;

			if (sce->toolsettings->sculpt_paint_unified_unprojected_radius == 0)
				sce->toolsettings->sculpt_paint_unified_unprojected_radius = 0.125f;

			if (sce->toolsettings->sculpt_paint_unified_size == 0)
				sce->toolsettings->sculpt_paint_unified_size = 35;
		}
	}

	if (main->versionfile < 253 || (main->versionfile == 253 && main->subversionfile < 1)) {
		Object *ob;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;

			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Smoke) {
					SmokeModifierData *smd = (SmokeModifierData *)md;

					if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
						smd->domain->vorticity = 2.0f;
						smd->domain->time_scale = 1.0f;

						if (!(smd->domain->flags & (1<<4)))
							continue;

						/* delete old MOD_SMOKE_INITVELOCITY flag */
						smd->domain->flags &= ~(1<<4);

						/* for now just add it to all flow objects in the scene */
						{
							Object *ob2;
							for (ob2 = main->object.first; ob2; ob2 = ob2->id.next) {
								ModifierData *md2;
								for (md2 = ob2->modifiers.first; md2; md2 = md2->next) {
									if (md2->type == eModifierType_Smoke) {
										SmokeModifierData *smd2 = (SmokeModifierData *)md2;

										if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
											smd2->flow->flags |= MOD_SMOKE_FLOW_INITVELOCITY;
										}
									}
								}
							}
						}

					}
					else if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
						smd->flow->vel_multi = 1.0f;
					}
				}
			}
		}
	}

	if (main->versionfile < 255 || (main->versionfile == 255 && main->subversionfile < 1)) {
		Brush *br;
		ParticleSettings *part;
		bScreen *sc;
		Object *ob;

		for (br = main->brush.first; br; br = br->id.next) {
			if (br->ob_mode == 0)
				br->ob_mode = OB_MODE_ALL_PAINT;
		}

		for (part = main->particle.first; part; part = part->id.next) {
			if (part->boids)
				part->boids->pitch = 1.0f;

			part->flag &= ~PART_HAIR_REGROW; /* this was a deprecated flag before */
			part->kink_amp_clump = 1.f; /* keep old files looking similar */
		}

		for (sc = main->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_INFO) {
						SpaceInfo *sinfo = (SpaceInfo *) sl;
						ARegion *ar;

						sinfo->rpt_mask = INFO_RPT_OP;

						for (ar = sa->regionbase.first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								ar->v2d.scroll = (V2D_SCROLL_RIGHT);
								ar->v2d.align = V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y; /* align bottom left */
								ar->v2d.keepofs = V2D_LOCKOFS_X;
								ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
								ar->v2d.keeptot = V2D_KEEPTOT_BOUNDS;
								ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;
							}
						}
					}
				}
			}
		}

		/* fix rotation actuators for objects so they use real angles (radians)
		 * since before blender went opensource this strange scalar was used: (1 / 0.02) * 2 * math.pi/360 */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			bActuator *act = ob->actuators.first;
			while (act) {
				if (act->type == ACT_OBJECT) {
					/* multiply velocity with 50 in old files */
					bObjectActuator *oa = act->data;
					mul_v3_fl(oa->drot, 0.8726646259971648f);
				}
				act = act->next;
			}
		}
	}

	/* init facing axis property of steering actuators */
	{
		Object *ob;
		for (ob = main->object.first; ob; ob = ob->id.next) {
			bActuator *act;
			for (act = ob->actuators.first; act; act = act->next) {
				if (act->type == ACT_STEERING) {
					bSteeringActuator* stact = act->data;
					if (stact->facingaxis == 0) {
						stact->facingaxis = 1;
					}
				}
			}
		}
	}

	if (main->versionfile < 255 || (main->versionfile == 255 && main->subversionfile < 3)) {
		Object *ob;

		/* ocean res is now squared, reset old ones - will be massive */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Ocean) {
					OceanModifierData *omd = (OceanModifierData *)md;
					omd->resolution = 7;
					omd->oceancache = NULL;
				}
			}
		}
	}

	if (main->versionfile < 256) {
		bScreen *sc;
		ScrArea *sa;
		Key *key;

		/* Fix for sample line scope initializing with no height */
		for (sc = main->screen.first; sc; sc = sc->id.next) {
			sa = sc->areabase.first;
			while (sa) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_IMAGE) {
						SpaceImage *sima = (SpaceImage *) sl;
						if (sima->sample_line_hist.height == 0)
							sima->sample_line_hist.height = 100;
					}
				}
				sa = sa->next;
			}
		}

		/* old files could have been saved with slidermin = slidermax = 0.0, but the UI in
		 * 2.4x would never reveal this to users as a dummy value always ended up getting used
		 * instead
		 */
		for (key = main->key.first; key; key = key->id.next) {
			KeyBlock *kb;

			for (kb = key->block.first; kb; kb = kb->next) {
				if (IS_EQF(kb->slidermin, kb->slidermax) && IS_EQ(kb->slidermax, 0))
					kb->slidermax = kb->slidermin + 1.0f;
			}
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 1)) {
		/* fix for bones that didn't have arm_roll before */
		bArmature *arm;
		Bone *bone;
		Object *ob;

		for (arm = main->armature.first; arm; arm = arm->id.next)
			for (bone = arm->bonebase.first; bone; bone = bone->next)
				do_version_bone_roll_256(bone);

		/* fix for objects which have zero dquat's
		 * since this is multiplied with the quat rather than added */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			if (is_zero_v4(ob->dquat)) {
				unit_qt(ob->dquat);
			}
			if (is_zero_v3(ob->drotAxis) && ob->drotAngle == 0.0f) {
				unit_axis_angle(ob->drotAxis, &ob->drotAngle);
			}
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 2)) {
		bNodeTree *ntree;

		/* node sockets are not exposed automatically any more,
		 * this mimics the old behavior by adding all unlinked sockets to groups.
		 */
		for (ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
			/* XXX Only setting a flag here. Actual adding of group sockets
			 * is done in lib_verify_nodetree, because at this point the internal
			 * nodes may not be up-to-date! (missing lib-link)
			 */
			ntree->flag |= NTREE_DO_VERSIONS_GROUP_EXPOSE;
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 3)) {
		bScreen *sc;
		Brush *brush;
		Object *ob;
		ParticleSettings *part;
		Material *mat;
		int tex_nr, transp_tex;

		for (mat = main->mat.first; mat; mat = mat->id.next) {
			if (!(mat->mode & MA_TRANSP) && !(mat->material_type & MA_TYPE_VOLUME)) {
				transp_tex = 0;

				for (tex_nr = 0; tex_nr < MAX_MTEX; tex_nr++) {
					if (!mat->mtex[tex_nr])
						continue;
					if (mat->mtex[tex_nr]->mapto & MAP_ALPHA)
						transp_tex = 1;
				}

				/* weak! material alpha could be animated */
				if (mat->alpha < 1.0f || mat->fresnel_tra > 0.0f || transp_tex) {
					mat->mode |= MA_TRANSP;
					mat->mode &= ~(MA_ZTRANSP|MA_RAYTRANSP);
				}
			}
		}

		/* redraws flag in SpaceTime has been moved to Screen level */
		for (sc = main->screen.first; sc; sc = sc->id.next) {
			if (sc->redraws_flag == 0) {
				/* just initialize to default? */
				// XXX: we could also have iterated through areas, and taken them from the first timeline available...
				sc->redraws_flag = TIME_ALL_3D_WIN|TIME_ALL_ANIM_WIN;
			}
		}

		for (brush = main->brush.first; brush; brush = brush->id.next) {
			if (brush->height == 0)
				brush->height = 0.4f;
		}

		/* replace 'rim material' option for in offset*/
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Solidify) {
					SolidifyModifierData *smd = (SolidifyModifierData *) md;
					if (smd->flag & MOD_SOLIDIFY_RIM_MATERIAL) {
						smd->mat_ofs_rim = 1;
						smd->flag &= ~MOD_SOLIDIFY_RIM_MATERIAL;
					}
				}
			}
		}

		/* particle draw color from material */
		for (part = main->particle.first; part; part = part->id.next) {
			if (part->draw & PART_DRAW_MAT_COL)
				part->draw_col = PART_DRAW_COL_MAT;
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 6)) {
		Mesh *me;

		for (me = main->mesh.first; me; me = me->id.next)
			BKE_mesh_calc_normals_tessface(me->mvert, me->totvert, me->mface, me->totface, NULL);
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 2)) {
		/* update blur area sizes from 0..1 range to 0..100 percentage */
		Scene *scene;
		bNode *node;
		for (scene = main->scene.first; scene; scene = scene->id.next)
			if (scene->nodetree)
				for (node = scene->nodetree->nodes.first; node; node = node->next)
					if (node->type == CMP_NODE_BLUR) {
						NodeBlurData *nbd = node->storage;
						nbd->percentx *= 100.0f;
						nbd->percenty *= 100.0f;
					}
	}

	if (main->versionfile < 258 || (main->versionfile == 258 && main->subversionfile < 1)) {
		/* screen view2d settings were not properly initialized [#27164]
		 * v2d->scroll caused the bug but best reset other values too which are in old blend files only.
		 * need to make less ugly - possibly an iterator? */
		bScreen *screen;

		for (screen = main->screen.first; screen; screen = screen->id.next) {
			ScrArea *sa;
			/* add regions */
			for (sa = screen->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl = sa->spacedata.first;
				if (sl->spacetype == SPACE_IMAGE) {
					ARegion *ar;
					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						if (ar->regiontype == RGN_TYPE_WINDOW) {
							View2D *v2d = &ar->v2d;
							v2d->minzoom = v2d->maxzoom = v2d->scroll = v2d->keeptot = v2d->keepzoom = v2d->keepofs = v2d->align = 0;
						}
					}
				}

				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_IMAGE) {
						ARegion *ar;
						for (ar = sl->regionbase.first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								View2D *v2d = &ar->v2d;
								v2d->minzoom = v2d->maxzoom = v2d->scroll = v2d->keeptot = v2d->keepzoom = v2d->keepofs = v2d->align = 0;
							}
						}
					}
				}
			}
		}

		{
			/* Initialize texture point density curve falloff */
			Tex *tex;
			for (tex = main->tex.first; tex; tex = tex->id.next) {
				if (tex->pd) {
					if (tex->pd->falloff_speed_scale == 0.0f)
						tex->pd->falloff_speed_scale = 100.0f;

					if (!tex->pd->falloff_curve) {
						tex->pd->falloff_curve = curvemapping_add(1, 0, 0, 1, 1);

						tex->pd->falloff_curve->preset = CURVE_PRESET_LINE;
						tex->pd->falloff_curve->cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
						curvemap_reset(tex->pd->falloff_curve->cm, &tex->pd->falloff_curve->clipr, tex->pd->falloff_curve->preset, CURVEMAP_SLOPE_POSITIVE);
						curvemapping_changed(tex->pd->falloff_curve, 0);
					}
				}
			}
		}

		{
			/* add default value for behind strength of camera actuator */
			Object *ob;
			bActuator *act;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				for (act = ob->actuators.first; act; act = act->next) {
					if (act->type == ACT_CAMERA) {
						bCameraActuator *ba = act->data;

						ba->damping = 1.0/32.0;
					}
				}
			}
		}

		{
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				/* Initialize particle billboard scale */
				part->bb_size[0] = part->bb_size[1] = 1.0f;
			}
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 1)) {
		{
			Scene *scene;
			Sequence *seq;

			for (scene = main->scene.first; scene; scene = scene->id.next) {
				scene->r.ffcodecdata.audio_channels = 2;
				scene->audio.volume = 1.0f;
				SEQ_BEGIN (scene->ed, seq)
				{
					seq->pitch = 1.0f;
				}
				SEQ_END
			}
		}

		{
			bScreen *screen;
			for (screen = main->screen.first; screen; screen = screen->id.next) {
				ScrArea *sa;

				/* add regions */
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					SpaceLink *sl = sa->spacedata.first;
					if (sl->spacetype == SPACE_SEQ) {
						ARegion *ar;
						for (ar = sa->regionbase.first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								if (ar->v2d.min[1] == 4.0f)
									ar->v2d.min[1] = 0.5f;
							}
						}
					}
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_SEQ) {
							ARegion *ar;
							for (ar = sl->regionbase.first; ar; ar = ar->next) {
								if (ar->regiontype == RGN_TYPE_WINDOW) {
									if (ar->v2d.min[1] == 4.0f)
										ar->v2d.min[1] = 0.5f;
								}
							}
						}
					}
				}
			}
		}

		{
			/* Make "auto-clamped" handles a per-keyframe setting instead of per-FCurve
			 *
			 * We're only patching F-Curves in Actions here, since it is assumed that most
			 * drivers out there won't be using this (and if they are, they're in the minority).
			 * While we should aim to fix everything ideally, in practice it's far too hard
			 * to get to every animdata block, not to mention the performance hit that'd have
			 */
			bAction *act;
			FCurve *fcu;

			for (act = main->action.first; act; act = act->id.next) {
				for (fcu = act->curves.first; fcu; fcu = fcu->next) {
					BezTriple *bezt;
					unsigned int i = 0;

					/* only need to touch curves that had this flag set */
					if ((fcu->flag & FCURVE_AUTO_HANDLES) == 0)
						continue;
					if ((fcu->totvert == 0) || (fcu->bezt == NULL))
						continue;

					/* only change auto-handles to auto-clamped */
					for (bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
						if (bezt->h1 == HD_AUTO)
							bezt->h1 = HD_AUTO_ANIM;
						if (bezt->h2 == HD_AUTO)
							bezt->h2 = HD_AUTO_ANIM;
					}

					fcu->flag &= ~FCURVE_AUTO_HANDLES;
				}
			}
		}

		{
			/* convert fcurve and shape action actuators to action actuators */
			Object *ob;
			bActuator *act;
			bIpoActuator *ia;
			bActionActuator *aa;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				for (act = ob->actuators.first; act; act = act->next) {
					if (act->type == ACT_IPO) {
						/* Create the new actuator */
						ia = act->data;
						aa = MEM_callocN(sizeof(bActionActuator), "fcurve -> action actuator do_version");

						/* Copy values */
						aa->type = ia->type;
						aa->flag = ia->flag;
						aa->sta = ia->sta;
						aa->end = ia->end;
						BLI_strncpy(aa->name, ia->name, sizeof(aa->name));
						BLI_strncpy(aa->frameProp, ia->frameProp, sizeof(aa->frameProp));
						if (ob->adt)
							aa->act = ob->adt->action;

						/* Get rid of the old actuator */
						MEM_freeN(ia);

						/* Assign the new actuator */
						act->data = aa;
						act->type = act->otype = ACT_ACTION;

						/* Fix for converting 2.4x files: if we don't have an action, but we have an
						   object IPO, then leave the actuator as an IPO actuator for now and let the
						   IPO conversion code handle it */
						if (ob->ipo && !aa->act)
							act->type = ACT_IPO;
					}
					else if (act->type == ACT_SHAPEACTION) {
						act->type = act->otype = ACT_ACTION;
					}
				}
			}
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 2)) {
		{
			/* Convert default socket values from bNodeStack */
			Scene *sce;
			Material *mat;
			Tex *tex;
			bNodeTree *ntree;

			for (ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
				blo_do_versions_nodetree_default_value(ntree);
				ntree->update |= NTREE_UPDATE;
			}

			for (sce = main->scene.first; sce; sce = sce->id.next)
				if (sce->nodetree) {
					blo_do_versions_nodetree_default_value(sce->nodetree);
					sce->nodetree->update |= NTREE_UPDATE;
				}

			for (mat = main->mat.first; mat; mat = mat->id.next)
				if (mat->nodetree) {
					blo_do_versions_nodetree_default_value(mat->nodetree);
					mat->nodetree->update |= NTREE_UPDATE;
				}

			for (tex = main->tex.first; tex; tex = tex->id.next)
				if (tex->nodetree) {
					blo_do_versions_nodetree_default_value(tex->nodetree);
					tex->nodetree->update |= NTREE_UPDATE;
				}
		}

		/* add SOCK_DYNAMIC flag to existing group sockets */
		{
			bNodeTree *ntree;
			/* only need to do this for trees in main, local trees are not used as groups */
			for (ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
				do_versions_nodetree_dynamic_sockets(ntree);
				ntree->update |= NTREE_UPDATE;
			}
		}

		{
			/* Initialize group tree nodetypes.
			 * These are used to distinguish tree types and
			 * associate them with specific node types for polling.
			 */
			bNodeTree *ntree;
			/* all node trees in main->nodetree are considered groups */
			for (ntree = main->nodetree.first; ntree; ntree = ntree->id.next)
				ntree->nodetype = NODE_GROUP;
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 4)) {
		{
			/* Adaptive time step for particle systems */
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				part->courant_target = 0.2f;
				part->time_flag &= ~PART_TIME_AUTOSF;
			}
		}

		{
			/* set defaults for obstacle avoidance, recast data */
			Scene *sce;
			for (sce = main->scene.first; sce; sce = sce->id.next) {
				if (sce->gm.levelHeight == 0.f)
					sce->gm.levelHeight = 2.f;

				if (sce->gm.recastData.cellsize == 0.0f)
					sce->gm.recastData.cellsize = 0.3f;
				if (sce->gm.recastData.cellheight == 0.0f)
					sce->gm.recastData.cellheight = 0.2f;
				if (sce->gm.recastData.agentmaxslope == 0.0f)
					sce->gm.recastData.agentmaxslope = (float)M_PI/4;
				if (sce->gm.recastData.agentmaxclimb == 0.0f)
					sce->gm.recastData.agentmaxclimb = 0.9f;
				if (sce->gm.recastData.agentheight == 0.0f)
					sce->gm.recastData.agentheight = 2.0f;
				if (sce->gm.recastData.agentradius == 0.0f)
					sce->gm.recastData.agentradius = 0.6f;
				if (sce->gm.recastData.edgemaxlen == 0.0f)
					sce->gm.recastData.edgemaxlen = 12.0f;
				if (sce->gm.recastData.edgemaxerror == 0.0f)
					sce->gm.recastData.edgemaxerror = 1.3f;
				if (sce->gm.recastData.regionminsize == 0.0f)
					sce->gm.recastData.regionminsize = 8.f;
				if (sce->gm.recastData.regionmergesize == 0.0f)
					sce->gm.recastData.regionmergesize = 20.f;
				if (sce->gm.recastData.vertsperpoly<3)
					sce->gm.recastData.vertsperpoly = 6;
				if (sce->gm.recastData.detailsampledist == 0.0f)
					sce->gm.recastData.detailsampledist = 6.0f;
				if (sce->gm.recastData.detailsamplemaxerror == 0.0f)
					sce->gm.recastData.detailsamplemaxerror = 1.0f;
			}
		}
	}
}
