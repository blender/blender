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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2007, Joshua Leung, Action Editor Recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_EDITACTION_H
#define BIF_EDITACTION_H

/* some interface related sizes*/
#define	CHANNELHEIGHT	16
#define	CHANNELSKIP		2
#define NAMEWIDTH      	164
#define SLIDERWIDTH    	125
#define ACTWIDTH 		(G.saction->actwidth)

/* Some types for easier type-testing */
enum {
	ACTTYPE_NONE= 0,
	ACTTYPE_GROUP,
	ACTTYPE_ACHAN,
	ACTTYPE_CONCHAN,
	ACTTYPE_CONCHAN2,
	ACTTYPE_ICU,
	ACTTYPE_FILLIPO,
	ACTTYPE_FILLCON,
	ACTTYPE_IPO,
	ACTTYPE_SHAPEKEY,
	ACTTYPE_GPDATABLOCK,
	ACTTYPE_GPLAYER,
	ACTTYPE_SPECIALDATA
};

/* Macros for easier/more consistant state testing */
#define EDITABLE_AGRP(agrp) ((agrp->flag & AGRP_PROTECTED)==0)
#define EXPANDED_AGRP(agrp) (agrp->flag & AGRP_EXPANDED)
#define SEL_AGRP(agrp) ((agrp->flag & AGRP_SELECTED) || (agrp->flag & AGRP_ACTIVE))

#define VISIBLE_ACHAN(achan) ((achan->flag & ACHAN_HIDDEN)==0)
#define EDITABLE_ACHAN(achan) ((VISIBLE_ACHAN(achan)) && ((achan->flag & ACHAN_PROTECTED)==0))
#define EXPANDED_ACHAN(achan) ((VISIBLE_ACHAN(achan)) && (achan->flag & ACHAN_EXPANDED))
#define SEL_ACHAN(achan) ((achan->flag & ACHAN_SELECTED) || (achan->flag & ACHAN_HILIGHTED))
#define FILTER_IPO_ACHAN(achan) ((achan->flag & ACHAN_SHOWIPO))
#define FILTER_CON_ACHAN(achan) ((achan->flag & ACHAN_SHOWCONS))

#define EDITABLE_CONCHAN(conchan) ((conchan->flag & CONSTRAINT_CHANNEL_PROTECTED)==0)
#define SEL_CONCHAN(conchan) (conchan->flag & CONSTRAINT_CHANNEL_SELECT)

#define EDITABLE_ICU(icu) ((icu->flag & IPO_PROTECT)==0)
#define SEL_ICU(icu) (icu->flag & IPO_SELECT)

#define EXPANDED_GPD(gpd) (gpd->flag & GP_DATA_EXPAND) 

#define EDITABLE_GPL(gpl) ((gpl->flag & GP_LAYER_LOCKED)==0)
#define SEL_GPL(gpl) ((gpl->flag & GP_LAYER_ACTIVE) || (gpl->flag & GP_LAYER_SELECT))

#define NLA_ACTION_SCALED (G.saction->mode==SACTCONT_ACTION && G.saction->pin==0 && OBACT && OBACT->action)
#define NLA_IPO_SCALED (OBACT && OBACT->action && G.sipo->pin==0 && G.sipo->actname)

/* constants for setting ipo-interpolation type */
enum {
	SET_IPO_MENU = -1,
	SET_IPO_POPUP = 0,
	
	SET_IPO_CONSTANT,
	SET_IPO_LINEAR,
	SET_IPO_BEZIER,
};

/* constants for setting ipo-extrapolation type */
enum {
	SET_EXTEND_MENU = 9,
	SET_EXTEND_POPUP = 10,
	
	SET_EXTEND_CONSTANT,
	SET_EXTEND_EXTRAPOLATION,
	SET_EXTEND_CYCLIC,
	SET_EXTEND_CYCLICEXTRAPOLATION
};

/* constants for channel rearranging */
/* WARNING: don't change exising ones without modifying rearrange func accordingly */
enum {
	REARRANGE_ACTCHAN_TOP= -2,
	REARRANGE_ACTCHAN_UP= -1,
	REARRANGE_ACTCHAN_DOWN= 1,
	REARRANGE_ACTCHAN_BOTTOM= 2
};


struct bAction;
struct bActionChannel;
struct bActionGroup;
struct bPose;
struct bPoseChannel;
struct Object;
struct Ipo;
struct BWinEvent;
struct Key;
struct ListBase;
struct TimeMarker;
struct bGPdata;
struct bGPDlayer;

/* Key operations */
void transform_action_keys(int mode, int dummy);
void duplicate_action_keys(void);
void snap_cfra_action(void);
void snap_action_keys(short mode);
void mirror_action_keys(short mode);
void insertkey_action(void);
void delete_action_keys(void);
void delete_action_channels(void);
void clean_action(void);
void sample_action_keys(void);

/* Column/Channel Key select */
void column_select_action_keys(int mode);
void selectall_action_keys(short mval[], short mode, short selectmode);
void markers_selectkeys_between(void);
void nextprev_action_keyframe(short dir);

/* Action Data Copying */
void free_actcopybuf(void);
void copy_actdata(void);
void paste_actdata(void);

/* Group/Channel Operations */
struct bActionGroup *get_active_actiongroup(struct bAction *act);
void set_active_actiongroup(struct bAction *act, struct bActionGroup *agrp, short select);
void actionbone_group_copycolors(struct bActionGroup *grp, short init_new);
void verify_pchan2achan_grouping(struct bAction *act, struct bPose *pose, char name[]);
void sync_pchan2achan_grouping(void); 
void action_groups_group(short add_group);
void action_groups_ungroup(void);

/* Channel/Strip Operations */
void rearrange_action_channels(short mode);

void expand_all_action(void);
void expand_obscuregroups_action(void);
void openclose_level_action(short mode);
void setflag_action_channels(short mode);

/* IPO/Handle Types  */
void sethandles_action_keys(int code);
void action_set_ipo_flags(short mode, short event);

/* Select */
void borderselect_action(void);
void borderselect_actionchannels(void);
void deselect_action_keys(short test, short sel);
void deselect_action_channels(short mode);
void deselect_actionchannels(struct bAction *act, short mode);
int select_channel(struct bAction *act, struct bActionChannel *achan, int selectmode);
void select_actionchannel_by_name(struct bAction *act, char *name, int select);
void select_action_group_channels(struct bAction *act, struct bActionGroup *agrp);
void selectkeys_leftright (short leftright, short select_mode);

/* Action Markers */
void action_set_activemarker(struct bAction *act, struct TimeMarker *active, short deselect);
void action_add_localmarker(struct bAction *act, int frame);
void action_rename_localmarker(struct bAction *act);
void action_remove_localmarkers(struct bAction *act);

/* Grease-Pencil Data */
void gplayer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, short onlysel);

void deselect_gpencil_layers(void *data, short select_mode);

short is_gplayer_frame_selected(struct bGPDlayer *gpl);
void set_gplayer_frame_selection(struct bGPDlayer *gpl, short mode);
void select_gpencil_frames(struct bGPDlayer *gpl, short select_mode);
void select_gpencil_frame(struct bGPDlayer *gpl, int selx, short select_mode);
void borderselect_gplayer_frames(struct bGPDlayer *gpl, float min, float max, short select_mode);

void delete_gpencil_layers(void);
void delete_gplayer_frames(struct bGPDlayer *gpl);
void duplicate_gplayer_frames(struct bGPDlayer *gpd);

void snap_gplayer_frames(struct bGPDlayer *gpl, short mode);
void mirror_gplayer_frames(struct bGPDlayer *gpl, short mode);

/* ShapeKey stuff */
struct Key *get_action_mesh_key(void);
int get_nearest_key_num(struct Key *key, short *mval, float *x);

void *get_nearest_act_channel(short mval[], short *ret_type, void **owner);

/* Action */
struct bActionChannel *get_hilighted_action_channel(struct bAction* action);
struct bAction *add_empty_action(char *name);
struct bAction *ob_get_action(struct Object *ob);

void actdata_filter(ListBase *act_data, int filter_mode, void *data, short datatype);
void *get_action_context(short *datatype);

void remake_action_ipos(struct bAction *act);
void action_previewrange_set(struct bAction *act);

/* event handling */
void winqreadactionspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

#endif

