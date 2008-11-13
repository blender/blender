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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"

/* **************** COMPOSITE ******************** */
static bNodeSocketType inputs[]= {
	{ SOCK_RGBA,   1, "Color",  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ SOCK_VECTOR, 1, "Normal", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
	{ -1, 0, ""	}
};

/* applies to render pipeline */
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	TexCallData *cdata = (TexCallData *)data;
	TexResult *target = cdata->target;
	
	if(in[1]->hasinput && !in[0]->hasinput)
		tex_do_preview(node, in[1], data);
	else
		tex_do_preview(node, in[0], data);
	
	if(!cdata->do_preview) {
		if(cdata->which_output == node->custom1)
		{
			tex_input_rgba(&target->tr, in[0], cdata->coord, cdata->thread);
		
			target->tin = (target->tr + target->tg + target->tb) / 3.0f;
			target->talpha = 1.0f;
		
			if(target->nor) {
				if(in[1]->hasinput)
					tex_input_vec(target->nor, in[1], cdata->coord, cdata->thread);
				else
					target->nor = 0;
			}
		}
	}
}

static void init(bNode* node)
{
   TexNodeOutput *tno = MEM_callocN(sizeof(TexNodeOutput), "TEX_output");
   strcpy(tno->name, "Default");
   node->storage= tno;
}


bNodeType tex_node_output= {
	/* *next,*prev     */  NULL, NULL,
	/* type code       */  TEX_NODE_OUTPUT,
	/* name            */  "Output",
	/* width+range     */  150, 60, 200,
	/* class+opts      */  NODE_CLASS_OUTPUT, NODE_PREVIEW | NODE_OPTIONS, 
	/* input sock      */  inputs,
	/* output sock     */  NULL,
	/* storage         */  "TexNodeOutput",
	/* execfunc        */  exec,
	/* butfunc         */  NULL,
	/* initfunc        */  init,
	/* freestoragefunc */  node_free_standard_storage,
	/* copystoragefunc */  node_copy_standard_storage,  
	/* id              */  NULL
};
