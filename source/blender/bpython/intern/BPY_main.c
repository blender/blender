/**
 * blenkernel/py_main.c
 * (cleaned up somewhat nzc apr-2001)

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
 *
 */

/* NOTE: all externally callable routines have the prefix BPY_
 -- see also ../include/BPY_extern.h */

#include "BPY_main.h"
#include "BPY_modules.h"
#include "BPY_macros.h"
#include "DNA_space_types.h"

#include "b_interface.h"
#include "mydevice.h"
#include "import.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* PROTOS */

extern void init_frozenmodules(void);  // frozen module library
extern void initmxTextTools(void);
extern void inittess(void);            // tesselator module

void init_ourImport(void);


/* GLOBALS */

PyObject* ErrorObject     = NULL;
PyObject* callback        = NULL;
PyObject* callbackArgs    = NULL;
PyObject* blenderprogname = NULL;
ID*       script_link_id  = NULL;

/*------------------------------------------------------------------------*/ 
/* START PYTHON (from creator.c)                                          */

void INITMODULE(BLENDERMODULE)(void);

struct _inittab blendermodules[] = {
#ifndef SHAREDMODULE         // Blender module can alternatively be compiled shared
	#ifdef STATIC_TEXTTOOLS  // see api.h
	{ "mxTextTools" , initmxTextTools },
	#endif
	{ MODNAME(BLENDERMODULE) , INITMODULE(BLENDERMODULE) },
#endif
#ifdef NO_RELEASE
	{ "tess" , inittess }, // GLU tesselator wrapper module
#endif
	{ 0, 0}
};

/* hack to make sure, inittab is extended only first time */

static short g_is_extended = 0;

/** (Re)initializes the Python Interpreter.
  * This function should be only called if the Python interpreter
  * was not yet initialized (check Py_IsInitialized() )
  */

static void initBPythonInterpreter(void)
{
	Py_Initialize();

	init_ourImport(); /* our own import, later: security */
	if (!BPY_CHECKFLAG(G_NOFROZEN)) {
		init_frozenmodules(); /* initialize frozen modules unless disabled */
	}
	init_syspath();
}

/** This function initializes Blender Python. It should be called only
  * once at start, which is currently not the case (GameEngine Python).
  * Therefore, it contains some dirty workarounds. They will be thrown
  * into the grachten once the different APIs are merged into something
  * more consistent.
  *
  */

void BPY_start_python(void)
{
	Py_SetProgramName("blender");
	if (BPY_DEBUGFLAG) {

		Py_VerboseFlag = 1;
		Py_DebugFlag = 1;
	} else {
#ifndef EXPERIMENTAL 		
		Py_FrozenFlag = 1; /* no warnings about non set PYTHONHOME */
		Py_NoSiteFlag = 1; /* disable auto site module import */
#endif		
	}	

	if (!g_is_extended) {
		g_is_extended = 1;
		PyImport_ExtendInittab(blendermodules); /* extend builtin module table */
	}	

	initBPythonInterpreter();
#ifdef NO_RELEASE
	if (PyRun_SimpleString("import startup"))
	{
		BPY_warn(("init script not found, continuing anyway\n"));
		PyErr_Clear();
		return;
	}	
#endif	
}

/** Ends the Python interpreter. This cleans up all global variables
  * Blender-Python descriptor objects will (MUST!) decref on their
  * raw blender objects, so this function should be called more or less
  * immediately before garbage collection actions.
  */

void BPY_end_python(void)
{
	Py_Finalize();
}

void BPY_free_compiled_text(Text* text)
{
	if (!text->compiled) return;
	Py_DECREF((PyObject*) text->compiled);
	text->compiled = NULL;
}

void syspath_append(PyObject *dir)
{
	PyObject *m, *d;
	PyObject *o;

	PyErr_Clear();
	m = PyImport_ImportModule("sys");
	d = PyModule_GetDict(m);
	o = PyDict_GetItemString(d, "path");
	if (!PyList_Check(o)) {
		return;
	}
	PyList_Append(o, dir);
	if (PyErr_Occurred()) {
		Py_FatalError("could not build sys.path");
	}	
	Py_DECREF(m);
}

/* build blender specific system path for external modules */

void init_syspath(void)
{
	PyObject *path;
	PyObject *m, *d;
	PyObject *p;
	char *c;


	char execdir[PATH_MAXCHAR], *progname;

	int n;
	
	path = Py_BuildValue("s", bprogname);

	m = PyImport_ImportModule(MODNAME(BLENDERMODULE) ".sys");
	if (m) {
		d = PyModule_GetDict(m);
		PyDict_SetItemString(d, "progname", path);
		Py_DECREF(m);
	} else {
		BPY_debug(("Warning: could not set Blender.sys.progname\n"));
	}	

	progname = BLI_last_slash(bprogname); /* looks for the last dir separator */

	c = Py_GetPath(); /* get python system path */
	PySys_SetPath(c); /* initialize */

	n = progname - bprogname; 
	if (n > 0) {
		strncpy(execdir, bprogname, n);
		execdir[n] = '\0';

		p = Py_BuildValue("s", execdir);
		syspath_append(p);  /* append to module search path */

		/* set Blender.sys.progname */
	} else {	
		BPY_debug(("Warning: could not determine argv[0] path\n"));
	}
	/* TODO look for the blender executable in the search path */
	BPY_debug(("append to syspath: %s\n", U.pythondir));
	if (U.pythondir) {
		p = Py_BuildValue("s", U.pythondir);
		syspath_append(p);  /* append to module search path */
	}
	BPY_debug(("append done\n"));
}

/*****************************************************************************/
/* Description: This function adds the user defined folder for Python        */
/*              scripts to sys.path.  This is done in init_syspath, too, but */
/*              when Blender's main() runs BPY_start_python(), U.pythondir   */
/*              isn't set yet, so we provide this function to be executed    */
/*              after U.pythondir is defined.                                */
/*****************************************************************************/
void BPY_syspath_append_pythondir(void)
{
	syspath_append(Py_BuildValue("s", U.pythondir));
}

#define FILENAME_LENGTH 24
typedef struct _ScriptError {
	char filename[FILENAME_LENGTH];
	int lineno;
} ScriptError;

ScriptError g_script_error;

int BPY_Err_getLinenumber()
{
	return g_script_error.lineno;
}	

const char *BPY_Err_getFilename()
{
	return g_script_error.filename;
}

/** Returns (PyString) filename from a traceback object */

PyObject *traceback_getFilename(PyObject *tb)
{
	PyObject *v;

	v = PyObject_GetAttrString(tb, "tb_frame"); Py_DECREF(v);
	v = PyObject_GetAttrString(v, "f_code"); Py_DECREF(v);
	v = PyObject_GetAttrString(v, "co_filename"); 
	return v;
}

/** Blender Python error handler. This catches the error and stores
  * filename and line number in a global
  */

void BPY_Err_Handle(Text *text)
{
	PyObject *exception, *err, *tb, *v;

	PyErr_Fetch(&exception, &err, &tb);

	if (!exception && !tb) {
		printf("FATAL: spurious exception\n");
		return;
	}

	strcpy(g_script_error.filename, getName(text));

	if (exception && PyErr_GivenExceptionMatches(exception, PyExc_SyntaxError)) {
		// no traceback available when SyntaxError
		PyErr_Restore(exception, err, tb); // takes away reference!
		PyErr_Print();
		v = PyObject_GetAttrString(err, "lineno");
		g_script_error.lineno = PyInt_AsLong(v);
		Py_XDECREF(v);
		return; 
	} else {
		PyErr_NormalizeException(&exception, &err, &tb);
		PyErr_Restore(exception, err, tb); // takes away reference!
		PyErr_Print();
		tb = PySys_GetObject("last_traceback");
		Py_INCREF(tb);

// check traceback objects and look for last traceback in the
// same text file. This is used to jump to the line of where the
// error occured. If the error occured in another text file or module,
// the last frame in the current file is adressed

		while (1) {	
			v = PyObject_GetAttrString(tb, "tb_next");
			if (v == Py_None || 
			  strcmp(PyString_AsString(traceback_getFilename(v)), getName(text)))
				break;
			Py_DECREF(tb);
			tb = v;
		}

		v = PyObject_GetAttrString(tb, "tb_lineno");
		g_script_error.lineno = PyInt_AsLong(v);
		Py_XDECREF(v);
		v = traceback_getFilename(tb);
		strncpy(g_script_error.filename, PyString_AsString(v), FILENAME_LENGTH);
		Py_XDECREF(v);
		Py_DECREF(tb);
	}
}

/** Runs a Python string in the global name space of the given dictionary
    'globaldict' */

static PyObject *newGlobalDictionary(void)
{
	PyObject *d = PyDict_New();
	PyDict_SetItemString(d, "__builtins__", PyEval_GetBuiltins());
	PyDict_SetItemString(d, "__name__", PyString_FromString("__main__"));
	return d;
}	

static void releaseGlobalDictionary(PyObject *d)
{
	BPY_debug(("--- CLEAR namespace\n"));
	PyDict_Clear(d);
	Py_DECREF(d); // release dictionary
}

PyObject *BPY_runPython(Text *text, PyObject *globaldict)
{
	PyObject *ret;
	char* buf = NULL;

	if (!text->compiled)
	{
		buf = txt_to_buf(text);
		/* bah, what a filthy hack -- removed */
		/*      strcat(buf, "\n"); */
		text->compiled = Py_CompileString(buf, getName(text), Py_file_input);
		MEM_freeN(buf);
		if (PyErr_Occurred())
		{
			BPY_free_compiled_text(text);
			return 0;
		}
	}
	BPY_debug(("Run Python script \"%s\" ...\n", getName(text)));
	ret = PyEval_EvalCode(text->compiled, globaldict, globaldict);
	return ret;
}

/** This function is executed whenever ALT+PKEY is pressed -> drawtext.c 
    It returns the global namespace dictionary of the script context
	(which is created newly when CLEAR_NAMESPACE is defined).
	This may be stored in the SpaceText instance to give control over
	namespace persistence. Remember that the same script may be executed
	in several windows..
	Namespace persistence is desired for scripts that use the GUI and
	store callbacks to the current script.
*/

PyObject  *BPY_txt_do_python(SpaceText *st)
{
	PyObject* d = NULL;
	PyObject *ret;
	Text *text = st->text;

	if (!text)
	{
	  return NULL;
	}

	/* TODO: make this an option: */
#ifdef CLEAR_NAMESPACE	
	BPY_debug(("--- enable clear namespace\n"));
	st->flags |= ST_CLEAR_NAMESPACE;
#endif

#ifdef CLEAR_NAMESPACE
	d = newGlobalDictionary();
#else
	d = PyModule_GetDict(PyImport_AddModule("__main__"));
#endif	
	
	ret = BPY_runPython(text, d);

	if (!ret) {
#ifdef CLEAR_NAMESPACE			
		releaseGlobalDictionary(d);
#endif
		BPY_Err_Handle(text);
		Py_Finalize();
		initBPythonInterpreter();
		return NULL;
	}	
	else
		Py_DECREF(ret);


	/* The following lines clear the global name space of the python
	 * interpreter. This is desired to release objects after execution
	 * of a script (remember that each wrapper object increments the refcount
	 * of the Blender Object. 

	 * Exception: scripts that use the GUI rely on the
	 * persistent global namespace, so they need a workaround: The namespace
	 * is released when the GUI is exit.
	 * See opy_draw.c:Method_Register()
	 *
	 */

#ifdef CLEAR_NAMESPACE	
	if (st->flags & ST_CLEAR_NAMESPACE) {
		releaseGlobalDictionary(d);
		garbage_collect(getGlobal()->main); 
	}
#endif	
	
	return d;
}

/****************************************/
/* SCRIPTLINKS                          */

static void do_all_scriptlist(ListBase* list, short event)
{
	ID *id;

	id = list->first;
	while (id)
	{
		BPY_do_pyscript (id, event);
		id = id->next;
	}
}

void BPY_do_all_scripts(short event)
{
	do_all_scriptlist(getObjectList(), event);
	do_all_scriptlist(getLampList(),   event);
	do_all_scriptlist(getCameraList(), event);
	do_all_scriptlist(getMaterialList(), event);
	do_all_scriptlist(getWorldList(),  event);

	BPY_do_pyscript(&scene_getCurrent()->id, event);
}


char *event_to_name(short event)
{
	switch (event) {
	case SCRIPT_FRAMECHANGED:
		return "FrameChanged";
	case SCRIPT_ONLOAD:
		return "OnLoad";
	case SCRIPT_REDRAW:
		return "Redraw";
	default:
		return "Unknown";
	}
}	


void BPY_do_pyscript(ID *id, short event)
{
	int         i, offset;
	char        evName[24] = "";
	char*       structname = NULL;
	ScriptLink* scriptlink;
	PyObject *globaldict;

	switch(GET_ID_TYPE(id)) {
	case ID_OB: structname= "Object"; break;
	case ID_LA:  structname= "Lamp"; break;
	case ID_CA:  structname= "Camera"; break;
	case ID_MA:  structname= "Material"; break;
	case ID_WO:  structname= "World"; break;
	case ID_SCE: structname= "Scene"; break;
	default: return;
	}

	offset = BLO_findstruct_offset(structname, "scriptlink");
	if (offset < 0)
	{
		BPY_warn(("Internal error, unable to find script link\n"));
		return;
	}
	scriptlink = (ScriptLink*) (((char*)id) + offset);

	/* no script provided */
	if (!scriptlink->totscript) return;

/* Debugging output */
	switch (event)
	{
	case SCRIPT_FRAMECHANGED:
		strcpy(evName, "SCRIPT_FRAMECHANGED");
		BPY_debug(("do_pyscript(%s, %s)\n", getIDName(id), evName)); 
		break;
	case SCRIPT_ONLOAD:
		strcpy(evName, "SCRIPT_ONLOAD");
		BPY_debug(("do_pyscript(%s, %s)\n", getIDName(id), evName)); 
		break;
	case SCRIPT_REDRAW:
		strcpy(evName, "SCRIPT_REDRAW");
		BPY_debug(("do_pyscript(%s, %s)\n", getIDName(id), evName));
		break;
	default:
		BPY_debug(("do_pyscript(): This should not happen !!!")); 
		break;
	}

/* END DEBUGGING */
#ifndef SHAREDMODULE
	set_scriptlinks(id, event);
#endif
	disable_where_script(1);
	for (i = 0; i < scriptlink->totscript; i++)
	{
		if (scriptlink->flag[i] == event && scriptlink->scripts[i])
		{
			BPY_debug(("Evaluate script \"%s\" ...\n",
				getIDName(scriptlink->scripts[i])));
			script_link_id = id;
#ifdef CLEAR_NAMESPACE
			globaldict = newGlobalDictionary();
#else
			globaldict = PyModule_GetDict(PyImport_AddModule("__main__"));
#endif			
			BPY_runPython((Text*) scriptlink->scripts[i], globaldict);
#ifdef CLEAR_NAMESPACE
			releaseGlobalDictionary(globaldict);
#endif			
			
			script_link_id = NULL;
			BPY_debug(("... done\n"));
		}
	}
#ifndef SHAREDMODULE
	release_scriptlinks(id);
#endif
	disable_where_script(0);
}

void BPY_clear_bad_scriptlink(ID *id, Text *byebye)
{
	ScriptLink* scriptlink;
	int         offset = -1;
	char*       structname = NULL;
	int         i;

	switch (GET_ID_TYPE(id)) {
	case ID_OB:  structname = "Object"; break;
	case ID_LA:  structname = "Lamp"; break;
	case ID_CA:  structname = "Camera"; break;
	case ID_MA:  structname = "Material"; break;
	case ID_WO:  structname = "World"; break;
	case ID_SCE: structname = "Scene"; break;
	}

	if (!structname) return;

	offset= BLO_findstruct_offset(structname, "scriptlink");

	if (offset<0) return;

	scriptlink= (ScriptLink *) (((char *)id) + offset);

	for(i=0; i<scriptlink->totscript; i++)
	if ((Text*)scriptlink->scripts[i] == byebye)
		scriptlink->scripts[i] = NULL;
}

void BPY_clear_bad_scriptlist(ListBase *list, Text *byebye)
{
	ID *id;

	id= list->first;
	while (id)
	{
		BPY_clear_bad_scriptlink(id, byebye);
		id= id->next;
	}
}

void BPY_clear_bad_scriptlinks(Text *byebye)
{
	BPY_clear_bad_scriptlist(getObjectList(),	byebye);
	BPY_clear_bad_scriptlist(getLampList(),	byebye);
	BPY_clear_bad_scriptlist(getCameraList(),	byebye);
	BPY_clear_bad_scriptlist(getMaterialList(),	byebye);
	BPY_clear_bad_scriptlist(getWorldList(),	byebye);
	BPY_clear_bad_scriptlink(&scene_getCurrent()->id,    byebye);
	allqueue(REDRAWBUTSSCRIPT, 0);
}

void BPY_free_scriptlink(ScriptLink *slink)
{
	if (slink->totscript)
	{
		if(slink->flag) MEM_freeN(slink->flag);
		if(slink->scripts) MEM_freeN(slink->scripts);
	}
}

void BPY_copy_scriptlink(ScriptLink *scriptlink)
{
	void *tmp;

	if (scriptlink->totscript)
	{
		tmp = scriptlink->scripts;
		scriptlink->scripts = MEM_mallocN(sizeof(ID*)*scriptlink->totscript, "scriptlistL");
		memcpy(scriptlink->scripts, tmp, sizeof(ID*)*scriptlink->totscript);

		tmp = scriptlink->flag;
		scriptlink->flag = MEM_mallocN(sizeof(short)*scriptlink->totscript, "scriptlistF");
		memcpy(scriptlink->flag, tmp, sizeof(short)*scriptlink->totscript);
	}
}

/*
 *  Python alien graphics format conversion framework
 *
 *  $Id$
 *
 *	
 *
 */

/* import importloader module with registered importers */
#include "BPY_extern.h"
#include "Python.h"


int BPY_call_importloader(char *name)
{
	PyObject *mod, *tmp, *meth, *args;
	int i, success = 0;

	init_syspath();
	mod = PyImport_ImportModule("Converter.importloader");
	if (mod) {
		meth = PyObject_GetAttrString(mod, "process"); // new ref
		args = Py_BuildValue("(s)", name);
		tmp = PyEval_CallObject(meth, args);
		Py_DECREF(meth);
		if (PyErr_Occurred()) {
			PyErr_Print();
		}

		if (tmp) { 
			i = PyInt_AsLong(tmp);
			if (i) 
				success = 1;
			Py_DECREF(tmp);
		}
		Py_DECREF(mod);
	} else {
		PyErr_Print();
		BPY_warn(("couldn't import 'importloader' \n"));
	}
	return success;
}

// more to come...
