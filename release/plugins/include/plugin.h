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

#ifndef PLUGIN_H
#define PLUGIN_H

#include "iff.h"
#include "util.h"
#include "floatpatch.h"

#define B_PLUGIN_VERSION	3

typedef	int (*TexDoit)(int, void*, float*, float*, float*);
typedef void (*SeqDoit)(void*, float, float, int, int, ImBuf*, ImBuf*, ImBuf*, ImBuf*);

typedef struct VarStruct {
	int type;
	char name[16];
	float def, min, max;
	char tip[80];
} VarStruct;

typedef struct _PluginInfo {
	char *name;
	char *snames;

	int stypes;
	int nvars;
	VarStruct *varstr;
	float *result;
	float *cfra;

	void (*init)(void);
	void (*callback)(int);
	TexDoit tex_doit;
	SeqDoit seq_doit;
} PluginInfo;

int plugin_tex_getversion(void);
int plugin_seq_getversion(void);
void plugin_getinfo(PluginInfo *);

/* *************** defines for button types ************** */

#define INT	96
#define FLO	128

#define TOG	(3<<9)
#define	NUM	(5<<9)
#define LABEL	(10<<9)
#define NUMSLI	(14<<9)


/* *************** API functions ******************** */

	/* derived from the famous Perlin noise */
extern float hnoise(float noisesize, float x, float y, float z);
	/* the original Perlin noise */
extern float hnoisep(float noisesize, float x, float y, float z);

	/* soft turbulence */
extern float turbulence(float noisesize, float x, float y, float z, int depth);
	/* hard turbulence */
extern float turbulence1(float noisesize, float x, float y, float z, int depth);

#endif /* PLUGIN_H */
