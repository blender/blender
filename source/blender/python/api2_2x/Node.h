/* 
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
 * The Original Code is Copyright (C) 2006, Blender Foundation
 * All rights reserved.
 *
 * Original code is this file
 *
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef __NODE_H__
#define __NODE_H__

#include <Python.h>
#include "DNA_node_types.h"
#include "BKE_node.h"

#include "RE_shader_ext.h"		/* <- ShadeInput Shaderesult TexResult */

#ifndef SH_NODE_SCRIPT_READY
#define SH_NODE_SCRIPT_READY	0
#define SH_NODE_SCRIPT_LOADED	1
#define SH_NODE_SCRIPT_REPARSE	2
#define SH_NODE_SCRIPT_NEW	3
#define SH_NODE_SCRIPT_CREATED	4
#define SH_NODE_SCRIPT_UPDATED	5
#define SH_NODE_SCRIPT_ADDEXIST	6
#endif

extern PyTypeObject Node_Type;
extern PyTypeObject ShadeInput_Type;

#define BPy_Node_Check(v) \
    ((v)->ob_type == &Node_Type)

#define BPy_ShadeInput_Check(v) \
    ((v)->ob_type == &ShadeInput_Type)

typedef struct BPy_Node {
	PyObject_HEAD
	bNode *node;
	bNodeStack **inputs;
	bNodeStack **outputs;
	int altered;
} BPy_Node;

typedef struct BPy_ShadeInput {
	PyObject_HEAD
	ShadeInput *shi;
} BPy_ShadeInput;

typedef struct {
	PyObject_VAR_HEAD /* required python macro */
	bNodeType *typeinfo;
	bNodeStack **inputs;
} BPy_SockInMap;

typedef struct {
	PyObject_VAR_HEAD /* required python macro */
	bNodeType *typeinfo;
	bNodeStack **outputs;
} BPy_SockOutMap;

typedef struct {
	PyObject_HEAD /* required python macro */
	bNode *node;
	bNodeStack **outputs;
} BPy_OutputDefMap;

typedef struct {
	PyObject_HEAD /* required python macro */
	bNode *node;
	bNodeStack **outputs;
} BPy_InputDefMap;

extern PyObject *Node_Init(void);
extern BPy_Node *Node_CreatePyObject(bNode *node);
extern PyObject *Node_CreateOutputDefMap(BPy_Node *self);
extern PyObject *Node_CreateInputDefMap(BPy_Node *self);
extern BPy_ShadeInput *ShadeInput_CreatePyObject(ShadeInput *shi);
extern BPy_SockInMap *Node_getInputs(BPy_Node *self);
extern BPy_SockOutMap *Node_getOutputs(BPy_Node *self);
extern void Node_dealloc(BPy_Node *self);
extern void ShadeInput_dealloc(BPy_ShadeInput *self);

#endif /* __NODE_H__*/
