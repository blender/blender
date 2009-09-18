/**
 * blenlib/BKE_main.h (mar-2001 nzc)
 *
 * Main is the root of the 'database' of a Blender context. All data
 * is stuffed into lists, and all these lists are knotted to here. A
 * Blender file is not much more but a binary dump of these
 * lists. This list of lists is not serialized itself.
 *
 * Oops... this should be a _types.h file.
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
#ifndef BKE_MAIN_H
#define BKE_MAIN_H

#include "DNA_listBase.h"

struct Library;

typedef struct Main {
	struct Main *next, *prev;
	char name[240];
	short versionfile, subversionfile;
	short minversionfile, minsubversionfile;
	
	struct Library *curlib;
	ListBase scene;
	ListBase library;
	ListBase object;
	ListBase mesh;
	ListBase curve;
	ListBase mball;
	ListBase mat;
	ListBase tex;
	ListBase image;
	ListBase wave;
	ListBase latt;
	ListBase lamp;
	ListBase camera;
	ListBase ipo;	// XXX depreceated
	ListBase key;
	ListBase world;
	ListBase screen;
	ListBase script;
	ListBase vfont;
	ListBase text;
	ListBase sound;
	ListBase group;
	ListBase armature;
	ListBase action;
	ListBase nodetree;
	ListBase brush;
	ListBase particle;
	ListBase wm;
	ListBase gpencil;
} Main;


#endif

