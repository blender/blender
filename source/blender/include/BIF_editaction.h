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
 * Contributor(s): 2007, Joshua Leung, Action Editor Recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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
	ACTTYPE_ACHAN,
	ACTTYPE_CONCHAN,
	ACTTYPE_ICU,
	ACTTYPE_FILLIPO,
	ACTTYPE_FILLCON,
	ACTTYPE_IPO,
	ACTTYPE_SHAPEKEY
};

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

#define NLA_ACTION_SCALED (G.saction->pin==0 && OBACT && OBACT->action)
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

struct bAction;
struct bActionChannel;
struct bPoseChannel;
struct Object;
struct Ipo;
struct BWinEvent;
struct Key;
struct ListBase;

/* Key operations */
void transform_action_keys(int mode, int dummy);
void duplicate_action_keys(void);
void snap_action_keys(short mode);
void mirror_action_keys(short mode);
void insertkey_action(void);
void delete_action_keys(void);
void delete_action_channels(void);
void clean_action(void);

/* Column/Channel Key select */
void column_select_action_keys(int mode);
void selectall_action_keys(short mval[], short mode, short selectmode);
void markers_selectkeys_between(void);

/* channel/strip operations */
void up_sel_action(void);
void down_sel_action(void);
void top_sel_action(void);
void bottom_sel_action(void);

/* IPO/Handle Types  */
void sethandles_action_keys(int code);
void action_set_ipo_flags(short mode, short event);

/* Select */
void borderselect_action(void);
void deselect_action_keys(short test, short sel);
void deselect_action_channels(short test);
void deselect_actionchannels(struct bAction *act, short test);
int select_channel(struct bAction *act, struct bActionChannel *achan, int selectmode);
void select_actionchannel_by_name (struct bAction *act, char *name, int select);

/* ShapeKey stuff */
struct Key *get_action_mesh_key(void);
int get_nearest_key_num(struct Key *key, short *mval, float *x);
void *get_nearest_act_channel(short mval[], short *ret_type);

/* Action */
struct bActionChannel *get_hilighted_action_channel(struct bAction* action);
struct bAction *add_empty_action(char *name);
struct bAction *ob_get_action(struct Object *ob);

void actdata_filter(ListBase *act_data, int filter_mode, void *data, short datatype);
void *get_action_context(short *datatype);

void remake_action_ipos(struct bAction *act);

/* event handling */
void winqreadactionspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

#endif

