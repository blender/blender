/**
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

#include "license_key.h"
#include "keyed_functions.h"
#include "BKE_utildefines.h"
#include "BIF_screen.h"  // splash
#include "BIF_toolbox.h"
#include "blenkey.h"
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"

#include "BLO_readfile.h"
#include "BLO_keyStore.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int LICENSE_KEY_VALID = FALSE;
int I_AM_PUBLISHER = TRUE;

static UserStruct User;

// Python stuff

#include "Python.h"
#include "marshal.h" 
#include "compile.h" /* to give us PyCodeObject */
#include "eval.h"		/* prototype for PyEval_EvalCode */

#include "BPY_extern.h"

#include "IMB_imbuf.h"

Fptr g_functab[PYKEY_TABLEN];
Fptr g_ptrtab[PYKEY_TABLEN];

static int g_seed[3] = PYKEY_SEED;
static PyObject *g_module_self;
static PyObject *g_main;


// end Python stuff

// **************** PYTHON STUFF **************************
/* ----------------------------------------------------- */
/* this is the dummy functions to demonstrate */

int sticky_shoes(void *vp)
{
	return 0;
}

/*
int key_func1(void *vp) {
	printf("function 1 called\n");
}

*/
int key_return_true(void *vp) {
	return 1;
}


/* ----------------------------------------------------- */

/* Declarations for objects of type Fplist */


static char prot_getseed__doc__[] = "";

static PyObject *
prot_getseed(self, args)
	PyObject *self;	/* Not used */
	PyObject *args;
{
	PyObject *p;
	p = PyTuple_New(3);
	PyTuple_SetItem(p, 0, PyInt_FromLong(g_seed[0]));
	PyTuple_SetItem(p, 1, PyInt_FromLong(g_seed[1]));
	PyTuple_SetItem(p, 2, PyInt_FromLong(g_seed[2]));
	return p;
}

static char prot_getlen__doc__[] = "";
static PyObject *
prot_getlen(self, args)
	PyObject *self;	/* Not used */
	PyObject *args;
{
	return Py_BuildValue("i", PYKEY_TABLEN);
}

static char prot_getptr__doc__[] =
""
;

static PyObject *
prot_getptr(self, args)
	PyObject *self;	/* Not used */
	PyObject *args;
{
	PyObject *p;
	Fptr f;
	int index;
	/* we don't catch errors here, we're in the key code */
	if (!g_functab)
		return NULL;
	if (!PyArg_ParseTuple(args, "i", &index))
		return NULL;
	if (index >= PYKEY_TABLEN)
		return NULL;

	f = g_functab[index];
	p = PyCObject_FromVoidPtr(f , NULL);
	return p;
}

static char prot_setptr__doc__[] =
""
;
static PyObject *
prot_setptr(self, args)
	PyObject *self;	/* Not used */
	PyObject *args;
{
	PyObject *p;

	int index;

	if (!g_ptrtab) 
		return NULL;
	if (!PyArg_ParseTuple(args, "iO", &index, &p))
		return NULL;
	if (index >= PYKEY_TABLEN)
		return NULL;
	if (!PyCObject_Check(p)) {
		return NULL;
	}

	g_ptrtab[index] = PyCObject_AsVoidPtr(p);
	return Py_BuildValue("i", 1);
}

static PyObject *callkeycode(
	unsigned char *keycode,
	int keycodelen)
{
	PyCodeObject *code;
	PyObject *maindict = PyModule_GetDict(g_main);

	code = (PyCodeObject *) PyMarshal_ReadObjectFromString(keycode, keycodelen);
	if (!PyEval_EvalCode(code, maindict, maindict))
		return NULL;
	return Py_BuildValue("i", 1);
}



/* List of methods defined in the module */

static struct PyMethodDef prot_methods[] = {
	{"getlen",	(PyCFunction)prot_getlen,	METH_VARARGS,	prot_getlen__doc__},
	{"getseed",	(PyCFunction)prot_getseed,	METH_VARARGS,	prot_getseed__doc__},
	{"getptr",	(PyCFunction)prot_getptr,	METH_VARARGS,	prot_getptr__doc__},
	{"setptr",	(PyCFunction)prot_setptr,	METH_VARARGS,	prot_setptr__doc__},
 
	{NULL,	 (PyCFunction)NULL, 0, NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initprot) */

static char prot_module_documentation[] = "No Documentation";

static void init_ftable(void)  // initializes functiontable
{
	int i;

	g_functab[0] = &key_func1;
	g_functab[1] = &key_func2;
	g_functab[2] = &key_func3;
/*  add more key_funcs here */

	for (i = 3; i < PYKEY_TABLEN; i++)
	{
		g_functab[i] = &sticky_shoes;
	}
}


static void init_ptable(void)  // initializes functiontable
{
	int i;

	for (i = 0; i < PYKEY_TABLEN; i++)
	{
		g_ptrtab[i] = &sticky_shoes;
	}
}


static void insertname(PyObject *m,PyObject *p, char *name)
{
	PyObject *d = PyModule_GetDict(m);

	PyDict_SetItemString(d, name, p);
	Py_DECREF(p);
}

/* initialisation */
static void initprot()
{
	PyObject *m, *d;
	PyObject *capi1;
	PyObject *ErrorObject;

	init_ftable(); 

	g_main = PyImport_AddModule("__main__");

	m = Py_InitModule4("prot", prot_methods,
		prot_module_documentation,
		(PyObject*)NULL,PYTHON_API_VERSION);
	g_module_self = m;	
	d = PyModule_GetDict(m);
	ErrorObject = PyString_FromString("prot.error");
	PyDict_SetItemString(d, "error", ErrorObject);

	/* add global object */

	capi1 = PyCObject_FromVoidPtr((void *)g_functab , NULL);
	if (capi1) {
		insertname(m, capi1, "APIfunctab");
	}	
	
	/* Check for errors */
	if (PyErr_Occurred())
		Py_FatalError("can't initialize module prot");

	init_ptable(); 
}

// ******************************* KEY STUFF *********************

static void create_key_name(char * keyname)
{
	sprintf(keyname, "%s/.BPkey", BLI_gethome());
}

void checkhome()
{
	int keyresult;
	char *HexPriv, *HexPub, *HexPython;
	byte *Byte;
	char keyname[FILE_MAXDIR + FILE_MAXFILE];
	int wasInitialized;
	unsigned char *keycode = NULL;
	int keycodelen = 0;

	create_key_name(keyname);
	keyresult = ReadKeyFile(keyname, &User, &HexPriv, &HexPub,
				&Byte, &HexPython);
	if (keyresult != 0) {
	    // printf("\nReadKeyFile error %d\n", keyresult);
	} else {
	    // printf("\nReadKeyFile OK\n");
		LICENSE_KEY_VALID = TRUE;

		wasInitialized = Py_IsInitialized();
		
		// make it failsafe if python interpreter was already initialized
		if (wasInitialized) 
			Py_Initialize();
			
		initprot();                   // initialize module and function tables
		// get python byte code
		keycode = DeHexify(HexPython);
		keycodelen = strlen(HexPython) / 2;

		callkeycode(keycode, keycodelen);

		Py_Finalize(); 

		if (wasInitialized) 	// if we were initialized,
			BPY_start_python(); // restart creator python

		//some debugging stuff
		// print_ptable();

		// Store key stuff for use by stream
		keyStoreConstructor(
			&User,
			HexPriv,
			HexPub,
			Byte,
			HexPython);

		// other initialization code

		// enable png writing in the ImBuf library
		IMB_fp_png_encode = IMB_png_encode;
	}
}

void SHOW_LICENSE_KEY(void)
{
	extern int datatoc_tonize;
	extern char datatoc_ton[];
	char string[1024];
	int maxtype, type;
	char *typestrings[] = {
		"",
		"Individual",
		"Company",
		"Unlimited",
		"Educational"};

	maxtype = (sizeof(typestrings) / sizeof(char *)) - 1;
	type = User.keytype;
	if (type > maxtype) {
		type = 0;
	}

	if (LICENSE_KEY_VALID) {
		sprintf(string, "%s License registered to: %s (%s)", typestrings[type], User.name, User.email);
		splash((void *)datatoc_ton, datatoc_tonize, string);
	}
}

void loadKeyboard(char * name)
{
	char keyname[FILE_MAXDIR + FILE_MAXFILE];
	FILE *in, *out;
	char string[1024], *match = 0;
	int i, c;
	int found = 0;

	// make sure we don't overwrite a valid key...
	
	if (!LICENSE_KEY_VALID) {
		in = fopen(name, "rb");
		if (in) {
			// scan for blender key magic, read strings
			// with anything but a newline
			while (fscanf(in, "%1000[^\n\r]", string) != EOF) {
				match = strstr(string, BLENKEYMAGIC);
				if (match) {
					break;
				}
				fscanf(in, "\n");
			}
			
			if (match) {
				// found blender key magic, open output file
				// to copy key information
				
				create_key_name(keyname);
				out = fopen(keyname, "wb");
				if (out) {
					// printout first line
					fprintf(out, "%s", match);
					for (i = 0; i < 350; i++) {
						// handle control characters (\n\r)
						while (1) {
							c = getc(in);
							if (c == '\n') {
								// output a \n for each \n in the input
								fprintf(out, "\n");
							} else if (c == EOF) {
								break;
							} else if (c < ' ') {
								// skip control characters
							} else {
								ungetc(c, in);
								break;
							}
						}
						
						if (fscanf(in, "%1000[^\n\r]", string) != EOF) {
							if (strcmp(string, BLENKEYSEPERATOR) == 0) {
								found++;
							}
							fprintf(out, "%s", string);
						} else {
							break;
						}

						if (found >= 2) {
							break;
						}
					}
					
					fclose(out);
					
					checkhome();
					if (LICENSE_KEY_VALID) {
						SHOW_LICENSE_KEY();
					} else {
						error("Not a valid license key ! Removing installed key.");
						BLI_delete(keyname, 0, 0);
					}

				} else {
					error("Can't install key");
				}
			} else {
				error("File doesn't contain a valid key: %s", name);
			}

			fclose(in);

			if (LICENSE_KEY_VALID) {
				if (okee("Remove input file: '%s'?", name)) {
					BLI_delete(name, 0, 0);
				}
			}

		} else {
			error("File doesn't exist: %s", name);
		}
	}
}
