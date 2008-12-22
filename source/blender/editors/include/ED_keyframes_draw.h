/**
 * $Id: BDR_drawaction.h 17579 2008-11-26 11:01:56Z aligorith $
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BDR_DRAWACTION_H
#define BDR_DRAWACTION_H

struct BezTriple;
struct Ipo;
struct IpoCurve;
struct gla2DDrawInfo;
struct bAction;
struct bActionGroup;
struct bActListElem;
struct Object;
struct ListBase;
struct bGPDlayer;

/* ****************************** Base Structs ****************************** */

/* Keyframe Column Struct */
typedef struct ActKeyColumn {
	struct ActKeyColumn *next, *prev;
	short sel, handle_type;
	float cfra;
	
	/* only while drawing - used to determine if long-keyframe needs to be drawn */
	short modified;
	short totcurve;
} ActKeyColumn;

/* 'Long Keyframe' Struct */
typedef struct ActKeyBlock {
	struct ActKeyBlock *next, *prev;
	short sel, handle_type;
	float val;
	float start, end;
	
	/* only while drawing - used to determine if block needs to be drawn */
	short modified;
	short totcurve; 
} ActKeyBlock;


/* Inclusion-Range Limiting Struct (optional) */
typedef struct ActKeysInc {
	struct bDopeSheet *ads;			/* dopesheet data (for dopesheet mode) */
	struct Object *ob;				/* owner object for NLA-scaling info (if Object channels, is just Object) */
	short actmode;					/* mode of the Action Editor (-1 is for NLA) */
	
	float start, end;				/* frames (global-time) to only consider keys between */  // XXX not used anymore!
} ActKeysInc;

/* ******************************* Methods ****************************** */

/* Channel Drawing */
void draw_icu_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct IpoCurve *icu, float ypos);
void draw_ipo_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct Ipo *ipo, float ypos);
void draw_agroup_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bActionGroup *agrp, float ypos);
void draw_action_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bAction *act, float ypos);
void draw_object_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct Object *ob, float ypos);
void draw_gpl_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bGPDlayer *gpl, float ypos);

/* Keydata Generation */
void icu_to_keylist(struct IpoCurve *icu, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void ipo_to_keylist(struct Ipo *ipo, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void agroup_to_keylist(struct bActionGroup *agrp, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void action_to_keylist(struct bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void action_nlascaled_to_keylist(struct Object *ob, struct bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void ob_to_keylist(struct Object *ob, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void gpl_to_keylist(struct bGPDlayer *gpl, ListBase *keys, ListBase *blocks, ActKeysInc *aki);

#endif  /*  BDR_DRAWACTION_H */

