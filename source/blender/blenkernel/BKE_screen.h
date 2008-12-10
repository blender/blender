/**
 * blenlib/BKE_screen.h (mar-2001 nzc)
 *	
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SCREEN_H
#define BKE_SCREEN_H

struct SpaceType;
struct ScrArea;
struct bScreen;
struct ARegion;
struct wmNotifier;
struct wmWindowManager;
struct ListBase;

/* spacetype has everything stored to get an editor working, it gets initialized via 
   ED_spacetypes_init() in editors/area/spacetypes.c   */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData */

#define BKE_ST_MAXNAME	32

typedef struct SpaceType {
	struct SpaceType *next, *prev;
	
	char			name[BKE_ST_MAXNAME];		/* for menus */
	int				spaceid;					/* unique space identifier */
	int				iconid;						/* icon lookup for menus */
	
	/* initial allocation, after this WM will call init() too */
	struct SpaceLink	*(*new)(void);
	/* not free spacelink itself */
	void		(*free)(struct SpaceLink *);
	
	/* init is to cope with file load, screen (size) changes, check handlers */
	void		(*init)(struct wmWindowManager *, struct ScrArea *);
	/* Listeners can react to bContext changes */
	void		(*listener)(struct ARegion *, struct wmNotifier *);
	
	/* after a spacedata copy, an init should result in exact same situation */
	struct SpaceLink	*(*duplicate)(struct SpaceLink *);

	/* register operator types on startup */
	void		(*operatortypes)(void);
	/* add default items to WM keymap */
	void		(*keymap)(struct wmWindowManager *);

	/* region type definitions */
	ListBase	regiontypes;
	
	/* read and write... */
	
	/* default keymaps to add */
	int			keymapflag;
	
} SpaceType;

/* region types are also defined using spacetypes_init, via a callback */

typedef struct ARegionType {
	struct ARegionType *next, *prev;
	
	int			regionid;	/* unique identifier within this space */
	
	/* add handlers, stuff you only do once or on area/region type/size changes */
	void		(*init)(struct wmWindowManager *, struct ARegion *);
	/* draw entirely, view changes should be handled here */
	void		(*draw)(const struct bContext *, struct ARegion *);	
	/* contextual changes should be handled here */
	void		(*listener)(struct ARegion *, struct wmNotifier *);
	
	void		(*free)(struct ARegion *);

	/* register operator types on startup */
	void		(*operatortypes)(void);
	/* add own items to keymap */
	void		(*keymap)(struct wmWindowManager *);
	
	/* hardcoded constraints, smaller than these values region is not visible */
	int			minsizex, minsizey;
	/* default keymaps to add */
	int			keymapflag;
} ARegionType;


void BKE_screen_area_free(struct ScrArea *sa);
void BKE_area_region_free(struct ARegion *ar);
void free_screen(struct bScreen *sc); 

/* spacetypes */
struct SpaceType *BKE_spacetype_from_id(int spaceid);
const struct ListBase *BKE_spacetypes_list(void);
void BKE_spacetype_register(struct SpaceType *st);
void BKE_spacetypes_free(void);	/* only for quitting blender */

/* spacedata */
void BKE_spacedata_freelist(ListBase *lb);
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2);

#endif

