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
 * Contributor(s): Michel Selten, Willian P. Germano
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

#include <BKE_global.h>
#include <BKE_main.h>
#include <BKE_text.h>
#include <DNA_camera_types.h>
#include <DNA_ID.h>
#include <DNA_lamp_types.h>
#include <DNA_material_types.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_scriptlink_types.h>
#include <DNA_space_types.h>
#include <DNA_text_types.h>
#include <DNA_world_types.h>

#include "BPY_extern.h"
#include "api2_2x/EXPP_interface.h"


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

/*****************************************************************************/
/* Description: This function will initialise Python and all the implemented */
/*              api variations.                                              */
/* Notes:       Currently only the api for 2.2x will be initialised.         */
/*****************************************************************************/
void BPY_start_python(void)
{
  printf ("In BPY_start_python\n");
/* TODO: Shouldn't "blender" be replaced by PACKAGE ?? (config.h) */
  Py_SetProgramName("blender");

  Py_Initialize ();

  initBlenderApi2_2x ();

  return; /* We could take away all these return; ... */
}

/*****************************************************************************/
/* Description: This function will terminate the Python interpreter          */
/*****************************************************************************/
void BPY_end_python(void)
{
  printf ("In BPY_end_python\n");
  Py_Finalize();
  return;
}

/*****************************************************************************/
/* Description: This function will return the linenumber on which an error   */
/*              has occurred in the Python script.                           */
/*****************************************************************************/
int BPY_Err_getLinenumber(void)
{
  printf ("In BPY_Err_getLinenumber\n");
  return g_script_error.lineno;
}

/*****************************************************************************/
/* Description: This function will return the filename of the python script. */
/*****************************************************************************/
const char *BPY_Err_getFilename(void)
{
  printf ("In BPY_Err_getFilename\n");
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
void BPY_Err_Handle(Text *text)
{
  PyObject *exception, *err, *tb, *v;

  PyErr_Fetch(&exception, &err, &tb);

  if (!exception && !tb) {
    printf("FATAL: spurious exception\n");
    return;
  }

  strcpy(g_script_error.filename, GetName(text));

  if (exception && PyErr_GivenExceptionMatches(exception, PyExc_SyntaxError)) {
    /* no traceback available when SyntaxError */
    PyErr_Restore(exception, err, tb); /* takes away reference! */
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
                              GetName(text))) {
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
struct _object *BPY_txt_do_python(struct SpaceText* st)
{
  PyObject *dict, *ret;

  printf ("\nIn BPY_txt_do_python\n");

  if (!st->text) return NULL;

/* The EXPP_releaseGlobalDict global variable controls whether we should run
 * the script with a clean global dictionary or should keep the current one,
 * possibly already "polluted" by other calls to the Python Interpreter.
 * The default is to use a clean one.  To change this the script writer must
 * call Blender.releaseGlobalDict(bool), with bool == 0, in the script */

  if (EXPP_releaseGlobalDict) {
    printf("Using a clean Global Dictionary.\n");
    st->flags |= ST_CLEAR_NAMESPACE;
    dict = CreateGlobalDictionary();
  }
  else
    dict = PyModule_GetDict(PyImport_AddModule("__main__"));

  clearScriptLinks ();

  ret = RunPython (st->text, dict); /* Run the script */

  if (!ret) { /* Failed execution of the script */

    if (EXPP_releaseGlobalDict) ReleaseGlobalDictionary(dict);

    BPY_Err_Handle(st->text);
    BPY_end_python();
    BPY_start_python();

    return NULL;
  }

  else Py_DECREF (ret);

/* From the old BPY_main.c:
 * 'The following lines clear the global name space of the python
 *  interpreter. This is desired to release objects after execution
 *  of a script (remember that each wrapper object increments the refcount
 *  of the Blender Object. 
 *  Exception: scripts that use the GUI rely on the
 *  persistent global namespace, so they need a workaround: The namespace
 *  is released when the GUI is exit.'
 * See api2_2x/Draw.c: Method_Register() */

  if (EXPP_releaseGlobalDict) {     
    if (st->flags & ST_CLEAR_NAMESPACE) {
      ReleaseGlobalDictionary(dict);
      /*garbage_collect(&G.main); Unfinished in the previous implementation */ 
    }
  }

/* Edited from old BPY_main.c:
 * 'The return value is the global namespace dictionary of the script
 *  context.  This may be stored in the SpaceText instance to give control
 *  over namespace persistence.  Remember that the same script may be
 *  executed in several windows ...  Namespace persistence is desired for
 *  scripts that use the GUI and store callbacks to the current script.' */

  return dict;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:                                                                    */
/*****************************************************************************/
void BPY_free_compiled_text(struct Text* text)
{
  printf ("In BPY_free_compiled_text\n");
  if (!text->compiled) return;
  Py_DECREF((PyObject*) text->compiled);
  text->compiled = NULL;

  return;
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
  printf ("In BPY_clear_bad_scriptlinks\n");
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
  printf ("In BPY_do_all_scripts(event=%d)\n",event);

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

  printf ("In BPY_do_pyscript(id=%s, event=%d)\n",id->name, event);

  scriptlink = setScriptLinks (id, event);

  if (scriptlink == NULL) return;

  for (index = 0; index < scriptlink->totscript; index++)
  {
    printf ("scriptnr: %d\tevent=%d, flag[index]=%d\n", index,
        event, scriptlink->flag[index]);
    if ((scriptlink->flag[index] == event) &&
        (scriptlink->scripts[index] != NULL))
    {
      dict = CreateGlobalDictionary();
      ret = RunPython ((Text*) scriptlink->scripts[index], dict);
      ReleaseGlobalDictionary (dict);
      if (!ret)
      {
          /* Failed execution of the script */
          BPY_Err_Handle ((Text*) scriptlink->scripts[index]);
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
  printf ("In BPY_free_scriptlink\n");

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

  printf ("In BPY_copy_scriptlink\n");

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

  printf("Run Python script \"%s\" ...\n", GetName(text));

/* The script text is compiled to Python bytecode and saved at text->compiled
 * to speed-up execution if the user executes the script multiple times */

  if (!text->compiled) { /* if it wasn't already compiled, do it now */

/*#ifdef BLENDER_SANDBOX_MODE

// IGNORE THIS ALL FOR A WHILE, IT'S VERY INCOMPLETE AND WILL CHANGE
// CONSIDERABLY, SOON.  The #ifdef won't stay, either.

// The import statement is a security risk, so we don't allow it in
// SANDBOX MODE.  Instead, we import all needed modules ourselves and
// substitute all 'import' and '__import__' statements in the code by
// '#mport' and '#_import__', resp., making their lines become comments
// in Python (to let scripts run without import errors).

// Disable importing only for the safest sandbox mode 

    txt_move_bof(text, 0); // move to the beginning of the script

// Search all occurrences of 'import' in the script
// XXX Also check for from ... import ...
    while (txt_find_string (text, "import")) {
      char *line = text->sell->line;

      if (text->curc > 1) // is it '__import__' ?
        if (strncmp (&line[text->curc - 2],
                     "__import__", 10) == 0) text->curc -= 2;

      line[text->curc] = '#'; // change them to '#mport' or '#_import__'
    }

#endif */

    buf = txt_to_buf(text);

    text->compiled = Py_CompileString(buf, GetName(text), Py_file_input);

    MEM_freeN(buf);

    if (PyErr_Occurred()) {
      BPY_free_compiled_text(text);
      return NULL;
    }

  }

/*#ifdef BLENDER_SANDBOX_MODE
  //save the script as a dict entry and call the eval code for it
  //then return
  PyDict_SetItemString(globaldict, "_SB_code", text->compiled);
#endif */

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
