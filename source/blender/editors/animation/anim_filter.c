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
 * The Original Code is Copyright (C) 2008 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 *
 * Contributor(s): Joshua Leung (original author)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_filter.c
 *  \ingroup edanimation
 */


/* This file contains a system used to provide a layer of abstraction between sources
 * of animation data and tools in Animation Editors. The method used here involves
 * generating a list of edit structures which enable tools to naively perform the actions
 * they require without all the boiler-plate associated with loops within loops and checking
 * for cases to ignore.
 *
 * While this is primarily used for the Action/Dopesheet Editor (and its accessory modes),
 * the Graph Editor also uses this for its channel list and for determining which curves
 * are being edited. Likewise, the NLA Editor also uses this for its channel list and in
 * its operators.
 *
 * Note: much of the original system this was based on was built before the creation of the RNA
 * system. In future, it would be interesting to replace some parts of this code with RNA queries,
 * however, RNA does not eliminate some of the boiler-plate reduction benefits presented by this
 * system, so if any such work does occur, it should only be used for the internals used here...
 *
 * -- Joshua Leung, Dec 2008 (Last revision July 2009)
 */

#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_key_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_world_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"
#include "DNA_layer_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_mask.h"
#include "BKE_sequencer.h"

#include "ED_anim_api.h"
#include "ED_markers.h"

#include "UI_resources.h"  /* for TH_KEYFRAME_SCALE lookup */

/* ************************************************************ */
/* Blender Context <-> Animation Context mapping */

/* ----------- Private Stuff - General -------------------- */

/* Get vertical scaling factor (i.e. typically used for keyframe size) */
static void animedit_get_yscale_factor(bAnimContext *ac)
{
	bTheme *btheme = UI_GetTheme();

	/* grab scale factor directly from action editor setting
	 * NOTE: This theme setting doesn't have an ID, as it cannot be accessed normally
	 *       since it is a float, and the theme settings methods can only handle chars.
	 */
	ac->yscale_fac = btheme->tact.keyframe_scale_fac;

	/* clamp to avoid problems with uninitialised values... */
	if (ac->yscale_fac < 0.1f)
		ac->yscale_fac = 1.0f;
	//printf("yscale_fac = %f\n", ac->yscale_fac);
}

/* ----------- Private Stuff - Action Editor ------------- */

/* Get shapekey data being edited (for Action Editor -> ShapeKey mode) */
/* Note: there's a similar function in key.c (BKE_key_from_object) */
static Key *actedit_get_shapekeys(bAnimContext *ac)
{
	ViewLayer *view_layer = ac->view_layer;
	Object *ob;
	Key *key;

	ob = OBACT(view_layer);
	if (ob == NULL)
		return NULL;

	/* XXX pinning is not available in 'ShapeKey' mode... */
	//if (saction->pin) return NULL;

	/* shapekey data is stored with geometry data */
	key = BKE_key_from_object(ob);

	if (key) {
		if (key->type == KEY_RELATIVE)
			return key;
	}

	return NULL;
}

/* Get data being edited in Action Editor (depending on current 'mode') */
static bool actedit_get_context(bAnimContext *ac, SpaceAction *saction)
{
	/* get dopesheet */
	ac->ads = &saction->ads;

	/* sync settings with current view status, then return appropriate data */
	switch (saction->mode) {
		case SACTCONT_ACTION: /* 'Action Editor' */
			/* if not pinned, sync with active object */
			if (/*saction->pin == 0*/ true) {
				if (ac->obact && ac->obact->adt)
					saction->action = ac->obact->adt->action;
				else
					saction->action = NULL;
			}

			ac->datatype = ANIMCONT_ACTION;
			ac->data = saction->action;

			ac->mode = saction->mode;
			return true;

		case SACTCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			ac->datatype = ANIMCONT_SHAPEKEY;
			ac->data = actedit_get_shapekeys(ac);

			/* if not pinned, sync with active object */
			if (/*saction->pin == 0*/ true) {
				Key *key = (Key *)ac->data;

				if (key && key->adt)
					saction->action = key->adt->action;
				else
					saction->action = NULL;
			}

			ac->mode = saction->mode;
			return true;

		case SACTCONT_GPENCIL: /* Grease Pencil */ /* XXX review how this mode is handled... */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source = (ID *)ac->scene;

			ac->datatype = ANIMCONT_GPENCIL;
			ac->data = &saction->ads;

			ac->mode = saction->mode;
			return true;

		case SACTCONT_CACHEFILE: /* Cache File */ /* XXX review how this mode is handled... */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source = (ID *)ac->scene;

			ac->datatype = ANIMCONT_CHANNEL;
			ac->data = &saction->ads;

			ac->mode = saction->mode;
			return true;

		case SACTCONT_MASK: /* Mask */ /* XXX review how this mode is handled... */
		{
			/* TODO, other methods to get the mask */
			// Sequence *seq = BKE_sequencer_active_get(ac->scene);
			//MovieClip *clip = ac->scene->clip;
//			struct Mask *mask = seq ? seq->mask : NULL;

			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source = (ID *)ac->scene;

			ac->datatype = ANIMCONT_MASK;
			ac->data = &saction->ads;

			ac->mode = saction->mode;
			return true;
		}

		case SACTCONT_DOPESHEET: /* DopeSheet */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source = (ID *)ac->scene;

			ac->datatype = ANIMCONT_DOPESHEET;
			ac->data = &saction->ads;

			ac->mode = saction->mode;
			return true;

		case SACTCONT_TIMELINE: /* Timeline */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source = (ID *)ac->scene;

			/* sync scene's "selected keys only" flag with our "only selected" flag
			 * XXX: This is a workaround for T55525. We shouldn't really be syncing the flags like this,
			 *      but it's a simpler fix for now than also figuring out how the next/prev keyframe tools
			 *      should work in the 3D View if we allowed full access to the timeline's dopesheet filters
			 *      (i.e. we'd have to figure out where to host those settings, to be on a scene level like
			 *      this flag currently is, along with several other unknowns)
			 */
			if (ac->scene->flag & SCE_KEYS_NO_SELONLY)
				saction->ads.filterflag &= ~ADS_FILTER_ONLYSEL;
			else
				saction->ads.filterflag |= ADS_FILTER_ONLYSEL;

			ac->datatype = ANIMCONT_TIMELINE;
			ac->data = &saction->ads;

			ac->mode = saction->mode;
			return true;

		default: /* unhandled yet */
			ac->datatype = ANIMCONT_NONE;
			ac->data = NULL;

			ac->mode = -1;
			return false;
	}
}

/* ----------- Private Stuff - Graph Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static bool graphedit_get_context(bAnimContext *ac, SpaceIpo *sipo)
{
	/* init dopesheet data if non-existent (i.e. for old files) */
	if (sipo->ads == NULL) {
		sipo->ads = MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
		sipo->ads->source = (ID *)ac->scene;
	}
	ac->ads = sipo->ads;

	/* set settings for Graph Editor - "Selected = Editable" */
	if (sipo->flag & SIPO_SELCUVERTSONLY)
		sipo->ads->filterflag |= ADS_FILTER_SELEDIT;
	else
		sipo->ads->filterflag &= ~ADS_FILTER_SELEDIT;

	/* sync settings with current view status, then return appropriate data */
	switch (sipo->mode) {
		case SIPO_MODE_ANIMATION:  /* Animation F-Curve Editor */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			sipo->ads->source = (ID *)ac->scene;
			sipo->ads->filterflag &= ~ADS_FILTER_ONLYDRIVERS;

			ac->datatype = ANIMCONT_FCURVES;
			ac->data = sipo->ads;

			ac->mode = sipo->mode;
			return true;

		case SIPO_MODE_DRIVERS:  /* Driver F-Curve Editor */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			sipo->ads->source = (ID *)ac->scene;
			sipo->ads->filterflag |= ADS_FILTER_ONLYDRIVERS;

			ac->datatype = ANIMCONT_DRIVERS;
			ac->data = sipo->ads;

			ac->mode = sipo->mode;
			return true;

		default: /* unhandled yet */
			ac->datatype = ANIMCONT_NONE;
			ac->data = NULL;

			ac->mode = -1;
			return false;
	}
}

/* ----------- Private Stuff - NLA Editor ------------- */

/* Get data being edited in Graph Editor (depending on current 'mode') */
static bool nlaedit_get_context(bAnimContext *ac, SpaceNla *snla)
{
	/* init dopesheet data if non-existent (i.e. for old files) */
	if (snla->ads == NULL)
		snla->ads = MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");
	ac->ads = snla->ads;

	/* sync settings with current view status, then return appropriate data */
	/* update scene-pointer (no need to check for pinning yet, as not implemented) */
	snla->ads->source = (ID *)ac->scene;
	snla->ads->filterflag |= ADS_FILTER_ONLYNLA;

	ac->datatype = ANIMCONT_NLA;
	ac->data = snla->ads;

	return true;
}

/* ----------- Public API --------------- */

/* Obtain current anim-data context, given that context info from Blender context has already been set
 *	- AnimContext to write to is provided as pointer to var on stack so that we don't have
 *	  allocation/freeing costs (which are not that avoidable with channels).
 */
bool ANIM_animdata_context_getdata(bAnimContext *ac)
{
	SpaceLink *sl = ac->sl;
	bool ok = false;

	/* context depends on editor we are currently in */
	if (sl) {
		switch (ac->spacetype) {
			case SPACE_ACTION:
			{
				SpaceAction *saction = (SpaceAction *)sl;
				ok = actedit_get_context(ac, saction);
				break;
			}
			case SPACE_IPO:
			{
				SpaceIpo *sipo = (SpaceIpo *)sl;
				ok = graphedit_get_context(ac, sipo);
				break;
			}
			case SPACE_NLA:
			{
				SpaceNla *snla = (SpaceNla *)sl;
				ok = nlaedit_get_context(ac, snla);
				break;
			}
		}
	}

	/* check if there's any valid data */
	return (ok && ac->data);
}

/* Obtain current anim-data context from Blender Context info
 *	- AnimContext to write to is provided as pointer to var on stack so that we don't have
 *	  allocation/freeing costs (which are not that avoidable with channels).
 *	- Clears data and sets the information from Blender Context which is useful
 */
bool ANIM_animdata_get_context(const bContext *C, bAnimContext *ac)
{
	Main *bmain = CTX_data_main(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	SpaceLink *sl = CTX_wm_space_data(C);
	Scene *scene = CTX_data_scene(C);

	/* clear old context info */
	if (ac == NULL) return false;
	memset(ac, 0, sizeof(bAnimContext));

	/* get useful default context settings from context */
	ac->bmain = bmain;
	ac->scene = scene;
	if (scene) {
		ac->markers = ED_context_get_markers(C);
	}
	ac->depsgraph = CTX_data_depsgraph(C);
	ac->view_layer = CTX_data_view_layer(C);
	ac->obact = (ac->view_layer->basact) ? ac->view_layer->basact->object : NULL;
	ac->sa = sa;
	ac->ar = ar;
	ac->sl = sl;
	ac->spacetype = (sa) ? sa->spacetype : 0;
	ac->regiontype = (ar) ? ar->regiontype : 0;

	/* initialise default y-scale factor */
	animedit_get_yscale_factor(ac);

	/* get data context info */
	// XXX: if the below fails, try to grab this info from context instead... (to allow for scripting)
	return ANIM_animdata_context_getdata(ac);
}

/* ************************************************************ */
/* Blender Data <-- Filter --> Channels to be operated on */

/* macros to use before/after getting the sub-channels of some channel,
 * to abstract away some of the tricky logic involved
 *
 * cases:
 *	1) Graph Edit main area (just data) OR channels visible in Channel List
 *	2) If not showing channels, we're only interested in the data (Action Editor's editing)
 *	3) We don't care what data, we just care there is some (so that a collapsed
 *	   channel can be kept around). No need to clear channels-flag in order to
 *	   keep expander channels with no sub-data out, as those cases should get
 *	   dealt with by the recursive detection idiom in place.
 *
 * Implementation Note:
 *  YES the _doSubChannels variable is NOT read anywhere. BUT, this is NOT an excuse
 *  to go steamrolling the logic into a single-line expression as from experience,
 *  those are notoriously difficult to read + debug when extending later on. The code
 *  below is purposefully laid out so that each case noted above corresponds clearly to
 *  one case below.
 */
#define BEGIN_ANIMFILTER_SUBCHANNELS(expanded_check) \
	{ \
		int _filter = filter_mode; \
		short _doSubChannels = 0; \
		if (!(filter_mode & ANIMFILTER_LIST_VISIBLE) || (expanded_check)) \
			_doSubChannels = 1; \
		else if (!(filter_mode & ANIMFILTER_LIST_CHANNELS)) \
			_doSubChannels = 2; \
		else { \
			filter_mode |= ANIMFILTER_TMP_PEEK; \
		} \
		 \
		{ \
			(void) _doSubChannels; \
		}
/* ... standard sub-channel filtering can go on here now ... */
#define END_ANIMFILTER_SUBCHANNELS \
		filter_mode = _filter; \
	} (void)0

/* ............................... */

/* quick macro to test if AnimData is usable */
#define ANIMDATA_HAS_KEYS(id) ((id)->adt && (id)->adt->action)

/* quick macro to test if AnimData is usable for drivers */
#define ANIMDATA_HAS_DRIVERS(id) ((id)->adt && (id)->adt->drivers.first)

/* quick macro to test if AnimData is usable for NLA */
#define ANIMDATA_HAS_NLA(id) ((id)->adt && (id)->adt->nla_tracks.first)

/* Quick macro to test for all three above usability tests, performing the appropriate provided
 * action for each when the AnimData context is appropriate.
 *
 * Priority order for this goes (most important, to least): AnimData blocks, NLA, Drivers, Keyframes.
 *
 * For this to work correctly, a standard set of data needs to be available within the scope that this
 * gets called in:
 *  - ListBase anim_data;
 *  - bDopeSheet *ads;
 *  - bAnimListElem *ale;
 *  - size_t items;
 *
 *  - id: ID block which should have an AnimData pointer following it immediately, to use
 *  - adtOk: line or block of code to execute for AnimData-blocks case (usually ANIMDATA_ADD_ANIMDATA)
 *  - nlaOk: line or block of code to execute for NLA tracks+strips case
 *  - driversOk: line or block of code to execute for Drivers case
 *  - nlaKeysOk: line or block of code for NLA Strip Keyframes case
 *  - keysOk: line or block of code for Keyframes case
 *
 * The checks for the various cases are as follows:
 *	0) top level: checks for animdata and also that all the F-Curves for the block will be visible
 *	1) animdata check: for filtering animdata blocks only
 *	2A) nla tracks: include animdata block's data as there are NLA tracks+strips there
 *	2B) actions to convert to nla: include animdata block's data as there is an action that can be
 *		converted to a new NLA strip, and the filtering options allow this
 *	2C) allow non-animated datablocks to be included so that datablocks can be added
 *	3) drivers: include drivers from animdata block (for Drivers mode in Graph Editor)
 *  4A) nla strip keyframes: these are the per-strip controls for time and influence
 *	4B) normal keyframes: only when there is an active action
 */
#define ANIMDATA_FILTER_CASES(id, adtOk, nlaOk, driversOk, nlaKeysOk, keysOk) \
	{ \
		if ((id)->adt) { \
			if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || !((id)->adt->flag & ADT_CURVES_NOT_VISIBLE)) { \
				if (filter_mode & ANIMFILTER_ANIMDATA) { \
					adtOk \
				} \
				else if (ads->filterflag & ADS_FILTER_ONLYNLA) { \
					if (ANIMDATA_HAS_NLA(id)) { \
						nlaOk \
					} \
					else if (!(ads->filterflag & ADS_FILTER_NLA_NOACT) || ANIMDATA_HAS_KEYS(id)) { \
						nlaOk \
					} \
				} \
				else if (ads->filterflag & ADS_FILTER_ONLYDRIVERS) { \
					if (ANIMDATA_HAS_DRIVERS(id)) { \
						driversOk \
					} \
				} \
				else { \
					if (ANIMDATA_HAS_NLA(id)) { \
						nlaKeysOk \
					} \
					if (ANIMDATA_HAS_KEYS(id)) { \
						keysOk \
					} \
				} \
			} \
		} \
	} (void)0

/* ............................... */

/* Add a new animation channel, taking into account the "peek" flag, which is used to just check
 * whether any channels will be added (but without needing them to actually get created).
 *
 * ! This causes the calling function to return early if we're only "peeking" for channels
 */
// XXX: ale_statement stuff is really a hack for one special case. It shouldn't really be needed...
#define ANIMCHANNEL_NEW_CHANNEL_FULL(channel_data, channel_type, owner_id, ale_statement) \
	if (filter_mode & ANIMFILTER_TMP_PEEK) \
		return 1; \
	else { \
		bAnimListElem *ale = make_new_animlistelem(channel_data, channel_type, (ID *)owner_id); \
		if (ale) { \
			BLI_addtail(anim_data, ale); \
			items ++; \
			ale_statement \
		} \
	} (void)0

#define ANIMCHANNEL_NEW_CHANNEL(channel_data, channel_type, owner_id) \
	ANIMCHANNEL_NEW_CHANNEL_FULL(channel_data, channel_type, owner_id, {})

/* ............................... */

/* quick macro to test if an anim-channel representing an AnimData block is suitably active */
#define ANIMCHANNEL_ACTIVEOK(ale) \
	(!(filter_mode & ANIMFILTER_ACTIVE) || !(ale->adt) || (ale->adt->flag & ADT_UI_ACTIVE) )

/* quick macro to test if an anim-channel (F-Curve, Group, etc.) is selected in an acceptable way */
#define ANIMCHANNEL_SELOK(test_func) \
	(!(filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)) || \
	 ((filter_mode & ANIMFILTER_SEL) && test_func) || \
	 ((filter_mode & ANIMFILTER_UNSEL) && test_func == 0) )

/* quick macro to test if an anim-channel (F-Curve) is selected ok for editing purposes
 *	- _SELEDIT means that only selected curves will have visible+editable keyframes
 *
 * checks here work as follows:
 *	1) seledit off - don't need to consider the implications of this option
 *	2) foredit off - we're not considering editing, so channel is ok still
 *	3) test_func (i.e. selection test) - only if selected, this test will pass
 */
#define ANIMCHANNEL_SELEDITOK(test_func) \
	(!(filter_mode & ANIMFILTER_SELEDIT) || \
	 !(filter_mode & ANIMFILTER_FOREDIT) || \
	 (test_func) )

/* ----------- 'Private' Stuff --------------- */

/* this function allocates memory for a new bAnimListElem struct for the
 * provided animation channel-data.
 */
static bAnimListElem *make_new_animlistelem(void *data, short datatype, ID *owner_id)
{
	bAnimListElem *ale = NULL;

	/* only allocate memory if there is data to convert */
	if (data) {
		/* allocate and set generic data */
		ale = MEM_callocN(sizeof(bAnimListElem), "bAnimListElem");

		ale->data = data;
		ale->type = datatype;

		ale->id = owner_id;
		ale->adt = BKE_animdata_from_id(owner_id);

		/* do specifics */
		switch (datatype) {
			case ANIMTYPE_SUMMARY:
			{
				/* nothing to include for now... this is just a dummy wrappy around all the other channels
				 * in the DopeSheet, and gets included at the start of the list
				 */
				ale->key_data = NULL;
				ale->datatype = ALE_ALL;
				break;
			}
			case ANIMTYPE_SCENE:
			{
				Scene *sce = (Scene *)data;

				ale->flag = sce->flag;

				ale->key_data = sce;
				ale->datatype = ALE_SCE;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_OBJECT:
			{
				Base *base = (Base *)data;
				Object *ob = base->object;

				ale->flag = ob->flag;

				ale->key_data = ob;
				ale->datatype = ALE_OB;

				ale->adt = BKE_animdata_from_id(&ob->id);
				break;
			}
			case ANIMTYPE_FILLACTD:
			{
				bAction *act = (bAction *)data;

				ale->flag = act->flag;

				ale->key_data = act;
				ale->datatype = ALE_ACT;
				break;
			}
			case ANIMTYPE_FILLDRIVERS:
			{
				AnimData *adt = (AnimData *)data;

				ale->flag = adt->flag;

				// XXX... drivers don't show summary for now
				ale->key_data = NULL;
				ale->datatype = ALE_NONE;
				break;
			}
			case ANIMTYPE_DSMAT:
			{
				Material *ma = (Material *)data;
				AnimData *adt = ma->adt;

				ale->flag = FILTER_MAT_OBJD(ma);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSLAM:
			{
				Lamp *la = (Lamp *)data;
				AnimData *adt = la->adt;

				ale->flag = FILTER_LAM_OBJD(la);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSCAM:
			{
				Camera *ca = (Camera *)data;
				AnimData *adt = ca->adt;

				ale->flag = FILTER_CAM_OBJD(ca);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSCACHEFILE:
			{
				CacheFile *cache_file = (CacheFile *)data;
				AnimData *adt = cache_file->adt;

				ale->flag = FILTER_CACHEFILE_OBJD(cache_file);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSCUR:
			{
				Curve *cu = (Curve *)data;
				AnimData *adt = cu->adt;

				ale->flag = FILTER_CUR_OBJD(cu);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSARM:
			{
				bArmature *arm = (bArmature *)data;
				AnimData *adt = arm->adt;

				ale->flag = FILTER_ARM_OBJD(arm);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSMESH:
			{
				Mesh *me = (Mesh *)data;
				AnimData *adt = me->adt;

				ale->flag = FILTER_MESH_OBJD(me);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSLAT:
			{
				Lattice *lt = (Lattice *)data;
				AnimData *adt = lt->adt;

				ale->flag = FILTER_LATTICE_OBJD(lt);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSSPK:
			{
				Speaker *spk = (Speaker *)data;
				AnimData *adt = spk->adt;

				ale->flag = FILTER_SPK_OBJD(spk);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSSKEY:
			{
				Key *key = (Key *)data;
				AnimData *adt = key->adt;

				ale->flag = FILTER_SKE_OBJD(key);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSWOR:
			{
				World *wo = (World *)data;
				AnimData *adt = wo->adt;

				ale->flag = FILTER_WOR_SCED(wo);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSNTREE:
			{
				bNodeTree *ntree = (bNodeTree *)data;
				AnimData *adt = ntree->adt;

				ale->flag = FILTER_NTREE_DATA(ntree);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSLINESTYLE:
			{
				FreestyleLineStyle *linestyle = (FreestyleLineStyle *)data;
				AnimData *adt = linestyle->adt;

				ale->flag = FILTER_LS_SCED(linestyle);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSPART:
			{
				ParticleSettings *part = (ParticleSettings *)ale->data;
				AnimData *adt = part->adt;

				ale->flag = FILTER_PART_OBJD(part);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSTEX:
			{
				Tex *tex = (Tex *)data;
				AnimData *adt = tex->adt;

				ale->flag = FILTER_TEX_DATA(tex);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSGPENCIL:
			{
				bGPdata *gpd = (bGPdata *)data;
				AnimData *adt = gpd->adt;

				/* NOTE: we just reuse the same expand filter for this case */
				ale->flag = EXPANDED_GPD(gpd);

				// XXX: currently, this is only used for access to its animation data
				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_DSMCLIP:
			{
				MovieClip *clip = (MovieClip *)data;
				AnimData *adt = clip->adt;

				ale->flag = EXPANDED_MCLIP(clip);

				ale->key_data = (adt) ? adt->action : NULL;
				ale->datatype = ALE_ACT;

				ale->adt = BKE_animdata_from_id(data);
				break;
			}
			case ANIMTYPE_NLACONTROLS:
			{
				AnimData *adt = (AnimData *)data;

				ale->flag = adt->flag;

				ale->key_data = NULL;
				ale->datatype = ALE_NONE;
				break;
			}
			case ANIMTYPE_GROUP:
			{
				bActionGroup *agrp = (bActionGroup *)data;

				ale->flag = agrp->flag;

				ale->key_data = NULL;
				ale->datatype = ALE_GROUP;
				break;
			}
			case ANIMTYPE_FCURVE:
			case ANIMTYPE_NLACURVE: /* practically the same as ANIMTYPE_FCURVE. Differences are applied post-creation */
			{
				FCurve *fcu = (FCurve *)data;

				ale->flag = fcu->flag;

				ale->key_data = fcu;
				ale->datatype = ALE_FCURVE;
				break;
			}
			case ANIMTYPE_SHAPEKEY:
			{
				KeyBlock *kb = (KeyBlock *)data;
				Key *key = (Key *)ale->id;

				ale->flag = kb->flag;

				/* whether we have keyframes depends on whether there is a Key block to find it from */
				if (key) {
					/* index of shapekey is defined by place in key's list */
					ale->index = BLI_findindex(&key->block, kb);

					/* the corresponding keyframes are from the animdata */
					if (ale->adt && ale->adt->action) {
						bAction *act = ale->adt->action;
						char *rna_path = BKE_keyblock_curval_rnapath_get(key, kb);

						/* try to find the F-Curve which corresponds to this exactly,
						 * then free the MEM_alloc'd string
						 */
						if (rna_path) {
							ale->key_data = (void *)list_find_fcurve(&act->curves, rna_path, 0);
							MEM_freeN(rna_path);
						}
					}
					ale->datatype = (ale->key_data) ? ALE_FCURVE : ALE_NONE;
				}
				break;
			}
			case ANIMTYPE_GPLAYER:
			{
				bGPDlayer *gpl = (bGPDlayer *)data;

				ale->flag = gpl->flag;

				ale->key_data = NULL;
				ale->datatype = ALE_GPFRAME;
				break;
			}
			case ANIMTYPE_MASKLAYER:
			{
				MaskLayer *masklay = (MaskLayer *)data;

				ale->flag = masklay->flag;

				ale->key_data = NULL;
				ale->datatype = ALE_MASKLAY;
				break;
			}
			case ANIMTYPE_NLATRACK:
			{
				NlaTrack *nlt = (NlaTrack *)data;

				ale->flag = nlt->flag;

				ale->key_data = &nlt->strips;
				ale->datatype = ALE_NLASTRIP;
				break;
			}
			case ANIMTYPE_NLAACTION:
			{
				/* nothing to include for now... nothing editable from NLA-perspective here */
				ale->key_data = NULL;
				ale->datatype = ALE_NONE;
				break;
			}
		}
	}

	/* return created datatype */
	return ale;
}

/* ----------------------------------------- */

/* 'Only Selected' selected data and/or 'Include Hidden' filtering
 * NOTE: when this function returns true, the F-Curve is to be skipped
 */
static bool skip_fcurve_selected_data(bDopeSheet *ads, FCurve *fcu, ID *owner_id, int filter_mode)
{
	if (fcu->grp != NULL && fcu->grp->flag & ADT_CURVES_ALWAYS_VISIBLE) {
		return false;
	}
	/* hidden items should be skipped if we only care about visible data, but we aren't interested in hidden stuff */
	const bool skip_hidden = (filter_mode & ANIMFILTER_DATA_VISIBLE) && !(ads->filterflag & ADS_FILTER_INCL_HIDDEN);

	if (GS(owner_id->name) == ID_OB) {
		Object *ob = (Object *)owner_id;

		/* only consider if F-Curve involves pose.bones */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "pose.bones")) {
			bPoseChannel *pchan;
			char *bone_name;

			/* get bone-name, and check if this bone is selected */
			bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
			pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
			if (bone_name) MEM_freeN(bone_name);

			/* check whether to continue or skip */
			if ((pchan) && (pchan->bone)) {
				/* if only visible channels, skip if bone not visible unless user wants channels from hidden data too */
				if (skip_hidden) {
					bArmature *arm = (bArmature *)ob->data;

					/* skipping - not visible on currently visible layers */
					if ((arm->layer & pchan->bone->layer) == 0)
						return true;
					/* skipping - is currently hidden */
					if (pchan->bone->flag & BONE_HIDDEN_P)
						return true;
				}

				/* can only add this F-Curve if it is selected */
				if (ads->filterflag & ADS_FILTER_ONLYSEL) {
					if ((pchan->bone->flag & BONE_SELECTED) == 0)
						return true;
				}
			}
		}
	}
	else if (GS(owner_id->name) == ID_SCE) {
		Scene *scene = (Scene *)owner_id;

		/* only consider if F-Curve involves sequence_editor.sequences */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "sequences_all")) {
			Editing *ed = BKE_sequencer_editing_get(scene, false);
			Sequence *seq = NULL;
			char *seq_name;

			if (ed) {
				/* get strip name, and check if this strip is selected */
				seq_name = BLI_str_quoted_substrN(fcu->rna_path, "sequences_all[");
				seq = BKE_sequence_get_by_name(ed->seqbasep, seq_name, false);
				if (seq_name) MEM_freeN(seq_name);
			}

			/* can only add this F-Curve if it is selected */
			if (ads->filterflag & ADS_FILTER_ONLYSEL) {
				if ((seq == NULL) || (seq->flag & SELECT) == 0)
					return true;
			}
		}
	}
	else if (GS(owner_id->name) == ID_NT) {
		bNodeTree *ntree = (bNodeTree *)owner_id;

		/* check for selected nodes */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "nodes")) {
			bNode *node;
			char *node_name;

			/* get strip name, and check if this strip is selected */
			node_name = BLI_str_quoted_substrN(fcu->rna_path, "nodes[");
			node = nodeFindNodebyName(ntree, node_name);
			if (node_name) MEM_freeN(node_name);

			/* can only add this F-Curve if it is selected */
			if (ads->filterflag & ADS_FILTER_ONLYSEL) {
				if ((node) && (node->flag & NODE_SELECT) == 0)
					return true;
			}
		}
	}

	return false;
}

/* Helper for name-based filtering - Perform "partial/fuzzy matches" (as in 80a7efd) */
static bool name_matches_dopesheet_filter(bDopeSheet *ads, char *name)
{
	if (ads->flag & ADS_FLAG_FUZZY_NAMES) {
		/* full fuzzy, multi-word, case insensitive matches */
		const size_t str_len = strlen(ads->searchstr);
		const int words_max = (str_len / 2) + 1;

		int (*words)[2] = BLI_array_alloca(words, words_max);
		const int words_len = BLI_string_find_split_words(ads->searchstr, str_len, ' ', words, words_max);
		bool found = false;

		/* match name against all search words */
		for (int index = 0; index < words_len; index++) {
			if (BLI_strncasestr(name, ads->searchstr + words[index][0], words[index][1])) {
				found = true;
				break;
			}
		}

		/* if we have a match somewhere, this returns true */
		return found;
	}
	else {
		/* fallback/default - just case insensitive, but starts from start of word */
		return BLI_strcasestr(name, ads->searchstr) != NULL;
	}
}

/* (Display-)Name-based F-Curve filtering
 * NOTE: when this function returns true, the F-Curve is to be skipped
 */
static bool skip_fcurve_with_name(bDopeSheet *ads, FCurve *fcu, eAnim_ChannelType channel_type, void *owner, ID *owner_id)
{
	bAnimListElem ale_dummy = {NULL};
	const bAnimChannelType *acf;

	/* create a dummy wrapper for the F-Curve, so we can get typeinfo for it */
	ale_dummy.type = channel_type;
	ale_dummy.owner = owner;
	ale_dummy.id = owner_id;
	ale_dummy.data = fcu;

	/* get type info for channel */
	acf = ANIM_channel_get_typeinfo(&ale_dummy);
	if (acf && acf->name) {
		char name[256]; /* hopefully this will be enough! */

		/* get name */
		acf->name(&ale_dummy, name);

		/* check for partial match with the match string, assuming case insensitive filtering
		 * if match, this channel shouldn't be ignored!
		 */
		return !name_matches_dopesheet_filter(ads, name);
	}

	/* just let this go... */
	return true;
}

/**
 * Check if F-Curve has errors and/or is disabled
 *
 * \return true if F-Curve has errors/is disabled
 */
static bool fcurve_has_errors(FCurve *fcu)
{
	/* F-Curve disabled - path eval error */
	if (fcu->flag & FCURVE_DISABLED) {
		return true;
	}

	/* driver? */
	if (fcu->driver) {
		ChannelDriver *driver = fcu->driver;
		DriverVar *dvar;

		/* error flag on driver usually means that there is an error
		 * BUT this may not hold with PyDrivers as this flag gets cleared
		 *     if no critical errors prevent the driver from working...
		 */
		if (driver->flag & DRIVER_FLAG_INVALID)
			return true;

		/* check variables for other things that need linting... */
		// TODO: maybe it would be more efficient just to have a quick flag for this?
		for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
			DRIVER_TARGETS_USED_LOOPER(dvar)
			{
				if (dtar->flag & DTAR_FLAG_INVALID)
					return true;
			}
			DRIVER_TARGETS_LOOPER_END
		}
	}

	/* no errors found */
	return false;
}

/* find the next F-Curve that is usable for inclusion */
static FCurve *animfilter_fcurve_next(bDopeSheet *ads, FCurve *first, eAnim_ChannelType channel_type, int filter_mode, void *owner, ID *owner_id)
{
	bActionGroup *grp = (channel_type == ANIMTYPE_FCURVE) ? owner : NULL;
	FCurve *fcu = NULL;

	/* loop over F-Curves - assume that the caller of this has already checked that these should be included
	 * NOTE: we need to check if the F-Curves belong to the same group, as this gets called for groups too...
	 */
	for (fcu = first; ((fcu) && (fcu->grp == grp)); fcu = fcu->next) {
		/* special exception for Pose-Channel/Sequence-Strip/Node Based F-Curves:
		 *	- the 'Only Selected' and 'Include Hidden' data filters should be applied to sub-ID data which
		 *	  can be independently selected/hidden, such as Pose-Channels, Sequence Strips, and Nodes.
		 *	  Since these checks were traditionally done as first check for objects, we do the same here
		 *	- we currently use an 'approximate' method for getting these F-Curves that doesn't require
		 *	  carefully checking the entire path
		 *	- this will also affect things like Drivers, and also works for Bone Constraints
		 */
		if (ads && owner_id) {
			if ((filter_mode & ANIMFILTER_TMP_IGNORE_ONLYSEL) == 0) {
				if ((ads->filterflag & ADS_FILTER_ONLYSEL) || (ads->filterflag & ADS_FILTER_INCL_HIDDEN) == 0) {
					if (skip_fcurve_selected_data(ads, fcu, owner_id, filter_mode))
						continue;
				}
			}
		}

		/* only include if visible (Graph Editor check, not channels check) */
		if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || (fcu->flag & FCURVE_VISIBLE)) {
			/* only work with this channel and its subchannels if it is editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_FCU(fcu)) {
				/* only include this curve if selected in a way consistent with the filtering requirements */
				if (ANIMCHANNEL_SELOK(SEL_FCU(fcu)) && ANIMCHANNEL_SELEDITOK(SEL_FCU(fcu))) {
					/* only include if this curve is active */
					if (!(filter_mode & ANIMFILTER_ACTIVE) || (fcu->flag & FCURVE_ACTIVE)) {
						/* name based filtering... */
						if ( ((ads) && (ads->searchstr[0] != '\0')) && (owner_id) ) {
							if (skip_fcurve_with_name(ads, fcu, channel_type, owner, owner_id))
								continue;
						}

						/* error-based filtering... */
						if ((ads) && (ads->filterflag & ADS_FILTER_ONLY_ERRORS)) {
							/* skip if no errors... */
							if (fcurve_has_errors(fcu) == false)
								continue;
						}

						/* this F-Curve can be used, so return it */
						return fcu;
					}
				}
			}
		}
	}

	/* no (more) F-Curves from the list are suitable... */
	return NULL;
}

static size_t animfilter_fcurves(ListBase *anim_data, bDopeSheet *ads,
                                 FCurve *first, eAnim_ChannelType fcurve_type,
                                 int filter_mode,
                                 void *owner, ID *owner_id)
{
	FCurve *fcu;
	size_t items = 0;

	/* loop over every F-Curve able to be included
	 *	- this for-loop works like this:
	 *		1) the starting F-Curve is assigned to the fcu pointer so that we have a starting point to search from
	 *		2) the first valid F-Curve to start from (which may include the one given as 'first') in the remaining
	 *		   list of F-Curves is found, and verified to be non-null
	 *		3) the F-Curve referenced by fcu pointer is added to the list
	 *		4) the fcu pointer is set to the F-Curve after the one we just added, so that we can keep going through
	 *		   the rest of the F-Curve list without an eternal loop. Back to step 2 :)
	 */
	for (fcu = first; ( (fcu = animfilter_fcurve_next(ads, fcu, fcurve_type, filter_mode, owner, owner_id)) ); fcu = fcu->next) {
		if (UNLIKELY(fcurve_type == ANIMTYPE_NLACURVE)) {
			/* NLA Control Curve - Basically the same as normal F-Curves, except we need to set some stuff differently */
			ANIMCHANNEL_NEW_CHANNEL_FULL(fcu, ANIMTYPE_NLACURVE, owner_id, {
				ale->owner = owner; /* strip */
				ale->adt = NULL;    /* to prevent time mapping from causing problems */
			});
		}
		else {
			/* Normal FCurve */
			ANIMCHANNEL_NEW_CHANNEL(fcu, ANIMTYPE_FCURVE, owner_id);
		}
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animfilter_act_group(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, bAction *UNUSED(act), bActionGroup *agrp, int filter_mode, ID *owner_id)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;
	//int ofilter = filter_mode;

	/* if we care about the selection status of the channels,
	 * but the group isn't expanded (1)...
	 *  (1) this only matters if we actually care about the hierarchy though.
	 *		- Hierarchy matters: this hack should be applied
	 *		- Hierarchy ignored: cases like [#21276] won't work properly, unless we skip this hack
	 */
	if ( ((filter_mode & ANIMFILTER_LIST_VISIBLE) && EXPANDED_AGRP(ac, agrp) == 0) &&     /* care about hierarchy but group isn't expanded */
	     (filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)) )                          /* care about selection status */
	{
		/* if the group itself isn't selected appropriately, we shouldn't consider it's children either */
		if (ANIMCHANNEL_SELOK(SEL_AGRP(agrp)) == 0)
			return 0;

		/* if we're still here, then the selection status of the curves within this group should not matter,
		 * since this creates too much overhead for animators (i.e. making a slow workflow)
		 *
		 * Tools affected by this at time of coding (2010 Feb 09):
		 *	- inserting keyframes on selected channels only
		 *	- pasting keyframes
		 *	- creating ghost curves in Graph Editor
		 */
		filter_mode &= ~(ANIMFILTER_SEL | ANIMFILTER_UNSEL | ANIMFILTER_LIST_VISIBLE);
	}

	/* add grouped F-Curves */
	BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_AGRP(ac, agrp))
	{
		/* special filter so that we can get just the F-Curves within the active group */
		if (!(filter_mode & ANIMFILTER_ACTGROUPED) || (agrp->flag & AGRP_ACTIVE)) {
			/* for the Graph Editor, curves may be set to not be visible in the view to lessen clutter,
			 * but to do this, we need to check that the group doesn't have it's not-visible flag set preventing
			 * all its sub-curves to be shown
			 */
			if (!(filter_mode & ANIMFILTER_CURVE_VISIBLE) || !(agrp->flag & AGRP_NOTVISIBLE)) {
				/* group must be editable for its children to be editable (if we care about this) */
				if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_AGRP(agrp)) {
					/* get first F-Curve which can be used here */
					FCurve *first_fcu = animfilter_fcurve_next(ads, agrp->channels.first, ANIMTYPE_FCURVE, filter_mode, agrp, owner_id);

					/* filter list, starting from this F-Curve */
					tmp_items += animfilter_fcurves(&tmp_data, ads, first_fcu, ANIMTYPE_FCURVE, filter_mode, agrp, owner_id);
				}
			}
		}
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* add this group as a channel first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* restore original filter mode so that this next step works ok... */
			//filter_mode = ofilter;

			/* filter selection of channel specially here again, since may be open and not subject to previous test */
			if (ANIMCHANNEL_SELOK(SEL_AGRP(agrp)) ) {
				ANIMCHANNEL_NEW_CHANNEL(agrp, ANIMTYPE_GROUP, owner_id);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animfilter_action(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, bAction *act, int filter_mode, ID *owner_id)
{
	bActionGroup *agrp;
	FCurve *lastchan = NULL;
	size_t items = 0;

	/* don't include anything from this action if it is linked in from another file,
	 * and we're getting stuff for editing...
	 */
	if ((filter_mode & ANIMFILTER_FOREDIT) && ID_IS_LINKED(act))
		return 0;

	/* do groups */
	// TODO: do nested groups?
	for (agrp = act->groups.first; agrp; agrp = agrp->next) {
		/* store reference to last channel of group */
		if (agrp->channels.last)
			lastchan = agrp->channels.last;

		/* action group's channels */
		items += animfilter_act_group(ac, anim_data, ads, act, agrp, filter_mode, owner_id);
	}

	/* un-grouped F-Curves (only if we're not only considering those channels in the active group) */
	if (!(filter_mode & ANIMFILTER_ACTGROUPED)) {
		FCurve *firstfcu = (lastchan) ? (lastchan->next) : (act->curves.first);
		items += animfilter_fcurves(anim_data, ads, firstfcu, ANIMTYPE_FCURVE, filter_mode, NULL, owner_id);
	}

	/* return the number of items added to the list */
	return items;
}

/* Include NLA-Data for NLA-Editor:
 *	- when ANIMFILTER_LIST_CHANNELS is used, that means we should be filtering the list for display
 *	  Although the evaluation order is from the first track to the last and then apply the Action on top,
 *	  we present this in the UI as the Active Action followed by the last track to the first so that we
 *	  get the evaluation order presented as per a stack.
 *	- for normal filtering (i.e. for editing), we only need the NLA-tracks but they can be in 'normal' evaluation
 *	  order, i.e. first to last. Otherwise, some tools may get screwed up.
 */
static size_t animfilter_nla(bAnimContext *UNUSED(ac), ListBase *anim_data, bDopeSheet *ads, AnimData *adt, int filter_mode, ID *owner_id)
{
	NlaTrack *nlt;
	NlaTrack *first = NULL, *next = NULL;
	size_t items = 0;

	/* if showing channels, include active action */
	if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
		/* if NLA action-line filtering is off, don't show unless there are keyframes,
		 * in order to keep things more compact for doing transforms
		 */
		if (!(ads->filterflag & ADS_FILTER_NLA_NOACT) || (adt->action)) {
			/* there isn't really anything editable here, so skip if need editable */
			if ((filter_mode & ANIMFILTER_FOREDIT) == 0) {
				/* just add the action track now (this MUST appear for drawing)
				 *	- as AnimData may not have an action, we pass a dummy pointer just to get the list elem created, then
				 *	  overwrite this with the real value - REVIEW THIS...
				 */
				ANIMCHANNEL_NEW_CHANNEL_FULL((void *)(&adt->action), ANIMTYPE_NLAACTION, owner_id,
					{
						ale->data = adt->action ? adt->action : NULL;
					});
			}
		}

		/* first track to include will be the last one if we're filtering by channels */
		first = adt->nla_tracks.last;
	}
	else {
		/* first track to include will the first one (as per normal) */
		first = adt->nla_tracks.first;
	}

	/* loop over NLA Tracks - assume that the caller of this has already checked that these should be included */
	for (nlt = first; nlt; nlt = next) {
		/* 'next' NLA-Track to use depends on whether we're filtering for drawing or not */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS)
			next = nlt->prev;
		else
			next = nlt->next;

		/* if we're in NLA-tweakmode, don't show this track if it was disabled (due to tweaking) for now
		 *	- active track should still get shown though (even though it has disabled flag set)
		 */
		// FIXME: the channels after should still get drawn, just 'differently', and after an active-action channel
		if ((adt->flag & ADT_NLA_EDIT_ON) && (nlt->flag & NLATRACK_DISABLED) && (adt->act_track != nlt))
			continue;

		/* only work with this channel and its subchannels if it is editable */
		if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_NLT(nlt)) {
			/* only include this track if selected in a way consistent with the filtering requirements */
			if (ANIMCHANNEL_SELOK(SEL_NLT(nlt))) {
				/* only include if this track is active */
				if (!(filter_mode & ANIMFILTER_ACTIVE) || (nlt->flag & NLATRACK_ACTIVE)) {
					/* name based filtering... */
					if (((ads) && (ads->searchstr[0] != '\0')) && (owner_id)) {
						bool track_ok = false, strip_ok = false;

						/* check if the name of the track, or the strips it has are ok... */
						track_ok = name_matches_dopesheet_filter(ads, nlt->name);

						if (track_ok == false) {
							NlaStrip *strip;
							for (strip = nlt->strips.first; strip; strip = strip->next) {
								if (name_matches_dopesheet_filter(ads, strip->name)) {
									strip_ok = true;
									break;
								}
							}
						}

						/* skip if both fail this test... */
						if (!track_ok && !strip_ok) {
							continue;
						}
					}

					/* add the track now that it has passed all our tests */
					ANIMCHANNEL_NEW_CHANNEL(nlt, ANIMTYPE_NLATRACK, owner_id);
				}
			}
		}
	}

	/* return the number of items added to the list */
	return items;
}

/* Include the control FCurves per NLA Strip in the channel list
 * NOTE: This is includes the expander too...
 */
static size_t animfilter_nla_controls(ListBase *anim_data, bDopeSheet *ads, AnimData *adt, int filter_mode, ID *owner_id)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add control curves from each NLA strip... */
	/* NOTE: ANIMTYPE_FCURVES are created here, to avoid duplicating the code needed */
	BEGIN_ANIMFILTER_SUBCHANNELS(((adt->flag & ADT_NLA_SKEYS_COLLAPSED) == 0))
	{
		NlaTrack *nlt;
		NlaStrip *strip;

		/* for now, we only go one level deep - so controls on grouped FCurves are not handled */
		for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
			for (strip = nlt->strips.first; strip; strip = strip->next) {
				/* pass strip as the "owner", so that the name lookups (used while filtering) will resolve */
				tmp_items += animfilter_fcurves(&tmp_data, ads, strip->fcurves.first, ANIMTYPE_NLACURVE, filter_mode, strip, owner_id);
			}
		}
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* add the expander as a channel first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* currently these channels cannot be selected, so they should be skipped */
			if ((filter_mode & (ANIMFILTER_SEL | ANIMFILTER_UNSEL)) == 0) {
				ANIMCHANNEL_NEW_CHANNEL(adt, ANIMTYPE_NLACONTROLS, owner_id);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the numebr of items added to the list */
	return items;
}

/* determine what animation data from AnimData block should get displayed */
static size_t animfilter_block_data(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, ID *id, int filter_mode)
{
	AnimData *adt = BKE_animdata_from_id(id);
	size_t items = 0;

	/* image object datablocks have no anim-data so check for NULL */
	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id;

		/* NOTE: this macro is used instead of inlining the logic here, since this sort of filtering is still needed
		 * in a few places in the rest of the code still - notably for the few cases where special mode-based
		 * different types of data expanders are required.
		 */
		ANIMDATA_FILTER_CASES(iat,
			{ /* AnimData */
				/* specifically filter animdata block */
				if (ANIMCHANNEL_SELOK(SEL_ANIMDATA(adt)) ) {
					ANIMCHANNEL_NEW_CHANNEL(adt, ANIMTYPE_ANIMDATA, id);
				}
			},
			{ /* NLA */
				items += animfilter_nla(ac, anim_data, ads, adt, filter_mode, id);
			},
			{ /* Drivers */
				items += animfilter_fcurves(anim_data, ads, adt->drivers.first, ANIMTYPE_FCURVE, filter_mode, NULL, id);
			},
			{ /* NLA Control Keyframes */
				items += animfilter_nla_controls(anim_data, ads, adt, filter_mode, id);
			},
			{ /* Keyframes */
				items += animfilter_action(ac, anim_data, ads, adt->action, filter_mode, id);
			}
		);
	}

	return items;
}



/* Include ShapeKey Data for ShapeKey Editor */
static size_t animdata_filter_shapekey(bAnimContext *ac, ListBase *anim_data, Key *key, int filter_mode)
{
	size_t items = 0;

	/* check if channels or only F-Curves */
	if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
		KeyBlock *kb;

		/* loop through the channels adding ShapeKeys as appropriate */
		for (kb = key->block.first; kb; kb = kb->next) {
			/* skip the first one, since that's the non-animatable basis */
			if (kb == key->block.first) continue;

			/* only work with this channel and its subchannels if it is editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_SHAPEKEY(kb)) {
				/* only include this track if selected in a way consistent with the filtering requirements */
				if (ANIMCHANNEL_SELOK(SEL_SHAPEKEY(kb)) ) {
					// TODO: consider 'active' too?

					/* owner-id here must be key so that the F-Curve can be resolved... */
					ANIMCHANNEL_NEW_CHANNEL(kb, ANIMTYPE_SHAPEKEY, key);
				}
			}
		}
	}
	else {
		/* just use the action associated with the shapekey */
		// TODO: somehow manage to pass dopesheet info down here too?
		if (key->adt) {
			if (filter_mode & ANIMFILTER_ANIMDATA) {
				if (ANIMCHANNEL_SELOK(SEL_ANIMDATA(key->adt)) ) {
					ANIMCHANNEL_NEW_CHANNEL(key->adt, ANIMTYPE_ANIMDATA, key);
				}
			}
			else if (key->adt->action) {
				items = animfilter_action(ac, anim_data, NULL, key->adt->action, filter_mode, (ID *)key);
			}
		}
	}

	/* return the number of items added to the list */
	return items;
}

/* Helper for Grease Pencil - layers within a datablock */
static size_t animdata_filter_gpencil_layers_data(ListBase *anim_data, bDopeSheet *ads, bGPdata *gpd, int filter_mode)
{
	bGPDlayer *gpl;
	size_t items = 0;

	/* loop over layers as the conditions are acceptable */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* only if selected */
		if (ANIMCHANNEL_SELOK(SEL_GPL(gpl)) ) {
			/* only if editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_GPL(gpl)) {
				/* active... */
				if (!(filter_mode & ANIMFILTER_ACTIVE) || (gpl->flag & GP_LAYER_ACTIVE)) {
					/* skip layer if the name doesn't match the filter string */
					if ((ads) && (ads->searchstr[0] != '\0')) {
						if (name_matches_dopesheet_filter(ads, gpl->info) == false)
							continue;
					}


					/* add to list */
					ANIMCHANNEL_NEW_CHANNEL(gpl, ANIMTYPE_GPLAYER, gpd);
				}
			}
		}
	}

	return items;
}

/* Helper for Grease Pencil - Grease Pencil datablock - GP Frames */
static size_t animdata_filter_gpencil_data(ListBase *anim_data, bDopeSheet *ads, bGPdata *gpd, int filter_mode)
{
	size_t items = 0;

	/* When asked from "AnimData" blocks (i.e. the top-level containers for normal animation),
	 * for convenience, this will return GP Datablocks instead. This may cause issues down
	 * the track, but for now, this will do...
	 */
	if (filter_mode & ANIMFILTER_ANIMDATA) {
		/* just add GPD as a channel - this will add everything needed */
		ANIMCHANNEL_NEW_CHANNEL(gpd, ANIMTYPE_GPDATABLOCK, gpd);
	}
	else {
		ListBase tmp_data = {NULL, NULL};
		size_t tmp_items = 0;

		/* add gpencil animation channels */
		BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_GPD(gpd))
		{
			tmp_items += animdata_filter_gpencil_layers_data(&tmp_data, ads, gpd, filter_mode);
		}
		END_ANIMFILTER_SUBCHANNELS;

		/* did we find anything? */
		if (tmp_items) {
			/* include data-expand widget first */
			if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
				/* add gpd as channel too (if for drawing, and it has layers) */
				ANIMCHANNEL_NEW_CHANNEL(gpd, ANIMTYPE_GPDATABLOCK, NULL);
			}

			/* now add the list of collected channels */
			BLI_movelisttolist(anim_data, &tmp_data);
			BLI_assert(BLI_listbase_is_empty(&tmp_data));
			items += tmp_items;
		}
	}

	return items;
}

/* Grab all Grease Pencil datablocks in file */
// TODO: should this be amalgamated with the dopesheet filtering code?
static size_t animdata_filter_gpencil(bAnimContext *ac, ListBase *anim_data, void *UNUSED(data), int filter_mode)
{
	bDopeSheet *ads = ac->ads;
	size_t items = 0;

	if (ads->filterflag & ADS_FILTER_GP_3DONLY) {
		Scene *scene = (Scene *)ads->source;
		ViewLayer *view_layer = (ViewLayer *)ac->view_layer;
		Base *base;

		/* Active scene's GPencil block first - No parent item needed... */
		if (scene->gpd) {
			items += animdata_filter_gpencil_data(anim_data, ads, scene->gpd, filter_mode);
		}

		/* Objects in the scene */
		for (base = view_layer->object_bases.first; base; base = base->next) {
			/* Only consider this object if it has got some GP data (saving on all the other tests) */
			if (base->object && (base->object->type == OB_GPENCIL)) {
				Object *ob = base->object;

				/* firstly, check if object can be included, by the following factors:
				 *	- if only visible, must check for layer and also viewport visibility
				 *		--> while tools may demand only visible, user setting takes priority
				 *			as user option controls whether sets of channels get included while
				 *			tool-flag takes into account collapsed/open channels too
				 *	- if only selected, must check if object is selected
				 *	- there must be animation data to edit (this is done recursively as we
				 *	  try to add the channels)
				 */
				if ((filter_mode & ANIMFILTER_DATA_VISIBLE) && !(ads->filterflag & ADS_FILTER_INCL_HIDDEN)) {
					/* layer visibility - we check both object and base, since these may not be in sync yet */
					if ((base->flag & BASE_VISIBLE) == 0) continue;

					/* outliner restrict-flag */
					if (ob->restrictflag & OB_RESTRICT_VIEW) continue;
				}

				/* check selection and object type filters */
				if ( (ads->filterflag & ADS_FILTER_ONLYSEL) && !((base->flag & BASE_SELECTED) /*|| (base == scene->basact)*/) ) {
					/* only selected should be shown */
					continue;
				}

				/* check if object belongs to the filtering group if option to filter
				 * objects by the grouped status is on
				 *	- used to ease the process of doing multiple-character choreographies
				 */
				if (ads->filter_grp != NULL) {
					if (BKE_collection_has_object_recursive(ads->filter_grp, ob) == 0)
						continue;
				}

				/* finally, include this object's grease pencil datablock */
				/* XXX: Should we store these under expanders per item? */
				items += animdata_filter_gpencil_data(anim_data, ads, ob->data, filter_mode);
			}
		}
	}
	else {
		bGPdata *gpd;

		/* Grab all Grease Pencil datablocks directly from main, but only those that seem to be useful somewhere */
		for (gpd = ac->bmain->gpencil.first; gpd; gpd = gpd->id.next) {
			/* only show if gpd is used by something... */
			if (ID_REAL_USERS(gpd) < 1)
				continue;

			/* add GP frames from this datablock */
			items += animdata_filter_gpencil_data(anim_data, ads, gpd, filter_mode);
		}
	}

	/* return the number of items added to the list */
	return items;
}

/* Helper for Grease Pencil data integrated with main DopeSheet */
static size_t animdata_filter_ds_gpencil(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, bGPdata *gpd, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add relevant animation channels for Grease Pencil */
	BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_GPD(gpd))
	{
		/* add animation channels */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, &gpd->id, filter_mode);

		/* add Grease Pencil layers */
		// TODO: do these need a separate expander?
		// XXX:  what order should these go in?
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			// XXX: active check here needs checking
			if (ANIMCHANNEL_ACTIVEOK(gpd)) {
				ANIMCHANNEL_NEW_CHANNEL(gpd, ANIMTYPE_DSGPENCIL, gpd);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

/* Helper for Cache File data integrated with main DopeSheet */
static size_t animdata_filter_ds_cachefile(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, CacheFile *cache_file, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add relevant animation channels for Cache File */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_CACHEFILE_OBJD(cache_file))
	{
		/* add animation channels */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, &cache_file->id, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			// XXX: active check here needs checking
			if (ANIMCHANNEL_ACTIVEOK(cache_file)) {
				ANIMCHANNEL_NEW_CHANNEL(cache_file, ANIMTYPE_DSCACHEFILE, cache_file);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

/* Helper for Mask Editing - mask layers */
static size_t animdata_filter_mask_data(ListBase *anim_data, Mask *mask, const int filter_mode)
{
	MaskLayer *masklay_act = BKE_mask_layer_active(mask);
	MaskLayer *masklay;
	size_t items = 0;

	/* loop over layers as the conditions are acceptable */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		/* only if selected */
		if (ANIMCHANNEL_SELOK(SEL_MASKLAY(masklay)) ) {
			/* only if editable */
			if (!(filter_mode & ANIMFILTER_FOREDIT) || EDITABLE_MASK(masklay)) {
				/* active... */
				if (!(filter_mode & ANIMFILTER_ACTIVE) || (masklay_act == masklay)) {
					/* add to list */
					ANIMCHANNEL_NEW_CHANNEL(masklay, ANIMTYPE_MASKLAYER, mask);
				}
			}
		}
	}

	return items;
}

/* Grab all mask data */
static size_t animdata_filter_mask(Main *bmain, ListBase *anim_data, void *UNUSED(data), int filter_mode)
{
	Mask *mask;
	size_t items = 0;

	/* for now, grab mask datablocks directly from main */
	// XXX: this is not good...
	for (mask = bmain->mask.first; mask; mask = mask->id.next) {
		ListBase tmp_data = {NULL, NULL};
		size_t tmp_items = 0;

		/* only show if mask is used by something... */
		if (ID_REAL_USERS(mask) < 1)
			continue;

		/* add mask animation channels */
		BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_MASK(mask))
		{
			tmp_items += animdata_filter_mask_data(&tmp_data, mask, filter_mode);
		}
		END_ANIMFILTER_SUBCHANNELS;

		/* did we find anything? */
		if (tmp_items) {
			/* include data-expand widget first */
			if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
				/* add gpd as channel too (if for drawing, and it has layers) */
				ANIMCHANNEL_NEW_CHANNEL(mask, ANIMTYPE_MASKDATABLOCK, NULL);
			}

			/* now add the list of collected channels */
			BLI_movelisttolist(anim_data, &tmp_data);
			BLI_assert(BLI_listbase_is_empty(&tmp_data));
			items += tmp_items;
		}
	}

	/* return the number of items added to the list */
	return items;
}

/* NOTE: owner_id is scene, material, or texture block, which is the direct owner of the node tree in question */
static size_t animdata_filter_ds_nodetree_group(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, ID *owner_id, bNodeTree *ntree, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add nodetree animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_NTREE_DATA(ntree))
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)ntree, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(ntree)) {
				ANIMCHANNEL_NEW_CHANNEL(ntree, ANIMTYPE_DSNTREE, owner_id);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animdata_filter_ds_nodetree(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, ID *owner_id, bNodeTree *ntree, int filter_mode)
{
	bNode *node;
	size_t items = 0;

	items += animdata_filter_ds_nodetree_group(ac, anim_data, ads, owner_id, ntree, filter_mode);

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP) {
			if (node->id) {
				if ((ads->filterflag & ADS_FILTER_ONLYSEL) && (node->flag & NODE_SELECT) == 0) {
					continue;
				}
				items += animdata_filter_ds_nodetree_group(ac, anim_data, ads, owner_id, (bNodeTree *) node->id,
				                                           filter_mode | ANIMFILTER_TMP_IGNORE_ONLYSEL);
			}
		}
	}

	return items;
}

static size_t animdata_filter_ds_linestyle(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Scene *sce, int filter_mode)
{
	ViewLayer *view_layer;
	FreestyleLineSet *lineset;
	size_t items = 0;

	for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
			if (lineset->linestyle) {
				lineset->linestyle->id.tag |= LIB_TAG_DOIT;
			}
		}
	}

	for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
		/* skip render layers without Freestyle enabled */
		if ((view_layer->flag & VIEW_LAYER_FREESTYLE) == 0) {
			continue;
		}

		/* loop over linesets defined in the render layer */
		for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
			FreestyleLineStyle *linestyle = lineset->linestyle;
			ListBase tmp_data = {NULL, NULL};
			size_t tmp_items = 0;

			if ((linestyle == NULL) ||
			    !(linestyle->id.tag & LIB_TAG_DOIT))
			{
				continue;
			}
			linestyle->id.tag &= ~LIB_TAG_DOIT;

			/* add scene-level animation channels */
			BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_LS_SCED(linestyle))
			{
				/* animation data filtering */
				tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)linestyle, filter_mode);
			}
			END_ANIMFILTER_SUBCHANNELS;

			/* did we find anything? */
			if (tmp_items) {
				/* include anim-expand widget first */
				if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
					/* check if filtering by active status */
					if (ANIMCHANNEL_ACTIVEOK(linestyle)) {
						ANIMCHANNEL_NEW_CHANNEL(linestyle, ANIMTYPE_DSLINESTYLE, sce);
					}
				}

				/* now add the list of collected channels */
				BLI_movelisttolist(anim_data, &tmp_data);
				BLI_assert(BLI_listbase_is_empty(&tmp_data));
				items += tmp_items;
			}
		}
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animdata_filter_ds_texture(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads,
                                         Tex *tex, ID *owner_id, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add texture's animation data to temp collection */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_TEX_DATA(tex))
	{
		/* texture animdata */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)tex, filter_mode);

		/* nodes */
		if ((tex->nodetree) && !(ads->filterflag & ADS_FILTER_NONTREE)) {
			/* owner_id as id instead of texture, since it'll otherwise be impossible to track the depth */
			// FIXME: perhaps as a result, textures should NOT be included under materials, but under their own section instead
			// so that free-floating textures can also be animated
			tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, ads, (ID *)tex, tex->nodetree, filter_mode);
		}
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include texture-expand widget? */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(tex)) {
				ANIMCHANNEL_NEW_CHANNEL(tex, ANIMTYPE_DSTEX, owner_id);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

/* NOTE: owner_id is the direct owner of the texture stack in question
 *       It used to be Material/Lamp/World before the Blender Internal removal for 2.8
 */
static size_t animdata_filter_ds_textures(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, ID *owner_id, int filter_mode)
{
	MTex **mtex = NULL;
	size_t items = 0;
	int a = 0;

	/* get datatype specific data first */
	if (owner_id == NULL)
		return 0;

	switch (GS(owner_id->name)) {
		case ID_PA:
		{
			ParticleSettings *part = (ParticleSettings *)owner_id;
			mtex = (MTex **)(&part->mtex);
			break;
		}
		default:
		{
			/* invalid/unsupported option */
			if (G.debug & G_DEBUG)
				printf("ERROR: Unsupported owner_id (i.e. texture stack) for filter textures - %s\n", owner_id->name);
			return 0;
		}
	}

	/* firstly check that we actuallly have some textures, by gathering all textures in a temp list */
	for (a = 0; a < MAX_MTEX; a++) {
		Tex *tex = (mtex[a]) ? mtex[a]->tex : NULL;

		/* for now, if no texture returned, skip (this shouldn't confuse the user I hope) */
		if (tex == NULL)
			continue;

		/* add texture's anim channels */
		items += animdata_filter_ds_texture(ac, anim_data, ads, tex, owner_id, filter_mode);
	}

	/* return the number of items added to the list */
	return items;
}


static size_t animdata_filter_ds_material(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Material *ma, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add material's animation data to temp collection */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_MAT_OBJD(ma))
	{
		/* material's animation data */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)ma, filter_mode);

		/* nodes */
		if ((ma->nodetree) && !(ads->filterflag & ADS_FILTER_NONTREE))
			tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, ads, (ID *)ma, ma->nodetree, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include material-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(ma)) {
				ANIMCHANNEL_NEW_CHANNEL(ma, ANIMTYPE_DSMAT, ma);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	return items;
}

static size_t animdata_filter_ds_materials(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, int filter_mode)
{
	bool has_nested = false;
	size_t items = 0;
	int a = 0;

	/* first pass: take the materials referenced via the Material slots of the object */
	for (a = 1; a <= ob->totcol; a++) {
		Material *ma = give_current_material(ob, a);

		/* if material is valid, try to add relevant contents from here */
		if (ma) {
			/* add channels */
			items += animdata_filter_ds_material(ac, anim_data, ads, ma, filter_mode);

			/* for optimising second pass - check if there's a nested material here to come back for */
			if (has_nested == false) {
				has_nested = (give_node_material(ma) != NULL);
			}
		}
	}

	/* second pass: go through a second time looking for "nested" materials (material.material references)
	 *
	 * NOTE: here we ignore the expanded status of the parent, as it could be too confusing as to why these are
	 *       disappearing/not available, since the relationships between these is not that clear
	 */
	if (has_nested) {
		for (a = 1; a <= ob->totcol; a++) {
			Material *base = give_current_material(ob, a);
			Material *ma   = give_node_material(base);

			/* add channels from the nested material if it exists
			 *   - skip if the same material is referenced in its node tree
			 *     (which is common for BI materials) as that results in
			 *     confusing duplicates
			 */
			if ((ma) && (ma != base)) {
				items += animdata_filter_ds_material(ac, anim_data, ads, ma, filter_mode);
			}
		}
	}

	/* return the number of items added to the list */
	return items;
}


/* ............ */

/* Temporary context for modifier linked-data channel extraction */
typedef struct tAnimFilterModifiersContext {
	bAnimContext *ac;	/* anim editor context */
	bDopeSheet *ads;    /* dopesheet filtering settings */

	ListBase tmp_data;  /* list of channels created (but not yet added to the main list) */
	size_t items;       /* number of channels created */

	int filter_mode;    /* flags for stuff we want to filter */
} tAnimFilterModifiersContext;


/* dependency walker callback for modifier dependencies */
static void animfilter_modifier_idpoin_cb(void *afm_ptr, Object *ob, ID **idpoin, int UNUSED(cb_flag))
{
	tAnimFilterModifiersContext *afm = (tAnimFilterModifiersContext *)afm_ptr;
	ID *owner_id = &ob->id;
	ID *id = *idpoin;

	/* NOTE: the walker only guarantees to give us all the ID-ptr *slots*,
	 * not just the ones which are actually used, so be careful!
	 */
	if (id == NULL)
		return;

	/* check if this is something we're interested in... */
	switch (GS(id->name)) {
		case ID_TE: /* Textures */
		{
			Tex *tex = (Tex *)id;
			if (!(afm->ads->filterflag & ADS_FILTER_NOTEX)) {
				afm->items += animdata_filter_ds_texture(afm->ac, &afm->tmp_data, afm->ads, tex, owner_id, afm->filter_mode);
			}
			break;
		}

		/* TODO: images? */
		default:
			break;
	}
}

/* animation linked to data used by modifiers
 * NOTE: strictly speaking, modifier animation is already included under Object level
 *       but for some modifiers (e.g. Displace), there can be linked data that has settings
 *       which would be nice to animate (i.e. texture parameters) but which are not actually
 *       attached to any other objects/materials/etc. in the scene
 */
// TODO: do we want an expander for this?
static size_t animdata_filter_ds_modifiers(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, int filter_mode)
{
	tAnimFilterModifiersContext afm = {NULL};
	size_t items = 0;

	/* 1) create a temporary "context" containing all the info we have here to pass to the callback
	 *    use to walk through the dependencies of the modifiers
	 *
	 * ! Assumes that all other unspecified values (i.e. accumulation buffers) are zero'd out properly
	 */
	afm.ac          = ac;
	afm.ads         = ads;
	afm.filter_mode = filter_mode;

	/* 2) walk over dependencies */
	modifiers_foreachIDLink(ob, animfilter_modifier_idpoin_cb, &afm);

	/* 3) extract data from the context, merging it back into the standard list */
	if (afm.items) {
		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &afm.tmp_data);
		BLI_assert(BLI_listbase_is_empty(&afm.tmp_data));
		items += afm.items;
	}

	return items;
}

/* ............ */


static size_t animdata_filter_ds_particles(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, int filter_mode)
{
	ParticleSystem *psys;
	size_t items = 0;

	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		ListBase tmp_data = {NULL, NULL};
		size_t tmp_items = 0;

		/* if no material returned, skip - so that we don't get weird blank entries... */
		if (ELEM(NULL, psys->part, psys->part->adt))
			continue;

		/* add particle-system's animation data to temp collection */
		BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_PART_OBJD(psys->part))
		{
			/* particle system's animation data */
			tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)psys->part, filter_mode);

			/* textures */
			if (!(ads->filterflag & ADS_FILTER_NOTEX))
				tmp_items += animdata_filter_ds_textures(ac, &tmp_data, ads, (ID *)psys->part, filter_mode);
		}
		END_ANIMFILTER_SUBCHANNELS;

		/* did we find anything? */
		if (tmp_items) {
			/* include particle-expand widget first */
			if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
				/* check if filtering by active status */
				if (ANIMCHANNEL_ACTIVEOK(psys->part)) {
					ANIMCHANNEL_NEW_CHANNEL(psys->part, ANIMTYPE_DSPART, psys->part);
				}
			}

			/* now add the list of collected channels */
			BLI_movelisttolist(anim_data, &tmp_data);
			BLI_assert(BLI_listbase_is_empty(&tmp_data));
			items += tmp_items;
		}
	}

	/* return the number of items added to the list */
	return items;
}


static size_t animdata_filter_ds_obdata(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	IdAdtTemplate *iat = ob->data;
	short type = 0, expanded = 0;

	/* get settings based on data type */
	switch (ob->type) {
		case OB_CAMERA: /* ------- Camera ------------ */
		{
			Camera *ca = (Camera *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOCAM)
				return 0;

			type = ANIMTYPE_DSCAM;
			expanded = FILTER_CAM_OBJD(ca);
			break;
		}
		case OB_LAMP: /* ---------- Lamp ----------- */
		{
			Lamp *la = (Lamp *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOLAM)
				return 0;

			type = ANIMTYPE_DSLAM;
			expanded = FILTER_LAM_OBJD(la);
			break;
		}
		case OB_CURVE: /* ------- Curve ---------- */
		case OB_SURF: /* ------- Nurbs Surface ---------- */
		case OB_FONT: /* ------- Text Curve ---------- */
		{
			Curve *cu = (Curve *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOCUR)
				return 0;

			type = ANIMTYPE_DSCUR;
			expanded = FILTER_CUR_OBJD(cu);
			break;
		}
		case OB_MBALL: /* ------- MetaBall ---------- */
		{
			MetaBall *mb = (MetaBall *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOMBA)
				return 0;

			type = ANIMTYPE_DSMBALL;
			expanded = FILTER_MBALL_OBJD(mb);
			break;
		}
		case OB_ARMATURE: /* ------- Armature ---------- */
		{
			bArmature *arm = (bArmature *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOARM)
				return 0;

			type = ANIMTYPE_DSARM;
			expanded = FILTER_ARM_OBJD(arm);
			break;
		}
		case OB_MESH: /* ------- Mesh ---------- */
		{
			Mesh *me = (Mesh *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOMESH)
				return 0;

			type = ANIMTYPE_DSMESH;
			expanded = FILTER_MESH_OBJD(me);
			break;
		}
		case OB_LATTICE: /* ---- Lattice ---- */
		{
			Lattice *lt = (Lattice *)ob->data;

			if (ads->filterflag & ADS_FILTER_NOLAT)
				return 0;

			type = ANIMTYPE_DSLAT;
			expanded = FILTER_LATTICE_OBJD(lt);
			break;
		}
		case OB_SPEAKER: /* ---------- Speaker ----------- */
		{
			Speaker *spk = (Speaker *)ob->data;

			type = ANIMTYPE_DSSPK;
			expanded = FILTER_SPK_OBJD(spk);
			break;
		}
	}

	/* add object data animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(expanded)
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)iat, filter_mode);

		/* sub-data filtering... */
		switch (ob->type) {
			case OB_LAMP:  /* lamp - textures + nodetree */
			{
				Lamp *la = ob->data;
				bNodeTree *ntree = la->nodetree;

				/* nodetree */
				if ((ntree) && !(ads->filterflag & ADS_FILTER_NONTREE))
					tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, ads, &la->id, ntree, filter_mode);
				break;
			}
		}
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(iat)) {
				ANIMCHANNEL_NEW_CHANNEL(iat, type, iat);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

/* shapekey-level animation */
static size_t animdata_filter_ds_keyanim(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, Key *key, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add shapekey-level animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_SKE_OBJD(key))
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)key, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include key-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			if (ANIMCHANNEL_ACTIVEOK(key)) {
				ANIMCHANNEL_NEW_CHANNEL(key, ANIMTYPE_DSSKEY, ob);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}


/* object-level animation */
static size_t animdata_filter_ds_obanim(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Object *ob, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	AnimData *adt = ob->adt;
	short type = 0, expanded = 1;
	void *cdata = NULL;

	/* determine the type of expander channels to use */
	/* this is the best way to do this for now... */
	ANIMDATA_FILTER_CASES(ob,
		{ /* AnimData - no channel, but consider data */ },
		{ /* NLA - no channel, but consider data */ },
		{ /* Drivers */
			type = ANIMTYPE_FILLDRIVERS;
			cdata = adt;
			expanded = EXPANDED_DRVD(adt);
		},
		{ /* NLA Strip Controls - no dedicated channel for now (XXX) */ },
		{ /* Keyframes */
			type = ANIMTYPE_FILLACTD;
			cdata = adt->action;
			expanded = EXPANDED_ACTC(adt->action);
		});

	/* add object-level animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(expanded)
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)ob, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include anim-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			if (type != ANIMTYPE_NONE) {
				/* NOTE: active-status (and the associated checks) don't apply here... */
				ANIMCHANNEL_NEW_CHANNEL(cdata, type, ob);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

/* get animation channels from object2 */
static size_t animdata_filter_dopesheet_ob(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Base *base, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	Object *ob = base->object;
	size_t tmp_items = 0;
	size_t items = 0;

	/* filter data contained under object first */
	BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_OBJC(ob))
	{
		Key *key = BKE_key_from_object(ob);

		/* object-level animation */
		if ((ob->adt) && !(ads->filterflag & ADS_FILTER_NOOBJ)) {
			tmp_items += animdata_filter_ds_obanim(ac, &tmp_data, ads, ob, filter_mode);
		}

		/* shape-key */
		if ((key && key->adt) && !(ads->filterflag & ADS_FILTER_NOSHAPEKEYS)) {
			tmp_items += animdata_filter_ds_keyanim(ac, &tmp_data, ads, ob, key, filter_mode);
		}

		/* modifiers */
		if ((ob->modifiers.first) && !(ads->filterflag & ADS_FILTER_NOMODIFIERS)) {
			tmp_items += animdata_filter_ds_modifiers(ac, &tmp_data, ads, ob, filter_mode);
		}

		/* materials */
		if ((ob->totcol) && !(ads->filterflag & ADS_FILTER_NOMAT)) {
			tmp_items += animdata_filter_ds_materials(ac, &tmp_data, ads, ob, filter_mode);
		}

		/* object data */
		if (ob->data) {
			tmp_items += animdata_filter_ds_obdata(ac, &tmp_data, ads, ob, filter_mode);
		}

		/* particles */
		if ((ob->particlesystem.first) && !(ads->filterflag & ADS_FILTER_NOPART)) {
			tmp_items += animdata_filter_ds_particles(ac, &tmp_data, ads, ob, filter_mode);
		}

		/* grease pencil */
		if ((ob->type == OB_GPENCIL) &&
		    (ob->data) && !(ads->filterflag & ADS_FILTER_NOGPENCIL))
		{
			tmp_items += animdata_filter_ds_gpencil(ac, &tmp_data, ads, ob->data, filter_mode);
		}
	}
	END_ANIMFILTER_SUBCHANNELS;


	/* if we collected some channels, add these to the new list... */
	if (tmp_items) {
		/* firstly add object expander if required */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by selection */
			// XXX: double-check on this - most of the time, a lot of tools need to filter out these channels!
			if (ANIMCHANNEL_SELOK((base->flag & BASE_SELECTED))) {
				/* check if filtering by active status */
				if (ANIMCHANNEL_ACTIVEOK(ob)) {
					ANIMCHANNEL_NEW_CHANNEL(base, ANIMTYPE_OBJECT, ob);
				}
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added */
	return items;
}

static size_t animdata_filter_ds_world(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Scene *sce, World *wo, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* add world animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(FILTER_WOR_SCED(wo))
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)wo, filter_mode);

		/* nodes */
		if ((wo->nodetree) && !(ads->filterflag & ADS_FILTER_NONTREE))
			tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, ads, (ID *)wo, wo->nodetree, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(wo)) {
				ANIMCHANNEL_NEW_CHANNEL(wo, ANIMTYPE_DSWOR, sce);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animdata_filter_ds_scene(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Scene *sce, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	AnimData *adt = sce->adt;
	short type = 0, expanded = 1;
	void *cdata = NULL;

	/* determine the type of expander channels to use */
	// this is the best way to do this for now...
	ANIMDATA_FILTER_CASES(sce,
		{ /* AnimData - no channel, but consider data */},
		{ /* NLA - no channel, but consider data */},
		{ /* Drivers */
			type = ANIMTYPE_FILLDRIVERS;
			cdata = adt;
			expanded = EXPANDED_DRVD(adt);
		},
		{ /* NLA Strip Controls - no dedicated channel for now (XXX) */ },
		{ /* Keyframes */
			type = ANIMTYPE_FILLACTD;
			cdata = adt->action;
			expanded = EXPANDED_ACTC(adt->action);
		});

	/* add scene-level animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(expanded)
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)sce, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* did we find anything? */
	if (tmp_items) {
		/* include anim-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			if (type != ANIMTYPE_NONE) {
				/* NOTE: active-status (and the associated checks) don't apply here... */
				ANIMCHANNEL_NEW_CHANNEL(cdata, type, sce);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added to the list */
	return items;
}

static size_t animdata_filter_dopesheet_scene(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, Scene *sce, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;

	/* filter data contained under object first */
	BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_SCEC(sce))
	{
		bNodeTree *ntree = sce->nodetree;
		bGPdata *gpd = sce->gpd;
		World *wo = sce->world;

		/* Action, Drivers, or NLA for Scene */
		if ((ads->filterflag & ADS_FILTER_NOSCE) == 0) {
			tmp_items += animdata_filter_ds_scene(ac, &tmp_data, ads, sce, filter_mode);
		}

		/* world */
		if ((wo) && !(ads->filterflag & ADS_FILTER_NOWOR)) {
			tmp_items += animdata_filter_ds_world(ac, &tmp_data, ads, sce, wo, filter_mode);
		}

		/* nodetree */
		if ((ntree) && !(ads->filterflag & ADS_FILTER_NONTREE)) {
			tmp_items += animdata_filter_ds_nodetree(ac, &tmp_data, ads, (ID *)sce, ntree, filter_mode);
		}

		/* line styles */
		if ((ads->filterflag & ADS_FILTER_NOLINESTYLE) == 0) {
			tmp_items += animdata_filter_ds_linestyle(ac, &tmp_data, ads, sce, filter_mode);
		}

		/* grease pencil */
		if ((gpd) && !(ads->filterflag & ADS_FILTER_NOGPENCIL)) {
			tmp_items += animdata_filter_ds_gpencil(ac, &tmp_data, ads, gpd, filter_mode);
		}

		/* TODO: one day, when sequencer becomes its own datatype, perhaps it should be included here */
	}
	END_ANIMFILTER_SUBCHANNELS;

	/* if we collected some channels, add these to the new list... */
	if (tmp_items) {
		/* firstly add object expander if required */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by selection */
			if (ANIMCHANNEL_SELOK((sce->flag & SCE_DS_SELECTED))) {
				/* NOTE: active-status doesn't matter for this! */
				ANIMCHANNEL_NEW_CHANNEL(sce, ANIMTYPE_SCENE, sce);
			}
		}

		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}

	/* return the number of items added */
	return items;
}

static size_t animdata_filter_ds_movieclip(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, MovieClip *clip, int filter_mode)
{
	ListBase tmp_data = {NULL, NULL};
	size_t tmp_items = 0;
	size_t items = 0;
	/* add world animation channels */
	BEGIN_ANIMFILTER_SUBCHANNELS(EXPANDED_MCLIP(clip))
	{
		/* animation data filtering */
		tmp_items += animfilter_block_data(ac, &tmp_data, ads, (ID *)clip, filter_mode);
	}
	END_ANIMFILTER_SUBCHANNELS;
	/* did we find anything? */
	if (tmp_items) {
		/* include data-expand widget first */
		if (filter_mode & ANIMFILTER_LIST_CHANNELS) {
			/* check if filtering by active status */
			if (ANIMCHANNEL_ACTIVEOK(clip)) {
				ANIMCHANNEL_NEW_CHANNEL(clip, ANIMTYPE_DSMCLIP, clip);
			}
		}
		/* now add the list of collected channels */
		BLI_movelisttolist(anim_data, &tmp_data);
		BLI_assert(BLI_listbase_is_empty(&tmp_data));
		items += tmp_items;
	}
	/* return the number of items added to the list */
	return items;
}

static size_t animdata_filter_dopesheet_movieclips(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, int filter_mode)
{
	size_t items = 0;
	MovieClip *clip;
	for (clip = ac->bmain->movieclip.first; clip != NULL; clip = clip->id.next) {
		/* only show if gpd is used by something... */
		if (ID_REAL_USERS(clip) < 1) {
			continue;
		}
		items += animdata_filter_ds_movieclip(ac, anim_data, ads, clip, filter_mode);
	}
	/* return the number of items added to the list */
	return items;
}

/* Helper for animdata_filter_dopesheet() - For checking if an object should be included or not */
static bool animdata_filter_base_is_ok(bDopeSheet *ads, Base *base, int filter_mode)
{
	Object *ob = base->object;

	if (base->object == NULL)
		return false;

	/* firstly, check if object can be included, by the following factors:
	 *	- if only visible, must check for layer and also viewport visibility
	 *		--> while tools may demand only visible, user setting takes priority
	 *			as user option controls whether sets of channels get included while
	 *			tool-flag takes into account collapsed/open channels too
	 *	- if only selected, must check if object is selected
	 *	- there must be animation data to edit (this is done recursively as we
	 *	  try to add the channels)
	 */
	if ((filter_mode & ANIMFILTER_DATA_VISIBLE) && !(ads->filterflag & ADS_FILTER_INCL_HIDDEN)) {
		/* layer visibility - we check both object and base, since these may not be in sync yet */
		if ((base->flag & BASE_VISIBLE) == 0)
			return false;

		/* outliner restrict-flag */
		if (ob->restrictflag & OB_RESTRICT_VIEW)
			return false;
	}

	/* if only F-Curves with visible flags set can be shown, check that
	 * datablock hasn't been set to invisible
	 */
	if (filter_mode & ANIMFILTER_CURVE_VISIBLE) {
		if ((ob->adt) && (ob->adt->flag & ADT_CURVES_NOT_VISIBLE))
			return false;
	}

	/* Pinned curves are visible regardless of selection flags. */
	if ((ob->adt) && (ob->adt->flag & ADT_CURVES_ALWAYS_VISIBLE)) {
		return true;
	}

	/* Special case.
	 * We don't do recursive checks for pin, but we need to deal with tricky
	 * setup like animated camera lens without animated camera location.
	 * Without such special handle here we wouldn't be able to bin such
	 * camera data only animation to the editor.
	 */
	if (ob->adt == NULL && ob->data != NULL) {
		AnimData *data_adt = BKE_animdata_from_id(ob->data);
		if (data_adt != NULL && (data_adt->flag & ADT_CURVES_ALWAYS_VISIBLE)) {
			return true;
		}
	}

	/* check selection and object type filters */
	if ((ads->filterflag & ADS_FILTER_ONLYSEL) && !((base->flag & BASE_SELECTED) /*|| (base == sce->basact)*/)) {
		/* only selected should be shown */
		return false;
	}

	/* check if object belongs to the filtering group if option to filter
	 * objects by the grouped status is on
	 *	- used to ease the process of doing multiple-character choreographies
	 */
	if (ads->filter_grp != NULL) {
		if (BKE_collection_has_object_recursive(ads->filter_grp, ob) == 0)
			return false;
	}

	/* no reason to exclude this object... */
	return true;
}

/* Helper for animdata_filter_ds_sorted_bases() - Comparison callback for two Base pointers... */
static int ds_base_sorting_cmp(const void *base1_ptr, const void *base2_ptr)
{
	const Base *b1 = *((const Base **)base1_ptr);
	const Base *b2 = *((const Base **)base2_ptr);

	return strcmp(b1->object->id.name + 2, b2->object->id.name + 2);
}

/* Get a sorted list of all the bases - for inclusion in dopesheet (when drawing channels) */
static Base **animdata_filter_ds_sorted_bases(bDopeSheet *ads, ViewLayer *view_layer, int filter_mode, size_t *r_usable_bases)
{
	/* Create an array with space for all the bases, but only containing the usable ones */
	size_t tot_bases = BLI_listbase_count(&view_layer->object_bases);
	size_t num_bases = 0;

	Base **sorted_bases = MEM_mallocN(sizeof(Base *) * tot_bases, "Dopesheet Usable Sorted Bases");
	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if (animdata_filter_base_is_ok(ads, base, filter_mode)) {
			sorted_bases[num_bases++] = base;
		}
	}

	/* Sort this list of pointers (based on the names) */
	qsort(sorted_bases, num_bases, sizeof(Base *), ds_base_sorting_cmp);

	/* Return list of sorted bases */
	*r_usable_bases = num_bases;
	return sorted_bases;
}


// TODO: implement pinning... (if and when pinning is done, what we need to do is to provide freeing mechanisms - to protect against data that was deleted)
static size_t animdata_filter_dopesheet(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, int filter_mode)
{
	Scene *scene = (Scene *)ads->source;
	ViewLayer *view_layer = (ViewLayer *)ac->view_layer;
	size_t items = 0;

	/* check that we do indeed have a scene */
	if ((ads->source == NULL) || (GS(ads->source->name) != ID_SCE)) {
		printf("Dope Sheet Error: No scene!\n");
		if (G.debug & G_DEBUG)
			printf("\tPointer = %p, Name = '%s'\n", (void *)ads->source, (ads->source) ? ads->source->name : NULL);
		return 0;
	}

	/* augment the filter-flags with settings based on the dopesheet filterflags
	 * so that some temp settings can get added automagically...
	 */
	if (ads->filterflag & ADS_FILTER_SELEDIT) {
		/* only selected F-Curves should get their keyframes considered for editability */
		filter_mode |= ANIMFILTER_SELEDIT;
	}

	/* Cache files level animations (frame duration and such). */
	CacheFile *cache_file = ac->bmain->cachefiles.first;
	for (; cache_file; cache_file = cache_file->id.next) {
		items += animdata_filter_ds_cachefile(ac, anim_data, ads, cache_file, filter_mode);
	}

	/* movie clip's animation */
	items += animdata_filter_dopesheet_movieclips(ac, anim_data, ads, filter_mode);

	/* scene-linked animation - e.g. world, compositing nodes, scene anim (including sequencer currently) */
	items += animdata_filter_dopesheet_scene(ac, anim_data, ads, scene, filter_mode);

	/* If filtering for channel drawing, we want the objects in alphabetical order,
	 * to make it easier to predict where items are in the hierarchy
	 *  - This order only really matters if we need to show all channels in the list (e.g. for drawing)
	 *    (XXX: What about lingering "active" flags? The order may now become unpredictable)
	 *  - Don't do this if this behaviour has been turned off (i.e. due to it being too slow)
	 *  - Don't do this if there's just a single object
	 */
	if ((filter_mode & ANIMFILTER_LIST_CHANNELS) && !(ads->flag & ADS_FLAG_NO_DB_SORT) &&
	    (view_layer->object_bases.first != view_layer->object_bases.last))
	{
		/* Filter list of bases (i.e. objects), sort them, then add their contents normally... */
		// TODO: Cache the old sorted order - if the set of bases hasn't changed, don't re-sort...
		Base **sorted_bases;
		size_t num_bases;

		sorted_bases = animdata_filter_ds_sorted_bases(ads, view_layer, filter_mode, &num_bases);
		if (sorted_bases) {
			/* Add the necessary channels for these bases... */
			for (size_t i = 0; i < num_bases; i++) {
				items += animdata_filter_dopesheet_ob(ac, anim_data, ads, sorted_bases[i], filter_mode);
			}

			// TODO: store something to validate whether any changes are needed?

			/* free temporary data */
			MEM_freeN(sorted_bases);
		}
	}
	else {
		/* Filter and add contents of each base (i.e. object) without them sorting first
		 * NOTE: This saves performance in cases where order doesn't matter
		 */
		for (Base *base = view_layer->object_bases.first; base; base = base->next) {
			if (animdata_filter_base_is_ok(ads, base, filter_mode)) {
				/* since we're still here, this object should be usable */
				items += animdata_filter_dopesheet_ob(ac, anim_data, ads, base, filter_mode);
			}
		}
	}

	/* return the number of items in the list */
	return items;
}

/* Summary track for DopeSheet/Action Editor
 *  - return code is whether the summary lets the other channels get drawn
 */
static short animdata_filter_dopesheet_summary(bAnimContext *ac, ListBase *anim_data, int filter_mode, size_t *items)
{
	bDopeSheet *ads = NULL;

	/* get the DopeSheet information to use
	 *	- we should only need to deal with the DopeSheet/Action Editor,
	 *	  since all the other Animation Editors won't have this concept
	 *	  being applicable.
	 */
	if ((ac && ac->sl) && (ac->spacetype == SPACE_ACTION)) {
		SpaceAction *saction = (SpaceAction *)ac->sl;
		ads = &saction->ads;
	}
	else {
		/* invalid space type - skip this summary channels */
		return 1;
	}

	/* dopesheet summary
	 *	- only for drawing and/or selecting keyframes in channels, but not for real editing
	 *	- only useful for DopeSheet/Action/etc. editors where it is actually useful
	 */
	if ((filter_mode & ANIMFILTER_LIST_CHANNELS) && (ads->filterflag & ADS_FILTER_SUMMARY)) {
		bAnimListElem *ale = make_new_animlistelem(ac, ANIMTYPE_SUMMARY, NULL);
		if (ale) {
			BLI_addtail(anim_data, ale);
			(*items)++;
		}

		/* if summary is collapsed, don't show other channels beneath this
		 *	- this check is put inside the summary check so that it doesn't interfere with normal operation
		 */
		if (ads->flag & ADS_FLAG_SUMMARY_COLLAPSED)
			return 0;
	}

	/* the other channels beneath this can be shown */
	return 1;
}

/* ......................... */

/* filter data associated with a channel - usually for handling summary-channels in DopeSheet */
static size_t animdata_filter_animchan(bAnimContext *ac, ListBase *anim_data, bDopeSheet *ads, bAnimListElem *channel, int filter_mode)
{
	size_t items = 0;

	/* data to filter depends on channel type */
	/* NOTE: only common channel-types have been handled for now. More can be added as necessary */
	switch (channel->type) {
		case ANIMTYPE_SUMMARY:
			items += animdata_filter_dopesheet(ac, anim_data, ads, filter_mode);
			break;

		case ANIMTYPE_SCENE:
			items += animdata_filter_dopesheet_scene(ac, anim_data, ads, channel->data, filter_mode);
			break;

		case ANIMTYPE_OBJECT:
			items += animdata_filter_dopesheet_ob(ac, anim_data, ads, channel->data, filter_mode);
			break;

		case ANIMTYPE_DSCACHEFILE:
			items += animdata_filter_ds_cachefile(ac, anim_data, ads, channel->data, filter_mode);
			break;

		case ANIMTYPE_ANIMDATA:
			items += animfilter_block_data(ac, anim_data, ads, channel->id, filter_mode);
			break;

		default:
			printf("ERROR: Unsupported channel type (%d) in animdata_filter_animchan()\n", channel->type);
			break;
	}

	return items;
}

/* ----------- Cleanup API --------------- */

/* Remove entries with invalid types in animation channel list */
static size_t animdata_filter_remove_invalid(ListBase *anim_data)
{
	bAnimListElem *ale, *next;
	size_t items = 0;

	/* only keep entries with valid types */
	for (ale = anim_data->first; ale; ale = next) {
		next = ale->next;

		if (ale->type == ANIMTYPE_NONE)
			BLI_freelinkN(anim_data, ale);
		else
			items++;
	}

	return items;
}

/* Remove duplicate entries in animation channel list */
static size_t animdata_filter_remove_duplis(ListBase *anim_data)
{
	bAnimListElem *ale, *next;
	GSet *gs;
	size_t items = 0;

	/* build new hashtable to efficiently store and retrieve which entries have been
	 * encountered already while searching
	 */
	gs = BLI_gset_ptr_new(__func__);

	/* loop through items, removing them from the list if a similar item occurs already */
	for (ale = anim_data->first; ale; ale = next) {
		next = ale->next;

		/* check if hash has any record of an entry like this
		 *	- just use ale->data for now, though it would be nicer to involve
		 *	  ale->type in combination too to capture corner cases (where same data performs differently)
		 */
		if (BLI_gset_add(gs, ale->data)) {
			/* this entry is 'unique' and can be kept */
			items++;
		}
		else {
			/* this entry isn't needed anymore */
			BLI_freelinkN(anim_data, ale);
		}
	}

	/* free the hash... */
	BLI_gset_free(gs, NULL);

	/* return the number of items still in the list */
	return items;
}

/* ----------- Public API --------------- */

/* This function filters the active data source to leave only animation channels suitable for
 * usage by the caller. It will return the length of the list
 *
 *  *anim_data: is a pointer to a ListBase, to which the filtered animation channels
 *		will be placed for use.
 *	filter_mode: how should the data be filtered - bitmapping accessed flags
 */
size_t ANIM_animdata_filter(bAnimContext *ac, ListBase *anim_data, eAnimFilter_Flags filter_mode, void *data, eAnimCont_Types datatype)
{
	size_t items = 0;

	/* only filter data if there's somewhere to put it */
	if (data && anim_data) {
		/* firstly filter the data */
		switch (datatype) {
			/* Action-Editing Modes */
			case ANIMCONT_ACTION:   /* 'Action Editor' */
			{
				Object *obact = ac->obact;
				SpaceAction *saction = (SpaceAction *)ac->sl;
				bDopeSheet *ads = (saction) ? &saction->ads : NULL;

				/* specially check for AnimData filter... [#36687] */
				if (UNLIKELY(filter_mode & ANIMFILTER_ANIMDATA)) {
					/* all channels here are within the same AnimData block, hence this special case */
					if (LIKELY(obact->adt)) {
						ANIMCHANNEL_NEW_CHANNEL(obact->adt, ANIMTYPE_ANIMDATA, (ID *)obact);
					}
				}
				else {
					/* the check for the DopeSheet summary is included here since the summary works here too */
					if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
						items += animfilter_action(ac, anim_data, ads, data, filter_mode, (ID *)obact);
				}

				break;
			}
			case ANIMCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			{
				Key *key = (Key *)data;

				/* specially check for AnimData filter... [#36687] */
				if (UNLIKELY(filter_mode & ANIMFILTER_ANIMDATA)) {
					/* all channels here are within the same AnimData block, hence this special case */
					if (LIKELY(key->adt)) {
						ANIMCHANNEL_NEW_CHANNEL(key->adt, ANIMTYPE_ANIMDATA, (ID *)key);
					}
				}
				else {
					/* the check for the DopeSheet summary is included here since the summary works here too */
					if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
						items = animdata_filter_shapekey(ac, anim_data, key, filter_mode);
				}

				break;
			}


			/* Modes for Specialty Data Types (i.e. not keyframes) */
			case ANIMCONT_GPENCIL:
			{
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items = animdata_filter_gpencil(ac, anim_data, data, filter_mode);
				break;
			}
			case ANIMCONT_MASK:
			{
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items = animdata_filter_mask(ac->bmain, anim_data, data, filter_mode);
				break;
			}


			/* DopeSheet Based Modes */
			case ANIMCONT_DOPESHEET: /* 'DopeSheet Editor' */
			{
				/* the DopeSheet editor is the primary place where the DopeSheet summaries are useful */
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items += animdata_filter_dopesheet(ac, anim_data, data, filter_mode);
				break;
			}
			case ANIMCONT_FCURVES: /* Graph Editor -> F-Curves/Animation Editing */
			case ANIMCONT_DRIVERS: /* Graph Editor -> Drivers Editing */
			case ANIMCONT_NLA:     /* NLA Editor */
			{
				/* all of these editors use the basic DopeSheet data for filtering options, but don't have all the same features */
				items = animdata_filter_dopesheet(ac, anim_data, data, filter_mode);
				break;
			}


			/* Timeline Mode - Basically the same as dopesheet, except we only have the summary for now */
			case ANIMCONT_TIMELINE:
			{
				/* the DopeSheet editor is the primary place where the DopeSheet summaries are useful */
				if (animdata_filter_dopesheet_summary(ac, anim_data, filter_mode, &items))
					items += animdata_filter_dopesheet(ac, anim_data, data, filter_mode);
				break;
			}

			/* Special/Internal Use */
			case ANIMCONT_CHANNEL: /* animation channel */
			{
				bDopeSheet *ads = ac->ads;

				/* based on the channel type, filter relevant data for this */
				items = animdata_filter_animchan(ac, anim_data, ads, data, filter_mode);
				break;
			}

			/* unhandled */
			default:
			{
				printf("ANIM_animdata_filter() - Invalid datatype argument %u\n", datatype);
				break;
			}
		}

		/* remove any 'weedy' entries */
		items = animdata_filter_remove_invalid(anim_data);

		/* remove duplicates (if required) */
		if (filter_mode & ANIMFILTER_NODUPLIS)
			items = animdata_filter_remove_duplis(anim_data);
	}

	/* return the number of items in the list */
	return items;
}

/* ************************************************************ */
