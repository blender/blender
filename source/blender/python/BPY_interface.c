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

/*****************************************************************************/
/* Function prototypes                                                       */
/*****************************************************************************/
PyObject * RunPython(Text *text, PyObject *globaldict);
char *     GetName(Text *text);
PyObject * CreateGlobalDictionary (void);
void       ReleaseGlobalDictionary (PyObject * dict);
void       DoAllScriptsFromList (ListBase * list, short event);

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

	return;
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
/* Description: This function executes the script passed by st.              */
/* Notes:       Currently, the script is compiled each time it is executed,  */
/*              This should be optimized to store the compiled bytecode as   */
/*              has been done by the previous implementation.                */
/*****************************************************************************/
struct _object *BPY_txt_do_python(struct SpaceText* st)
{
	PyObject *	dict;
	PyObject *	ret;
	printf ("In BPY_txt_do_python\n");

	if (!st->text)
	{
		return NULL;
	}

	dict = PyModule_GetDict(PyImport_AddModule("__main__"));
	/* dict = newGlobalDictionary(); */
	ret = RunPython (st->text, dict);

	/* If errors have occurred, set the error filename to the name of the
	   script.
	*/
	if (!ret)
	{
		sprintf(g_script_error.filename, "%s", st->text->id.name+2);
		return NULL;
	}

	return dict;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_free_compiled_text(struct Text* text)
{
	printf ("In BPY_free_compiled_text\n");
	return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_clear_bad_scriptlinks(struct Text *byebye)
{
	printf ("In BPY_clear_bad_scriptlinks\n");
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
	ScriptLink     * scriptlink;
	int              index;
	PyObject       * dict;

	printf ("In BPY_do_pyscript(id=%s, event=%d)\n",id->name, event);

	scriptlink = setScriptLinks (id, event);

	if (scriptlink == NULL)
	{
		return;
	}

	for (index=0 ; index<scriptlink->totscript ; index++)
	{
		printf ("scriptnr: %d\tevent=%d, flag[index]=%d\n", index,
				event, scriptlink->flag[index]);
		if ((scriptlink->flag[index] == event) &&
		    (scriptlink->scripts[index]!=NULL))
		{
			dict = CreateGlobalDictionary();
			RunPython ((Text*) scriptlink->scripts[index], dict);
			ReleaseGlobalDictionary (dict);
		}
	}

	return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_free_scriptlink(struct ScriptLink *slink)
{
	printf ("In BPY_free_scriptlink\n");
	return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_copy_scriptlink(struct ScriptLink *scriptlink)
{
	printf ("In BPY_copy_scriptlink\n");
	return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
int BPY_call_importloader(char *name)
{
	printf ("In BPY_call_importloader(name=%s)\n",name);
	return (0);
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
int BPY_spacetext_is_pywin(struct SpaceText *st)
{
	/* No printf is done here because it is called with every mouse move */
	return (0);
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_spacetext_do_pywin_draw(struct SpaceText *st)
{
	printf ("In BPY_spacetext_do_pywin_draw\n");
	return;
}

/*****************************************************************************/
/* Description:                                                              */
/* Notes:       Not implemented yet                                          */
/*****************************************************************************/
void BPY_spacetext_do_pywin_event(struct SpaceText *st,
                                  unsigned short event,
                                  short val)
{
	printf ("In BPY_spacetext_do_pywin_event(st=?, event=%d, val=%d)\n",
	        event, val);
	return;
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
	PyObject * ret;
	char     * buf;

	printf("Run Python script \"%s\" ...\n", GetName(text));
	buf = txt_to_buf(text);
	ret = PyRun_String (buf, Py_file_input, globaldict, globaldict);

	if (!ret)
	{
		/* an exception was raised, handle it here */
		PyErr_Print(); /* this function also clears the error
				  indicator */
	}
	else
	{
		PyErr_Clear(); /* seems necessary, at least now */
	}

	MEM_freeN (buf);
	return ret;
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

	return (dict);
}

/*****************************************************************************/
/* Description: This function deletes a given Python dictionary object.      */
/*****************************************************************************/
void ReleaseGlobalDictionary (PyObject * dict)
{
	PyDict_Clear (dict);
	Py_DECREF (dict);		/* Release dictionary. */
}

/*****************************************************************************/
/* Description: This function runs all scripts (if any) present in the       */
/*              list argument. The event by which the function has been      */
/*              called, is passed in the event argument.                     */
/*****************************************************************************/
void DoAllScriptsFromList (ListBase * list, short event)
{
	ID       * id;

	id = list->first;

	while (id != NULL)
	{
		BPY_do_pyscript (id, event);
		id = id->next;
	}
}

