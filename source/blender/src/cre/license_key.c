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
#include "BLO_readfile.h"
#include "BLO_keyStore.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int LICENSE_KEY_VALID = TRUE;
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
#ifndef NDEBUG
	printf("feature not enabled: Buy our Key NOW!\n");
#endif
	return 0;
}

/*
int key_func1(void *vp) {
	printf("function 1 called\n");
}

*/
int key_return_true(void *vp) {
#ifndef NDEBUG
	printf("function 2 called (return true)\n");
#endif
	return 1;
}

/* ----------------------------------------------------- */

/* Declarations for objects of type Fplist */


#ifndef NDEBUG
void feature1(void)
{
	Fptr f;

	printf("feature 2 called\n");
	f = g_ptrtab[KEY_FUNC2];
	if (f) f(0);
}

void feature2(void)
{
	Fptr f;

	printf("feature 3 called\n");
	f = g_ptrtab[KEY_FUNC3];
	if (f) f(0);
}

#endif


/* Initialization function for the module (*must* be called initprot) */

static void init_ftable(void)  // initializes functiontable
{
	int i;

	g_functab[0] = &key_func1;
	g_functab[1] = &key_func2;
	g_functab[2] = &key_func3;

	for (i = 3; i < PYKEY_TABLEN; i++)
	{
		g_functab[i] = &sticky_shoes;
	}

	// for debugging perposes
	/*
	for (i = 0; i < PYKEY_TABLEN; i++)
	{
		g_functab[i] = (Fptr *) (i + 100);
	}
	*/
}


static void init_ptable(void)  // initializes functiontable
{
	int i;

	for (i = 0; i < PYKEY_TABLEN; i++)
	{
		g_ptrtab[i] = &sticky_shoes;
	}
}


#ifdef NDEBUG
static void print_ptable(void)
{	
	int i;

	for (i = 0; i < PYKEY_TABLEN; i++)
	{
		printf ("index[%02d] = %08x\n", i, g_ptrtab[i]);
	}
}
#endif

static void insertname(PyObject *m,PyObject *p, char *name)
{
}

/* initialisation */
static void initprot()
{
	init_ftable(); // malloc
	init_ptable(); // malloc
}

// ******************************* KEY STUFF *********************

void create_key_name(char * keyname)
{
}

void checkhome()
{
	initprot();                   // initialize module and function tables
	IMB_fp_png_encode = IMB_png_encode;
}

void SHOW_LICENSE_KEY(void)
{
}

void loadKeyboard(char * name)
{
}
