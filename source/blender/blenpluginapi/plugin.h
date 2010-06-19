/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 * $Id$
 * 
 * All rights reserved.
 * 
 * Contact:      info@blender.org   
 * Information:  http://www.blender.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef PLUGIN_H
#define PLUGIN_H

#include "externdef.h"
#include "iff.h"
#include "util.h"
#include "floatpatch.h"

#define B_PLUGIN_VERSION	6

typedef	int (*TexDoit)(int, void*, float*, float*, float*, float*);
typedef	int (*TexDoitold)(int, void*, float*, float*, float*);
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

	void (*instance_init)(void *);
} PluginInfo;

LIBEXPORT int plugin_tex_getversion(void);
LIBEXPORT int plugin_seq_getversion(void);
LIBEXPORT void plugin_getinfo(PluginInfo *);

/* *************** defines for button types ************** */

#define CHA	32
#define INT	96
#define FLO	128

#define TOG	(3<<9)
#define	NUM	(5<<9)
#define LABEL	(10<<9)
#define NUMSLI	(14<<9)
#define COL	(15<<9)

/* return values (bitfield like) for textures (DNA_texture_types.h) */
#define TEX_INT         0
#define TEX_RGB         1
#define TEX_NOR         2

/* *************** API functions ******************** */

	/* derived from the famous Perlin noise */
LIBIMPORT float hnoise(float noisesize, float x, float y, float z);
	/* the original Perlin noise */
LIBIMPORT float hnoisep(float noisesize, float x, float y, float z);

	/* soft turbulence */
LIBIMPORT float turbulence(float noisesize, float x, float y, float z, int depth);
	/* hard turbulence */
LIBIMPORT float turbulence1(float noisesize, float x, float y, float z, int depth);

#endif /* PLUGIN_H */

