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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MAIN_H__
#define __BKE_MAIN_H__

/** \file BKE_main.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 *  \section aboutmain Main struct
 * Main is the root of the 'database' of a Blender context. All data
 * is stuffed into lists, and all these lists are knotted to here. A
 * Blender file is not much more but a binary dump of these
 * lists. This list of lists is not serialized itself.
 *
 * Oops... this should be a _types.h file.
 *
 */
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Library;

typedef struct Main {
	struct Main *next, *prev;
	char name[1024]; /* 1024 = FILE_MAX */
	short versionfile, subversionfile;
	short minversionfile, minsubversionfile;
	int revision;   /* svn revision of binary that saved file */
	
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
	ListBase latt;
	ListBase lamp;
	ListBase camera;
	ListBase ipo;   // XXX depreceated
	ListBase key;
	ListBase world;
	ListBase screen;
	ListBase script;
	ListBase vfont;
	ListBase text;
	ListBase speaker;
	ListBase sound;
	ListBase group;
	ListBase armature;
	ListBase action;
	ListBase nodetree;
	ListBase brush;
	ListBase particle;
	ListBase wm;
	ListBase gpencil;
	ListBase movieclip;

	char id_tag_update[256];
} Main;


#ifdef __cplusplus
}
#endif

#endif

