/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Python.h>
#include "compile.h" /* for the PyCodeObject */
#include "eval.h" /* for PyEval_EvalCode */

#include <stdio.h>

#include <MEM_guardedalloc.h>
#include <BLI_blenlib.h> /* for BLI_last_slash() */

#include <BIF_interface.h> /* for pupmenu */
#include <BIF_space.h>
#include <BIF_screen.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_main.h>
#include <BKE_text.h>
#include <BKE_utildefines.h>
#include <DNA_camera_types.h>
#include <DNA_ID.h>
#include <DNA_lamp_types.h>
#include <DNA_material_types.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_screen_types.h>
#include <DNA_script_types.h>
#include <DNA_scriptlink_types.h>
#include <DNA_space_types.h>
#include <DNA_text_types.h>
#include <DNA_world_types.h>
#include <DNA_userdef_types.h> /* for U.pythondir */

#include "BPY_extern.h"
#include "BPY_menus.h"
#include "api2_2x/EXPP_interface.h"
#include "api2_2x/constant.h"

/* bpy_registryDict is declared in api2_2x/Registry.h and defined
 * here.  This Python dictionary will be used to store data that scripts
 * choose to preserve after they are executed, so user changes can be
 * restored next time the script is used.  Check the Blender.Registry module. */
extern PyObject *bpy_registryDict;

/*****************************************************************************/
/* Structure definitions                                                     */
/*****************************************************************************/
#define FILENAME_LENGTH 24
typedef struct _ScriptError {
  char filename[FILENAME_LENGTH];
  int lineno;
} ScriptError;

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
ScriptError g_script_error;
short EXPP_releaseGlobalDict = 1;

/*****************************************************************************/
/* Function prototypes                                                       */
/*****************************************************************************/
PyObject *RunPython(Text *text, PyObject *globaldict);
char     *GetName(Text *text);
PyObject *CreateGlobalDictionary (void);
void      ReleaseGlobalDictionary (PyObject * dict);
void      DoAllScriptsFromList (ListBase * list, short event);
PyObject *importText(char *name);
void init_ourImport(void);
PyObject *blender_import(PyObject *self, PyObject *args);

/*****************************************************************************/
/* Description: This function will initialise Python and all the implemented */
/*              api variations.                                              */
/* Notes:       Currently only the api for 2.2x will be initialised.         */
/*****************************************************************************/
void BPY_start_python(void)
{
  bpy_registryDict = PyDict_New(); /* check comment at start of this file */

  if (!bpy_registryDict)
    printf("Error: Couldn't create the Registry Python Dictionary!");

/* TODO: Shouldn't "blender" be replaced by PACKAGE ?? (config.h) */
  Py_SetProgramName("blender");

  Py_Initialize ();

  init_ourImport ();

  initBlenderApi2_2x ();

  init_syspath();

  return;
}

/*****************************************************************************/
/* Description: This function will terminate the Python interpreter          */
/*****************************************************************************/
void BPY_end_python(void)
{
  if (bpy_registryDict) {
    Py_DECREF (bpy_registryDict);
    bpy_registryDict = NULL;
  }

  Py_Finalize();

	BPyMenu_RemoveAllEntries(); /* freeing bpymenu mem */

  return;
}

void syspath_append(PyObject *dir)
{
  PyObject *mod_sys, *dict, *path;

  PyErr_Clear();

  mod_sys = PyImport_ImportModule("sys"); /* new ref */
  dict = PyModule_GetDict(mod_sys);       /* borrowed ref */
  path = PyDict_GetItemString(dict, "path"); /* borrowed ref */

  if (!PyList_Check(path)) return;

  PyList_Append(path, dir);

  if (PyErr_Occurred()) Py_FatalError("could not build sys.path");

  Py_DECREF(mod_sys);
}

void init_syspath(void)
{
  PyObject *path;
  PyObject *mod, *d;
  PyObject *p;
  char *c, *progname;
  char execdir[FILE_MAXDIR + FILE_MAXFILE];/*defines from DNA_space_types.h*/

  int n;

  path = Py_BuildValue("s", bprogname);

  mod = PyImport_ImportModule("Blender.sys");

  if (mod) {
    d = PyModule_GetDict(mod);
    PyDict_SetItemString(d, "progname", path);
    Py_DECREF(mod);
  }
  else
    printf("Warning: could not set Blender.sys.progname\n");

  progname = BLI_last_slash(bprogname); /* looks for the last dir separator */

  c = Py_GetPath(); /* get python system path */
  PySys_SetPath(c); /* initialize */

  n = progname - bprogname;
  if (n > 0) {
    strncpy(execdir, bprogname, n);
    if (execdir[n-1] == '.') n--; /*fix for when run as ./blender */
    execdir[n] = '\0';

    p = Py_BuildValue("s", execdir);
    syspath_append(p);  /* append to module search path */

    /* set Blender.sys.progname */
  }
  else
    printf ("Warning: could not determine argv[0] path\n");

  if (U.pythondir && strcmp(U.pythondir, "")) {
    p = Py_BuildValue("s", U.pythondir);
    syspath_append(p);  /* append to module search path */
  }

  /* 
   * bring in the site module so we can add 
   * site-package dirs to sys.path 
   */

  mod = PyImport_ImportModule("site"); /* new ref */

  if (mod) {
    PyObject* item;
    int size = 0;
    int index;

    /* get the value of 'sitedirs' from the module */

    /* the ref man says GetDict() never fails!!! */
    d = PyModule_GetDict (mod); /* borrowed ref */
    p = PyDict_GetItemString (d, "sitedirs");  /* borrowed ref */

    if( p ) {  /* we got our string */
      /* append each item in sitedirs list to path */
      size = PyList_Size (p);

      for (index = 0; index < size; index++) {
	item  = PySequence_GetItem (p, index);  /* new ref */
	if( item )
	  syspath_append (item);
      }
    }
    Py_DECREF(mod);
  }
  else {  /* import 'site' failed */
    PyErr_Clear();
    printf("sys_init:warning - no sitedirs added from site module.\n");
  }

  /* 
   * initialize the sys module
   * set sys.executable to the Blender exe 
   * set argv[0] to the Blender exe
   */

  mod = PyImport_ImportModule("sys"); /* new ref */

  if (mod) {
    d = PyModule_GetDict(mod); /* borrowed ref */
    PyDict_SetItemString(d, "executable", Py_BuildValue("s", bprogname));
    /* in the future this can be extended to have more argv's if needed: */
    PyDict_SetItemString(d, "argv", Py_BuildValue("[s]", bprogname));
    Py_DECREF(mod);
  }
}

/*****************************************************************************/
/* Description: This function finishes Python initialization in Blender.     */
/*              Because U.pythondir (user defined dir for scripts) isn't     */
/*              initialized when BPY_start_Python needs to be executed, we   */
/*              postpone adding U.pythondir to sys.path and also BPyMenus    */
/*              (mechanism to register scripts in Blender menus) for when    */
/*              that dir info is available.                                  */
/*****************************************************************************/
void BPY_post_start_python(void)
{
  syspath_append(Py_BuildValue("s", U.pythondir));
	BPyMenu_Init(); /* get dynamic menus (registered scripts) data */
}

/*****************************************************************************/
/* Description: This function will return the linenumber on which an error   */
/*              has occurred in the Python script.                           */
/*****************************************************************************/
int BPY_Err_getLinenumber(void)
{
  return g_script_error.lineno;
}

/*****************************************************************************/
/* Description: This function will return the filename of the python script. */
/*****************************************************************************/
const char *BPY_Err_getFilename(void)
{
  return g_script_error.filename;
}

/*****************************************************************************/
/* Description: Return PyString filename from a traceback object             */
/*****************************************************************************/
PyObject *traceback_getFilename(PyObject *tb)
{
  PyObject *v;

/* co_filename is in f_code, which is in tb_frame, which is in tb */

  v = PyObject_GetAttrString(tb, "tb_frame"); Py_XDECREF(v);
  v = PyObject_GetAttrString(v, "f_code"); Py_XDECREF(v);
  v = PyObject_GetAttrString(v, "co_filename");

  return v;
}

/*****************************************************************************/
/* Description: Blender Python error handler. This catches the error and     */
/* stores filename and line number in a global                               */
/*****************************************************************************/
void BPY_Err_Handle(char *script_name)
{
  PyObject *exception, *err, *tb, *v;

	if (!script_name) {
		printf("Error: script has NULL name\n");
		return;
	}

  PyErr_Fetch(&exception, &err, &tb);

  if (!exception && !tb) {
    printf("FATAL: spurious exception\n");
    return;
  }

  strcpy(g_script_error.filename, script_name);

  if (exception && PyErr_GivenExceptionMatches(exception, PyExc_SyntaxError)) {
    /* no traceback available when SyntaxError */
    PyErr_Restore(exception, err, tb); /* takes away reference! */
    PyErr_Print();
    v = PyObject_GetAttrString(err, "lineno");
    g_script_error.lineno = PyInt_AsLong(v);
    Py_XDECREF(v);
    /* this avoids an abort in Python 2.3's garbage collecting: */
    PyErr_Clear(); 
    return;
  } else {
    PyErr_NormalizeException(&exception, &err, &tb);
    PyErr_Restore(exception, err, tb); // takes away reference!
    PyErr_Print();
    tb = PySys_GetObject("last_traceback");

    if (!tb) {
      printf("\nCan't get traceback\n");
      return;
    }

    Py_INCREF(tb);

/* From old bpython BPY_main.c:
 * 'check traceback objects and look for last traceback in the
 *  same text file. This is used to jump to the line of where the
 *  error occured. "If the error occured in another text file or module,
 *  the last frame in the current file is adressed."' */

    while (1) { 
      v = PyObject_GetAttrString(tb, "tb_next");

      if (v == Py_None || strcmp(PyString_AsString(traceback_getFilename(v)),
                              script_name)) {
        break;
      }

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

  return;
}

/*****************************************************************************/
/* Description: This function executes the script passed by st.              */
/* Notes:       It is called by blender/src/drawtext.c when a Blender user   */
/*              presses ALT+PKEY in the script's text window.                */
/*****************************************************************************/
int BPY_txt_do_python(struct SpaceText* st)
{
  PyObject *py_dict, *py_result;
	BPy_constant *info;
	Script *script = G.main->script.first;

  if (!st->text) return 0;

	/* check if this text is already running */
	while (script) {
		if (!strcmp(script->id.name+2, st->text->id.name+2)) {
			/* if this text is already a running script, just move to it: */	
			SpaceScript *sc;
			newspace(curarea, SPACE_SCRIPT);
			sc = curarea->spacedata.first;
			sc->script = script;
			return 1;
		}
		script = script->id.next;
	}

	/* Create a new script structure and initialize it: */
	script = alloc_libblock(&G.main->script, ID_SCRIPT, GetName(st->text));

	if (!script) {
		printf("couldn't allocate memory for Script struct!");
		return 0;
	}

	script->id.us = 1;
	script->filename = NULL; /* it's a Blender Text script */
	script->flags = SCRIPT_RUNNING;
	script->py_draw = NULL;
	script->py_event = NULL;
	script->py_button = NULL;

	py_dict = CreateGlobalDictionary();

	script->py_globaldict = py_dict;

	info = (BPy_constant *)M_constant_New();
	if (info) {
		constant_insert(info, "name", PyString_FromString(script->id.name+2));
		Py_INCREF (Py_None);
		constant_insert(info, "arg", Py_None);
		PyDict_SetItemString(py_dict, "__script__", (PyObject *)info);
	}

  clearScriptLinks ();

  py_result = RunPython (st->text, py_dict); /* Run the script */

	if (!py_result) { /* Failed execution of the script */

    BPY_Err_Handle(GetName(st->text));
		ReleaseGlobalDictionary(py_dict);
		free_libblock(&G.main->script, script);
    //BPY_end_python();
    //BPY_start_python();

    return 0;
	}
	else {
		Py_DECREF (py_result);
		script->flags &=~SCRIPT_RUNNING;
		if (!script->flags) {
			ReleaseGlobalDictionary(py_dict);
			script->py_globaldict = NULL;
			free_libblock(&G.main->script, script);
		}
	}

  return 1; /* normal return */
}

/*****************************************************************************/
/* Description: This function executes the script chosen from a menu.        */
/* Notes:       It is called by the ui code in src/header_???.c when a user  */
/*              clicks on a menu entry that refers to a script.              */
/*              Scripts are searched in the BPyMenuTable, using the given    */
/*              menutype and event values to know which one was chosen.      */
/*****************************************************************************/
int BPY_menu_do_python(short menutype, int event)
{
  PyObject *py_dict, *py_result, *pyarg = NULL;
	BPy_constant *info;
	BPyMenu *pym;
	BPySubMenu *pysm;
	FILE *fp = NULL;
	char filestr[FILE_MAXDIR+FILE_MAXFILE];
	Script *script = G.main->script.first;

	if ((menutype < 0) || (menutype > PYMENU_TOTAL) || (event < 0))
		return 0;

	pym = BPyMenuTable[menutype];

	while (event--) {
		if (pym) pym = pym->next;
		else break;
	}

	if (!pym) return 0;

/* if there are submenus, let the user choose one from a pupmenu that we
 * create here.*/
	pysm = pym->submenus;
	if (pysm) {
		char *pupstr; 
		int arg;

		pupstr = BPyMenu_CreatePupmenuStr(pym, menutype);

		if (pupstr) {
			arg = pupmenu(pupstr);
			MEM_freeN(pupstr);

			if (arg >= 0) {
				while (arg--) pysm = pysm->next;
				pyarg = PyString_FromString(pysm->arg);
			}
			else return 0;
		}
	}

	if (!pyarg) {/* no submenus */
		Py_INCREF (Py_None);
		pyarg = Py_None;
	}

	BLI_make_file_string(NULL, filestr, U.pythondir, pym->filename);
	fp = fopen(filestr, "r");
	if (!fp) { /* later also support userhome/.blender/scripts/ or whatever */
		printf("Error loading script: couldn't open file %s\n", filestr);
		return 0;
	}

	/* Create a new script structure and initialize it: */
	script = alloc_libblock(&G.main->script, ID_SCRIPT, pym->name);

	if (!script) {
		printf("couldn't allocate memory for Script struct!");
		fclose(fp);
		return 0;
	}

	script->id.us = 1;
	script->filename = NULL; /* it's a Blender Text script */
	script->flags = SCRIPT_RUNNING;
	script->py_draw = NULL;
	script->py_event = NULL;
	script->py_button = NULL;

	py_dict = CreateGlobalDictionary();

	script->py_globaldict = py_dict;

	info = (BPy_constant *)M_constant_New();
	if (info) {
		constant_insert(info, "name", PyString_FromString(script->id.name+2));
		constant_insert(info, "arg", pyarg);
		PyDict_SetItemString(py_dict, "__script__", (PyObject *)info);
	}

  clearScriptLinks ();

	py_result = PyRun_File(fp, pym->filename, Py_file_input, py_dict, py_dict);

	fclose(fp);

	if (!py_result) { /* Failed execution of the script */

    BPY_Err_Handle(script->id.name+2);
		PyErr_Print();
		ReleaseGlobalDictionary(py_dict);
		free_libblock(&G.main->script, script);
  //  BPY_end_python();
  //  BPY_start_python();

    return 0;
	}
	else {
		Py_DECREF (py_result);
		script->flags &=~SCRIPT_RUNNING;
		if (!script->flags) {
			ReleaseGlobalDictionary(py_dict);
			script->py_globaldict = NULL;
			free_libblock(&G.main->script, script);
		}
	}

  return 1; /* normal return */
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:                                                                    */
/*****************************************************************************/
void BPY_free_compiled_text(struct Text* text)
{
  if (!text->compiled) return;
  Py_DECREF((PyObject*) text->compiled);
  text->compiled = NULL;

  return;
}

/*****************************************************************************/
/* Description: This function frees a finished (flags == 0) script.          */
/*****************************************************************************/
void BPY_free_finished_script(Script *script)
{
	if (!script) return;

	if (script->lastspace != SPACE_SCRIPT)
		newspace (curarea, script->lastspace);

	free_libblock(&G.main->script, script);
	return;
}

void unlink_script(Script *script)
{ /* copied from unlink_text in drawtext.c */
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	for (scr= G.main->screen.first; scr; scr= scr->id.next) {
		for (area= scr->areabase.first; area; area= area->next) {
			for (sl= area->spacedata.first; sl; sl= sl->next) {
				if (sl->spacetype==SPACE_SCRIPT) {
					SpaceScript *sc= (SpaceScript*) sl;

					if (sc->script==script) {
						sc->script= NULL;

						if (sc==area->spacedata.first) {
							scrarea_queue_redraw(area);
						}
					}
				}
			}
		}
	}
}

void BPY_clear_script (Script *script)
{
	PyObject *dict;

	if (!script) return;

	Py_XDECREF((PyObject *)script->py_draw);
	Py_XDECREF((PyObject *)script->py_event);
	Py_XDECREF((PyObject *)script->py_button);

	dict = script->py_globaldict;

	if (dict) {
  	PyDict_Clear (dict);
  	Py_DECREF (dict);   /* Release dictionary. */
		script->py_globaldict = NULL;
	}

	unlink_script (script);
}

/*****************************************************************************/
/* ScriptLinks                                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_clear_bad_scriptlinks(struct Text *byebye)
{
/*
  BPY_clear_bad_scriptlist(getObjectList(), byebye);
  BPY_clear_bad_scriptlist(getLampList(), byebye);
  BPY_clear_bad_scriptlist(getCameraList(), byebye);
  BPY_clear_bad_scriptlist(getMaterialList(), byebye);
  BPY_clear_bad_scriptlist(getWorldList(),  byebye);
  BPY_clear_bad_scriptlink(&scene_getCurrent()->id, byebye);

  allqueue(REDRAWBUTSSCRIPT, 0);
*/
  return;
}

/*****************************************************************************/
/* Description: Loop through all scripts of a list of object types, and      */
/*              execute these scripts.                                       */
/*              For the scene, only the current active scene the scripts are */
/*              executed (if any).                                           */
/*****************************************************************************/
void BPY_do_all_scripts(short event)
{
  DoAllScriptsFromList (&(G.main->object), event);
  DoAllScriptsFromList (&(G.main->lamp), event);
  DoAllScriptsFromList (&(G.main->camera), event);
  DoAllScriptsFromList (&(G.main->mat), event);
  DoAllScriptsFromList (&(G.main->world), event);

  BPY_do_pyscript (&(G.scene->id), event);

  return;
}

/*****************************************************************************/
/* Description: Execute a Python script when an event occurs. The following  */
/*              events are possible: frame changed, load script and redraw.  */
/*              Only events happening to one of the following object types   */
/*              are handled: Object, Lamp, Camera, Material, World and       */
/*              Scene.                                                       */
/*****************************************************************************/
void BPY_do_pyscript(struct ID *id, short event)
{
  ScriptLink  * scriptlink;
  int           index;
  PyObject    * dict;
  PyObject    * ret;

  scriptlink = setScriptLinks (id, event);

  if (scriptlink == NULL) return;

  for (index = 0; index < scriptlink->totscript; index++)
  {
    if ((scriptlink->flag[index] == event) &&
        (scriptlink->scripts[index] != NULL))
    {
      dict = CreateGlobalDictionary();
      ret = RunPython ((Text*) scriptlink->scripts[index], dict);
      ReleaseGlobalDictionary (dict);
      if (!ret)
      {
          /* Failed execution of the script */
          BPY_Err_Handle (scriptlink->scripts[index]->name+2);
          BPY_end_python ();
          BPY_start_python ();
      }
      else
      {
          Py_DECREF (ret);
      }
    }
  }

  return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:                                                                    */
/*****************************************************************************/
void BPY_free_scriptlink(struct ScriptLink *slink)
{
  if (slink->totscript) {
    if(slink->flag) MEM_freeN(slink->flag);
    if(slink->scripts) MEM_freeN(slink->scripts); 
  }

  return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:                                                                    */
/*****************************************************************************/
void BPY_copy_scriptlink(struct ScriptLink *scriptlink)
{
  void *tmp;

  if (scriptlink->totscript) {

    tmp = scriptlink->scripts;
    scriptlink->scripts =
      MEM_mallocN(sizeof(ID*)*scriptlink->totscript, "scriptlistL");
    memcpy(scriptlink->scripts, tmp, sizeof(ID*)*scriptlink->totscript);

    tmp = scriptlink->flag;
    scriptlink->flag =
      MEM_mallocN(sizeof(short)*scriptlink->totscript, "scriptlistF");
    memcpy(scriptlink->flag, tmp, sizeof(short)*scriptlink->totscript);
  }

  return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
int BPY_call_importloader(char *name)
{ /* XXX Should this function go away from Blender? */
  printf ("In BPY_call_importloader(name=%s)\n",name);
  return (0);
}

/*****************************************************************************/
/* Private functions                                                         */
/*****************************************************************************/

/*****************************************************************************/
/* Description: This function executes the python script passed by text.     */
/*              The Python dictionary containing global variables needs to   */
/*              be passed in globaldict.                                     */
/*****************************************************************************/
PyObject * RunPython(Text *text, PyObject *globaldict)
{
  char *buf = NULL;

/* The script text is compiled to Python bytecode and saved at text->compiled
 * to speed-up execution if the user executes the script multiple times */

  if (!text->compiled) { /* if it wasn't already compiled, do it now */
    buf = txt_to_buf(text);

    text->compiled = Py_CompileString(buf, GetName(text), Py_file_input);

    MEM_freeN(buf);

    if (PyErr_Occurred()) {
      BPY_free_compiled_text(text);
      return NULL;
    }

  }

  return PyEval_EvalCode(text->compiled, globaldict, globaldict);
}

/*****************************************************************************/
/* Description: This function returns the value of the name field of the     */
/*              given Text struct.                                           */
/*****************************************************************************/
char * GetName(Text *text)
{
  return (text->id.name+2);
}

/*****************************************************************************/
/* Description: This function creates a new Python dictionary object.        */
/*****************************************************************************/
PyObject * CreateGlobalDictionary (void)
{
  PyObject *dict = PyDict_New();

  PyDict_SetItemString (dict, "__builtins__", PyEval_GetBuiltins());
  PyDict_SetItemString (dict, "__name__", PyString_FromString("__main__"));

  return dict;
}

/*****************************************************************************/
/* Description: This function deletes a given Python dictionary object.      */
/*****************************************************************************/
void ReleaseGlobalDictionary (PyObject * dict)
{
  PyDict_Clear (dict);
  Py_DECREF (dict);   /* Release dictionary. */

  return;
}

/*****************************************************************************/
/* Description: This function runs all scripts (if any) present in the       */
/*              list argument. The event by which the function has been      */
/*              called, is passed in the event argument.                     */
/*****************************************************************************/
void DoAllScriptsFromList (ListBase *list, short event)
{
  ID *id;

  id = list->first;

  while (id != NULL) {
    BPY_do_pyscript (id, event);
    id = id->next;
  }

  return;
}

PyObject *importText(char *name)
{
  Text *text;
  char *txtname;
  char *buf = NULL;
  int namelen = strlen(name);

  txtname = malloc(namelen+3+1);
  if (!txtname) return NULL;

  memcpy(txtname, name, namelen);
  memcpy(&txtname[namelen], ".py", 4);

  text = (Text*) &(G.main->text.first);

  while(text) {
    if (!strcmp (txtname, GetName(text)))
      break;
    text = text->id.next;
  }

  if (!text) {
    free(txtname);
    return NULL;
  }

  if (!text->compiled) {
    buf = txt_to_buf(text);
    text->compiled = Py_CompileString(buf, GetName(text), Py_file_input);
    MEM_freeN(buf);

    if (PyErr_Occurred()) {
      PyErr_Print();
      BPY_free_compiled_text(text);
      free(txtname);
      return NULL;
    }
  }

  free(txtname);
  return PyImport_ExecCodeModule(name, text->compiled);
}

static PyMethodDef bimport[] = {
  { "blimport", blender_import, METH_VARARGS, "our own import"}
};

PyObject *blender_import(PyObject *self, PyObject *args)
{
  PyObject *exception, *err, *tb;
  char *name;
  PyObject *globals = NULL, *locals = NULL, *fromlist = NULL;
  PyObject *m;

  if (!PyArg_ParseTuple(args, "s|OOO:bimport",
          &name, &globals, &locals, &fromlist))
      return NULL;

  m = PyImport_ImportModuleEx(name, globals, locals, fromlist);

  if (m) 
    return m;
  else
    PyErr_Fetch(&exception, &err, &tb); /*restore for probable later use*/
  
  m = importText(name);
  if (m) { /* found module, ignore above exception*/
    PyErr_Clear();
    Py_XDECREF(exception); Py_XDECREF(err); Py_XDECREF(tb);
    printf("imported from text buffer...\n");
  } else {
    PyErr_Restore(exception, err, tb);
  }
  return m;
}

void init_ourImport(void)
{
  PyObject *m, *d;
  PyObject *import = PyCFunction_New(bimport, NULL);

  m = PyImport_AddModule("__builtin__");
  d = PyModule_GetDict(m);
  PyDict_SetItemString(d, "__import__", import);
}
