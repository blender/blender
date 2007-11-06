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
#include "api2_2x/gen_utils.h"
#include "BPY_extern.h"

#include "../SHD_util.h"

static PyObject *init_dynamicdict(void) {
	PyObject *newscriptdict= PyDict_New();
	PyDict_SetItemString(newscriptdict, "__builtins__", PyEval_GetBuiltins());
	EXPP_dict_set_item_str(newscriptdict, "__name__", PyString_FromString("__main__"));
	return newscriptdict;
}

static void free_dynamicdict(PyObject *dict) {
	if(dict!=NULL) {
		Py_DECREF(dict);
	}
}

static void node_dynamic_init(bNode *node) {
	NodeScriptDict *nsd= MEM_callocN(sizeof(NodeScriptDict), "node script dictionary");
	int type= node->custom2;
	node->custom2= 0;
	node->storage= nsd;
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
		if(node->custom1== SH_NODE_DYNAMIC_LOADED) {
			nodeMakeDynamicType(node);
			nodeDynamicParse(node);
		} else if(node->custom1== SH_NODE_DYNAMIC_ADDEXIST)
			nodeDynamicParse(node);
	}
}

static void node_dynamic_free(bNode *node)
{
	NodeScriptDict *nsd= (NodeScriptDict *)(node->storage);
	BPy_Node *pynode= nsd->node;
	Py_XDECREF(pynode);
	free_dynamicdict((PyObject *)(nsd->dict));
	MEM_freeN(node->storage);
}

static void node_dynamic_copy(bNode *orig_node, bNode *new_node)
{
	NodeScriptDict *nsd= (NodeScriptDict *)(orig_node->storage);
	new_node->storage= MEM_dupallocN(orig_node->storage);
	if(nsd->node)
		Py_INCREF((PyObject *)(nsd->node));
	if(nsd->dict)
		Py_INCREF((PyObject *)(nsd->dict));
}

static void node_dynamic_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) {
	BPy_Node *mynode = NULL;
	NodeScriptDict *nsd = NULL;
	PyObject *pyresult = NULL;
	PyObject *args = NULL;
	ShadeInput *shi= ((ShaderCallData *)data)->shi;

	if(node->custom1==SH_NODE_DYNAMIC_NEW) {
		nodeDynamicParse(node);
		return;
	}

	if(node->custom2<0)
		return;

	if(node->custom1==SH_NODE_DYNAMIC_READY || node->custom1==SH_NODE_DYNAMIC_UPDATED) {
		if(node->custom1== SH_NODE_DYNAMIC_UPDATED)
			node->custom1= SH_NODE_DYNAMIC_READY;

		nsd = (NodeScriptDict *)node->storage;

		mynode = (BPy_Node *)(nsd->node);
		if(mynode && PyCallable_Check((PyObject *)mynode)) {
			mynode->node= node;
			Node_SetStack(mynode, in, NODE_INPUTSTACK);
			Node_SetStack(mynode, out, NODE_OUTPUTSTACK);
			Node_SetShi(mynode, shi);
			args=Py_BuildValue("()");
			pyresult= PyObject_Call((PyObject *)mynode, args, NULL);
			if(!pyresult) {
				if(PyErr_Occurred()) {
					PyErr_Print();
					node->custom2= -1;
				} else {
					printf("PyObject_Call __call__ failed\n");
				}
			}
			Py_XDECREF(pyresult);
			Py_DECREF(args);
		}
	}
}

void nodeDynamicParse(struct bNode *node)
{
	BPy_Node *pynode= NULL;
	PyObject *dict= NULL;
	PyObject *key= NULL;
	PyObject *value= NULL;
	PyObject *testinst= NULL;
	PyObject *args= NULL;
	int pos = 0;
	NodeScriptDict *nsd= NULL;
	PyObject *pyresult = NULL;
	PyObject *pycompiled = NULL;
	Text *txt = NULL;
	char *buf= NULL;

	if(! node->id) {
		return;
	}

	if(node->custom1!=SH_NODE_DYNAMIC_READY) {
		txt = (Text *)node->id;
		nsd = (NodeScriptDict *)node->storage;

		if(nsd->dict==NULL && (node->custom1==SH_NODE_DYNAMIC_NEW||node->custom1==SH_NODE_DYNAMIC_LOADED)) {
			nsd->dict= init_dynamicdict();
		} else if(nsd->dict==NULL && node->custom1==SH_NODE_DYNAMIC_ADDEXIST) {
			nsd->dict= node->typeinfo->pydict;
			nsd->node= node->typeinfo->pynode;
			Py_INCREF((PyObject *)(nsd->dict));
			Py_INCREF((PyObject *)(nsd->node));
			node->custom1= SH_NODE_DYNAMIC_READY;
			return;
		}
		dict= (PyObject *)(nsd->dict);

		if(node->custom1!=SH_NODE_DYNAMIC_ADDEXIST) {
			buf = txt_to_buf( txt );
			/*printf("Running script (%s, %d)...", node->name, node->custom1);*/
			pyresult = PyRun_String(buf, Py_file_input, dict, dict);
			/*printf(" done\n");*/

			MEM_freeN(buf);

			if(!pyresult) {
				if(PyErr_Occurred()) {
					PyErr_Print();
				}
				Py_XDECREF(pyresult);
				return;
			}

			Py_DECREF(pyresult);

			while(PyDict_Next( (PyObject *)(nsd->dict), &pos, &key, &value) ) {
				if(PyObject_TypeCheck(value, &PyType_Type)==1) {
					BPy_DefinitionMap *outputdef= Node_CreateOutputDefMap(node);
					BPy_DefinitionMap *inputdef= Node_CreateInputDefMap(node);

					args= Py_BuildValue("(OO)", inputdef, outputdef);
					testinst= PyObject_Call(value, args, NULL);

					Py_DECREF(outputdef);
					Py_DECREF(inputdef);
					if(testinst && PyObject_TypeCheck(testinst, &Node_Type)==1) {
						Py_INCREF(testinst);
						Py_INCREF(dict);
						InitNode((BPy_Node *)(testinst), node);
						nsd->node= testinst;
						node->typeinfo->execfunc= node_dynamic_exec;
						if(node->custom1== SH_NODE_DYNAMIC_NEW || node->custom1== SH_NODE_DYNAMIC_LOADED) {
							node->typeinfo->pynode= testinst;
							node->typeinfo->pydict= nsd->dict;
							node->typeinfo->id= node->id;
							nodeAddSockets(node, node->typeinfo);
							nodeRegisterType(&node_all_shaders, node->typeinfo);
							node->custom1= SH_NODE_DYNAMIC_READY;
						}
						break;
					}
					Py_DECREF(args);
				}
			}
		}
	}
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
	/* butfunc     */	NULL,
	/* initfunc    */	node_dynamic_init,
	/* freefunc    */	node_dynamic_free,
	/* copyfunc    */	node_dynamic_copy,
	/* id          */	NULL
};

#endif /* USE_PYNODES */

