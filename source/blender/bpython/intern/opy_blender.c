/* python code for Blender, written by Daniel Dunbar */
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"

#include "BPY_macros.h"
#include "b_interface.h"
#include "BPY_tools.h"
#include "BPY_modules.h"
#include "BPY_main.h"

#include "BKE_main.h"
#include "BKE_object.h" // during_script() (what is this ?)
#include "BKE_global.h"
#include "BKE_ipo.h" // frame_to_float()
#include "BKE_blender.h"

#include "BIF_space.h" // allqueue()
#include "BSE_headerbuttons.h"

#include "BPY_types.h" // userdefined Python Objects
#include <string.h>

#include "DNA_userdef_types.h"

#include "mydevice.h"

//#include "py_blender.h"
//#include "screen.h"
//#include "ipo.h"

void INITMODULE(BLENDERMODULE)(void);

PyObject *g_sysmodule;


/*
NamedEnum TextureTypes[]= {
	{"Clouds",	TEX_CLOUDS}, 
	{"Wood",	TEX_WOOD}, 
	{"Marble",	TEX_MARBLE}, 
	{"Magic",	TEX_MAGIC}, 
	{"Blend",	TEX_BLEND}, 
	{"Stucci",	TEX_STUCCI}, 
	{"Noise",	TEX_NOISE}, 
	{"Image",	TEX_IMAGE}, 
	{"Plugin",	TEX_PLUGIN}, 
	{"Envmap",	TEX_ENVMAP}, 
	{NULL}
};

DataBlockProperty Texture_Properties[]= {
	DBP_NamedEnum("type",	"type",			TextureTypes), 
	DBP_Short("stype",		"stype",		0.0, 0.0,	0),  

	DBP_Float("noisesize",	"noisesize",	0.0, 0.0,	0),  
	DBP_Float("turbulence",	"turbul",		0.0, 0.0,	0), 
	DBP_Float("brightness",	"bright",		0.0, 0.0,	0),  
	DBP_Float("contrast",	"contrast",		0.0, 0.0,	0),  
	DBP_Float("rfac",		"rfac",			0.0, 0.0,	0),  
	DBP_Float("gfac",		"gfac",			0.0, 0.0,	0),  
	DBP_Float("bfac",		"bfac",			0.0, 0.0,	0),  
	DBP_Float("filtersize",	"filtersize",	0.0, 0.0,	0),  

	DBP_Short("noisedepth",	"noisedepth",	0.0, 0.0,	0),  
	DBP_Short("noisetype",	"noisetype",	0.0, 0.0,	0),  

	{NULL}	
};
*/


/*****************************/
/*  Main interface routines  */
/*****************************/


#define BP_CURFRAME 1
#define BP_CURTIME  2
#define BP_FILENAME 3

static char Blender_Set_doc[]=
"(request, data) - Update settings in Blender\n\
\n\
(request) A string indentifying the setting to change\n\
	'curframe'	- Sets the current frame using the number in data";

static PyObject *Blender_Set (PyObject *self, PyObject *args)
{
	char *name;
	PyObject *arg;
	
	BPY_TRY(PyArg_ParseTuple(args, "sO", &name, &arg));

	if (STREQ(name, "curframe")) {
		int framenum;
		
		BPY_TRY(PyArg_Parse(arg, "i", &framenum));
		
		(G.scene->r.cfra)= framenum;
		
		update_for_newframe();
		
	} else {
		return BPY_err_ret_ob(PyExc_AttributeError, "bad request identifier");
	}

	return BPY_incr_ret(Py_None);
}

static char Blender_Get_doc[]=
"(request) - Retrieve settings from Blender\n\
\n\
(request) A string indentifying the data to be returned\n\
	'curframe'	- Returns the current animation frame\n\
	'curtime'	- Returns the current animation time\n\
	'staframe'	- Returns the start frame of the animation\n\
	'endframe'	- Returns the end frame of the animation\n\
	'filename'	- Returns the name of the last file read or written\n\
	'version'	- Returns the Blender version number";

static PyObject *Blender_Get (PyObject *self, PyObject *args)
{	
	PyObject *ob;
	PyObject *dict;

	BPY_TRY(PyArg_ParseTuple(args, "O", &ob));

	if (PyString_Check(ob)) {
		char *str= PyString_AsString(ob);
		
		if (STREQ(str, "curframe")) 
			return PyInt_FromLong((G.scene->r.cfra));
			
		else if (STREQ(str, "curtime"))
			return PyFloat_FromDouble(frame_to_float((G.scene->r.cfra)));
			
		else if (STREQ(str, "staframe")) 
			return PyInt_FromLong((G.scene->r.sfra));
			
		else if (STREQ(str, "endframe")) 
			return PyInt_FromLong((G.scene->r.efra));
			
		else if (STREQ(str, "filename"))
			return PyString_FromString(getGlobal()->sce);

		else if (STREQ(str, "vrmloptions")) // TODO: quick hack, clean up!
		{
			dict= PyDict_New();
			PyDict_SetItemString(dict, "twoside", 
				PyInt_FromLong(U.vrmlflag & USERDEF_VRML_TWOSIDED));
			PyDict_SetItemString(dict, "layers", 
				PyInt_FromLong(U.vrmlflag & USERDEF_VRML_LAYERS));
			PyDict_SetItemString(dict, "autoscale", 
				PyInt_FromLong(U.vrmlflag & USERDEF_VRML_AUTOSCALE));
			return dict;
		}	
		else if (STREQ(str, "version"))
			return PyInt_FromLong(G.version);
	} else {
		return BPY_err_ret_ob(PyExc_AttributeError, "expected string argument");
	}

	return BPY_err_ret_ob(PyExc_AttributeError, "bad request identifier");
}

/* for compatibility: <<EOT */

static char Blender_Redraw_doc[]= "() - Redraw all 3D windows";

static PyObject *Blender_Redraw(PyObject *self, PyObject *args) 
{
	int wintype = SPACE_VIEW3D;

	BPY_TRY(PyArg_ParseTuple(args, "|i", &wintype));

	return Windowmodule_Redraw(self, Py_BuildValue("(i)", wintype));
}
/* EOT  */
	
#undef MethodDef
#define MethodDef(func) {#func, Blender_##func, METH_VARARGS, Blender_##func##_doc}

static struct PyMethodDef Blender_methods[] = {
	MethodDef(Redraw), 
	MethodDef(Get),
	MethodDef(Set), 
	{NULL, NULL}
};

struct PyMethodDef Null_methods[] = {
	{NULL, NULL}
};


static char Blender_Const_doc[] = 
"This module is only there for compatibility reasons.\n\
It will disappear in future, please use the Blender.Get() syntax instead\n";


/*	Blender system module
	Sorry, this is a mess, but had to be hacked in in order to meet deadlines..
	TODO: Cleanup
*/

static char sys_dirname_doc[] = 
	"(path) - returns the directory name of 'path'";

static PyObject *sys_dirname(PyObject *self, PyObject *args) 
{
	PyObject *c;

	char *name, dirname[256];
	char sep;
	int n;

	BPY_TRY(PyArg_ParseTuple(args, "s", &name));

	c = PyObject_GetAttrString(g_sysmodule, "dirsep");
	sep = PyString_AsString(c)[0];
	Py_DECREF(c);
	
	n = strrchr(name, sep) - name;
	if (n > 255) {
		PyErr_SetString(PyExc_RuntimeError, "path too long");
		return 0;
	}
	strncpy(dirname, name, n);
	dirname[n] = 0;

	return Py_BuildValue("s", dirname);
}

#define METHODDEF(func) {#func, sys_##func, METH_VARARGS, sys_##func##_doc}

static struct PyMethodDef sys_methods[] = {
	METHODDEF(dirname),
	{NULL, NULL}
};


PyObject *init_sys(void)
{

	PyObject *m, *dict, *p;

	m = Py_InitModule(SUBMODULE(sys), sys_methods);
	g_sysmodule = m;

	dict= PyModule_GetDict(m);

#ifdef WIN32
	p = Py_BuildValue("s", "\\");
#else
	p = Py_BuildValue("s", "/");
#endif
	
	PyDict_SetItemString(dict, "dirsep" , p);
	return m;
}

/* ----------------------------------------------------------------- */

void INITMODULE(BLENDERMODULE)(void)
{
	PyObject *m, *dict, *d;

	m = Py_InitModule4(MODNAME(BLENDERMODULE), Blender_methods, (char*) 0, (PyObject*)NULL, PYTHON_API_VERSION);

	DataBlock_Type.ob_type = &PyType_Type;


	init_py_vector();
	init_py_matrix();
	
	dict = PyModule_GetDict(m);
	g_blenderdict = dict;

	/* pyblender data type initialization */
	init_types(dict);
	init_Datablockmodules(dict);

	/* kept for compatibility...will be removed XXX */
	PyDict_SetItemString(dict, "bylink", PyInt_FromLong(0));
	PyDict_SetItemString(dict, "link",	Py_None);

	/* initialize submodules */
	PyDict_SetItemString(dict, "sys", init_sys());
	Py_INCREF(Py_None);
	PyDict_SetItemString(dict, "Image",	INITMODULE(Image)());
	PyDict_SetItemString(dict, "Window",INITMODULE(Window)());
	PyDict_SetItemString(dict, "NMesh", init_py_nmesh());
	PyDict_SetItemString(dict, "Draw",	init_py_draw());
	PyDict_SetItemString(dict, "BGL",	init_py_bgl());
#ifdef EXPERIMENTAL
	PyDict_SetItemString(dict, "Nurbs",	init_py_nurbs());
#endif
	/* CONSTANTS */

	/* emulate old python  XXX -> should go to Blender/ python externals */

		m = Py_InitModule4(MODNAME(BLENDERMODULE) ".Const" , Null_methods, Blender_Const_doc, (PyObject*)NULL, PYTHON_API_VERSION);
		d = PyModule_GetDict(m);
		PyDict_SetItemString(dict, "Const", m);
		PyDict_SetItemString(d, "BP_CURFRAME", PyInt_FromLong(BP_CURFRAME));
		PyDict_SetItemString(d, "BP_CURTIME", PyInt_FromLong(BP_CURTIME));

		PyDict_SetItemString(d, "CURFRAME", PyInt_FromLong(BP_CURFRAME));
		PyDict_SetItemString(d, "CURTIME",	PyInt_FromLong(BP_CURTIME));
		PyDict_SetItemString(d, "FILENAME", PyInt_FromLong(BP_FILENAME));
}

