/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_EDITACTION_H
#define BIF_EDITACTION_H

#define SET_IPO_POPUP    0
#define SET_IPO_CONSTANT 1
#define SET_IPO_LINEAR   2
#define SET_IPO_BEZIER   3

#define SET_EXTEND_POPUP    0
#define SET_EXTEND_CONSTANT 1
#define SET_EXTEND_EXTRAPOLATION  2
#define SET_EXTEND_CYCLIC   3
#define SET_EXTEND_CYCLICEXTRAPOLATION   4

#define	CHANNELHEIGHT	16
#define	CHANNELSKIP		2
#define NAMEWIDTH      164
#define SLIDERWIDTH    125
#define ACTWIDTH 		(G.saction->actwidth)

/* Some types for easier type-testing */
#define ACTTYPE_NONE		0
#define ACTTYPE_ACHAN		1
#define ACTTYPE_CONCHAN		2
#define ACTTYPE_ICU			3
#define ACTTYPE_FILLIPO		4
#define ACTTYPE_FILLCON		5

/* Macros for easier/more consistant state testing */
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

struct bAction;
struct bActionChannel;
struct bPoseChannel;
struct Object;
struct Ipo;
struct BWinEvent;
struct Key;
struct ListBase;

/* Key operations */
void delete_meshchannel_keys(struct Key *key);
void delete_actionchannel_keys(void);
void duplicate_meshchannel_keys(struct Key *key);
void duplicate_actionchannel_keys(void);
void transform_actionchannel_keys(int mode, int dummy);
void transform_meshchannel_keys(char mode, struct Key *key);
void snap_keys_to_frame(int snap_mode);
void mirror_action_keys(short mirror_mode);
void clean_shapekeys(struct Key *key);
void clean_actionchannels(struct bAction *act);
void insertkey_action(void);

/* Marker Operations */
void column_select_shapekeys(struct Key *key, int mode);
void column_select_actionkeys(struct bAction *act, int mode);
void markers_selectkeys_between(void);

/* channel/strip operations */
void up_sel_action(void);
void down_sel_action(void);
void top_sel_action(void);
void bottom_sel_action(void);

/* Handles */
void sethandles_meshchannel_keys(int code, struct Key *key);
void sethandles_actionchannel_keys(int code);

/* Ipo type */ 
void set_ipotype_actionchannels(int ipotype);
void set_extendtype_actionchannels(int extendtype);

/* Select */
void borderselect_mesh(struct Key *key);
void borderselect_action(void);
void deselect_actionchannel_keys(struct bAction *act, int test, int sel);
void deselect_actionchannels (struct bAction *act, int test);
void deselect_meshchannel_keys (struct Key *key, int test, int sel);
int select_channel(struct bAction *act, struct bActionChannel *achan, int selectmode);
void select_actionchannel_by_name (struct bAction *act, char *name, int select);

/* */
struct Key *get_action_mesh_key(void);
int get_nearest_key_num(struct Key *key, short *mval, float *x);
void *get_nearest_act_channel(short mval[], short *ret_type);

/* Action */
struct bActionChannel* get_hilighted_action_channel(struct bAction* action);
struct bAction *add_empty_action(char *name);

void winqreadactionspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

/* contextual get action */
struct bAction *ob_get_action(struct Object *ob);

void remake_action_ipos(struct bAction *act);

/* this needs review badly! (ton) */
struct bAction *bake_action_with_client (struct bAction *act, struct Object *arm, float tolerance);
void world2bonespace(float boneSpaceMat[][4], float worldSpace[][4], float restPos[][4], float armPos[][4]);

#endif

