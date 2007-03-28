/**
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef USE_PYNODES /* note: won't work without patch */

#include <Python.h>
#include <eval.h>

#include "DNA_text_types.h"
#include "BKE_text.h"

#include "api2_2x/Node.h"
#include "BPY_extern.h"

#include "../SHD_util.h"

/* This code is modelled after pyTexture by Timothy Wakeham.
 */
static PyObject *init_dynamicdict(void) {
	PyObject *newscriptdict;
	newscriptdict = PyDict_New();
	PyDict_SetItemString(newscriptdict, "__builtins__", PyEval_GetBuiltins());
	PyDict_SetItemString(newscriptdict, "__name__", PyString_FromString( "__main__" ));
	Py_INCREF(newscriptdict);
	return newscriptdict;
}

static void free_dynamicdict(PyObject *dict) {
	Py_XDECREF(dict);
	dict = NULL;
}

static void node_dynamic_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) {
	BPy_Node *mynode = NULL;
	BPy_ShadeInput *myshi = NULL;
	NodeScriptDict *nsd = NULL;
	BPy_SockInMap *inputs= NULL;
	BPy_SockOutMap *outputs= NULL;
	PyObject *pyresult = NULL;
	PyObject *args = NULL;
	ShadeInput *shi= ((ShaderCallData *)data)->shi;

	if(node->custom1==SH_NODE_DYNAMIC_READY) {
		nsd = (NodeScriptDict *)node->storage;

		mynode = (BPy_Node *)(nsd->node);
		myshi = (BPy_ShadeInput *)(nsd->shi);
		if(mynode && PyCallable_Check((PyObject *)mynode)) {
			mynode->node= node;
			inputs= Node_getInputs(mynode);
			inputs->inputs= in;
			outputs= Node_getOutputs(mynode);
			outputs->outputs= out;
			if(myshi) myshi->shi= shi;
			else printf("no shi ");
			/*printf("%f %f %f ", shi->lo[0], shi->lo[1], shi->lo[2]);*/
			args=Py_BuildValue("(NOO)",
						(PyObject *)myshi,
						(PyObject *)inputs,
						(PyObject *)outputs);
			pyresult= PyObject_Call((PyObject *)mynode, args, NULL);
			if(!pyresult) {
				PyErr_Print();
			}
			Py_XDECREF(pyresult);
			Py_DECREF(args);
			PyObject_Del(inputs);
			PyObject_Del(outputs);
			/*printf(".");*/
		}
	}
}

void nodeDynamicParse(struct bNode *node)
{
	BPy_Node *pynode= NULL;
	BPy_ShadeInput *myshi = NULL;
	PyObject *outputdef= NULL;
	PyObject *inputdef= NULL;
	PyObject *key= NULL;
	PyObject *value= NULL;
	PyObject *testinst= NULL;
	PyObject *args= NULL;
	int pos = 0;
	NodeScriptDict *nsd;
	PyObject *pyresult = NULL;
	Text *txt = 0;
	char *buf;

	if(! node->id) {
		return;
	}

	if(node->custom1!=SH_NODE_DYNAMIC_READY) {
		txt = (Text *)node->id;
		nsd = (NodeScriptDict *)node->storage;

		buf = txt_to_buf( txt );
		
		printf("nsd %p, nsd->dict %p, buf %p\n", nsd, nsd->dict, buf);
		printf("Running script...");
		pyresult = PyRun_String(buf, Py_file_input, (PyObject *)(nsd->dict), (PyObject *)(nsd->dict));
		printf(" done\n");

		MEM_freeN(buf);

		if(!pyresult) {
			if(PyErr_Occurred()) {
				PyErr_Print();
			}
			Py_XDECREF(pyresult);
			return;
		}

		Py_DECREF(pyresult);
		/*PyObject_Del(pyresult);*/

		myshi=(BPy_ShadeInput *)ShadeInput_CreatePyObject(NULL);
		nsd->shi= myshi;
		while(PyDict_Next( (PyObject *)(nsd->dict), &pos, &key, &value) ) {
			if(PyObject_TypeCheck(value, &PyType_Type)) {
				pynode = (BPy_Node *)Node_CreatePyObject(node);
				outputdef= Node_CreateOutputDefMap(pynode);
				inputdef= Node_CreateInputDefMap(pynode);
				args= Py_BuildValue("(OO)", inputdef, outputdef);
				testinst= PyObject_Call(value, args, NULL);
				if(testinst && PyObject_TypeCheck(testinst, &Node_Type)==1) {
					nsd->node= testinst;
					node->typeinfo->execfunc= node_dynamic_exec;
					if(node->custom1== SH_NODE_DYNAMIC_NEW) {
						node->typeinfo->pynode= testinst;
						node->typeinfo->id= node->id;
						nodeRegisterType(&node_all_shaders, node->typeinfo);
						node->custom1= SH_NODE_DYNAMIC_CREATED;
					} else if(node->custom1== SH_NODE_DYNAMIC_REPARSE) {
						node->typeinfo->pynode= testinst;
						node->typeinfo->id= node->id;
						/*ntreeUpdateType(ntree, node->typeinfo);*/
						/* NEED TO UPDATE ALL TREES WITH NEW TYPEINFO */
						node->custom1= SH_NODE_DYNAMIC_UPDATED;
					} else if(node->custom1== SH_NODE_DYNAMIC_LOADED) {
						node->typeinfo->pynode= testinst;
						node->typeinfo->id= node->id;
						nodeRegisterType(&node_all_shaders, node->typeinfo);
						node->custom1= SH_NODE_DYNAMIC_READY;
					} else if(node->custom1== SH_NODE_DYNAMIC_ADDEXIST) {
						node->custom1= SH_NODE_DYNAMIC_READY;
					}
					break;
				}
			}
		}
	}
}

static void node_dynamic_init(bNode* node) {
	NodeScriptDict *nsd = MEM_callocN(sizeof(NodeScriptDict), "node script dictionary");
	int type = node->custom2;
	node->custom2= 0;
	node->storage = nsd;
	nsd->dict = init_dynamicdict(); /* each node has own dict */
	if(type>=NODE_DYNAMIC_MENU) {
		if(type==NODE_DYNAMIC_MENU) {
			nodeMakeDynamicType(node);
			node->custom1= SH_NODE_DYNAMIC_NEW;
		} else {
			node->custom1= SH_NODE_DYNAMIC_ADDEXIST;
		}
		node->id= node->typeinfo->id;
		nodeDynamicParse(node);
	} else {
		node->custom1= SH_NODE_DYNAMIC_NEW;
	}
}

static void node_dynamic_free(bNode *node)
{
	NodeScriptDict *nsd = (NodeScriptDict *)(node->storage);
	free_dynamicdict((PyObject *)(nsd->dict));
	MEM_freeN(node->storage);
}

bNodeType sh_node_dynamic = {
	/* next, prev  */	NULL, NULL,
	/* type code   */	SH_NODE_DYNAMIC,
	/* name        */	"Dynamic",
	/* width+range */	150, 60, 300,
	/* class+opts  */	NODE_CLASS_OP_DYNAMIC, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	NULL,
	/* storage     */	"NodeScriptDict",
	/* execfunc    */	node_dynamic_exec,
	/* butfunc     */   NULL,
	/* initfunc    */   node_dynamic_init,
	/* freefunc    */	node_dynamic_free,
	/* id          */	NULL
};

#endif /* USE_PYNODES */

