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
<<<<<<< .mine
 * of the License, or (at your option) any later version.
=======
 * of the License, or (at your option) any later version. 
>>>>>>> .r13159
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

/* spacetype has everything stored to get an editor working, it gets initialized via 
spacetypes_init() in editors/area/spacetypes.c   */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData */

typedef struct SpaceType {
	struct SpaceType *next, *prev;
	
	char			name[32];					/* for menus */
	int				spaceid;					/* unique space identifier */
	int				iconid;						/* icon lookup for menus */
	
	struct SpaceLink	*(*new)(void);							/* calls init too */
	void		(*free)(struct SpaceLink *sl);					/* not free sl itself */
	
	void		(*init)(struct ScrArea *);						/* init is to cope with internal contextual changes, adds handlers, sets screarea regions */
	void		(*refresh)(struct bContext *, struct ScrArea *);	/* refresh is for external bContext changes */
	
	struct SpaceLink	*(*duplicate)(struct SpaceLink *sl);		/* after a spacedata copy, an init should result in exact same situation */
	
	/* read and write... */
	
} SpaceType;

/* region type gets allocated and freed in spacetype init/free callback */
/* data storage for regions is in space struct (also width/height of regions!) */
typedef struct ARegionType {
	
	void		(*init)(const struct bContext *, struct ARegion *);		/* add handlers, stuff you only do once or on area/region changes */
	void		(*refresh)(const struct bContext *, struct ARegion *);		/* refresh to match contextual changes */
	void		(*draw)(const struct bContext *, struct ARegion *);		/* draw entirely, windowsize changes should be handled here */
	
	void		(*listener)(struct ARegion *, struct wmNotifier *);
} ARegionType;


void BKE_screen_area_free(struct ScrArea *sa);
void free_screen(struct bScreen *sc); 

struct SpaceType *BKE_spacetype_from_id(int spaceid);
void BKE_spacetype_register(struct SpaceType *st);
void BKE_spacedata_freelist(ListBase *lb);
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2);

#endif

