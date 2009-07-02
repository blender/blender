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

#ifndef ED_KEYFRAMES_DRAW_H
#define ED_KEYFRAMES_DRAW_H

struct BezTriple;
struct FCurve;
struct gla2DDrawInfo;
struct bAction;
struct bActionGroup;
struct Object;
struct ListBase;
struct bGPDlayer;
struct Scene;

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
void draw_fcurve_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct FCurve *fcu, float ypos);
void draw_agroup_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bActionGroup *agrp, float ypos);
void draw_action_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bAction *act, float ypos);
void draw_object_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct Object *ob, float ypos);
void draw_scene_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct Scene *sce, float ypos);
void draw_gpl_channel(struct gla2DDrawInfo *di, ActKeysInc *aki, struct bGPDlayer *gpl, float ypos);

/* Keydata Generation */
void fcurve_to_keylist(struct FCurve *fcu, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void agroup_to_keylist(struct bActionGroup *agrp, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void action_to_keylist(struct bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void action_nlascaled_to_keylist(struct Object *ob, struct bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void ob_to_keylist(struct Object *ob, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void scene_to_keylist(struct Scene *sce, ListBase *keys, ListBase *blocks, ActKeysInc *aki);
void gpl_to_keylist(struct bGPDlayer *gpl, ListBase *keys, ListBase *blocks, ActKeysInc *aki);

#endif  /*  ED_KEYFRAMES_DRAW_H */

