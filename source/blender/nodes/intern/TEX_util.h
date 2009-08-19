/**
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef TEX_NODE_UTIL_H_
#define TEX_NODE_UTIL_H_

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_ipo_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_library.h"

#include "../SHD_node.h"
#include "node_util.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

typedef struct TexCallData {
	TexResult *target;
	float *coord;
	float *dxt, *dyt;
	char do_preview;
	short thread;
	short which_output;
	int cfra;
} TexCallData;

typedef struct TexParams {
	float *coord;
	float *dxt, *dyt;
	int cfra;
} TexParams;

typedef void(*TexFn) (float *out, TexParams *params, bNode *node, bNodeStack **in, short thread);

typedef struct TexDelegate {
	TexFn fn;
	bNode *node;
	bNodeStack *in[MAX_SOCKET];
	int type;
} TexDelegate;

void tex_call_delegate(TexDelegate*, float *out, TexParams *params, short thread);

void tex_input_rgba(float *out, bNodeStack *in, TexParams *params, short thread);
void tex_input_vec(float *out, bNodeStack *in, TexParams *params, short thread);
float tex_input_value(bNodeStack *in, TexParams *params, short thread);

void tex_output(bNode *node, bNodeStack **in, bNodeStack *out, TexFn texfn);
void tex_do_preview(bNode *node, bNodeStack *ns, TexCallData *cdata);

void ntreeTexExecTree(bNodeTree *nodes, TexResult *texres, float *coord, float *dxt, float *dyt, short thread, struct Tex *tex, short which_output, int cfra);

void params_from_cdata(TexParams *out, TexCallData *in);

#endif
