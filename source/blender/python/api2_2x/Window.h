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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_WINDOW_H
#define EXPP_WINDOW_H

#include <Python.h>
#include <stdio.h>

#include <BKE_global.h>
#include <BKE_object.h> /* for during_script() */
#include <BIF_usiblender.h>
#include <BIF_mywindow.h>
#include <BSE_headerbuttons.h>
#include <BSE_filesel.h>
#include <BIF_screen.h>
#include <BIF_space.h>
#include <BIF_drawtext.h>
#include <mydevice.h>
#include <DNA_view3d_types.h>
#include <DNA_screen_types.h>
#include <DNA_space_types.h>
#include <DNA_text_types.h>

#include "gen_utils.h"
#include "modules.h"

/* Used in Draw.c */
extern int EXPP_disable_force_draw;

/* Callback used by the file and image selector access functions */
static PyObject *(*EXPP_FS_PyCallback)(PyObject *arg) = NULL;

/*****************************************************************************/
/* Python API function prototypes for the Window module.                     */
/*****************************************************************************/
PyObject *M_Window_Redraw (PyObject *self, PyObject *args);
static PyObject *M_Window_RedrawAll (PyObject *self, PyObject *args);
static PyObject *M_Window_QRedrawAll (PyObject *self, PyObject *args);
static PyObject *M_Window_FileSelector (PyObject *self, PyObject *args);
static PyObject *M_Window_ImageSelector (PyObject *self, PyObject *args);
static PyObject *M_Window_DrawProgressbar (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Window.__doc__                                                    */
/*****************************************************************************/
char M_Window_doc[] =
"The Blender Window module\n\n";

char M_Window_Redraw_doc[] =
"() - Force a redraw of a specific Window Type (see Window.Types)";

char M_Window_RedrawAll_doc[] =
"() - Redraw all windows";

char M_Window_QRedrawAll_doc[] =
"() - Redraw all windows by queue event";

char M_Window_FileSelector_doc[] =
"(callback [, title]) - Open a file selector window.\n\
The selected filename is used as argument to a function callback f(name)\n\
that you must provide. 'title' is optional and defaults to 'SELECT FILE'.\n\n\
Example:\n\n\
import Blender\n\n\
def my_function(filename):\n\
  print 'The selected file was: ', filename\n\n\
Blender.Window.FileSelector(my_function, 'SAVE FILE')\n";

char M_Window_ImageSelector_doc[] =
"(callback [, title]) - Open an image selector window.\n\
The selected filename is used as argument to a function callback f(name)\n\
that you must provide. 'title' is optional and defaults to 'SELECT IMAGE'.\n\n\
Example:\n\n\
import Blender\n\n\
def my_function(filename):\n\
  print 'The selected image file was: ', filename\n\n\
Blender.Window.ImageSelector(my_function, 'LOAD IMAGE')\n";

char M_Window_DrawProgressbar_doc[] =
"(done, text) - Draw a progressbar.\n\
'done' is a float value <= 1.0, 'text' contains info about what is\n\
currently being done.";

/*****************************************************************************/
/* Python method structure definition for Blender.Window module:             */
/*****************************************************************************/
struct PyMethodDef M_Window_methods[] = {
  {"Redraw",     M_Window_Redraw,     METH_VARARGS, M_Window_Redraw_doc},
  {"RedrawAll",  M_Window_RedrawAll,  METH_VARARGS, M_Window_RedrawAll_doc},
  {"QRedrawAll", M_Window_QRedrawAll, METH_VARARGS, M_Window_QRedrawAll_doc},
  {"FileSelector", M_Window_FileSelector, METH_VARARGS, M_Window_FileSelector_doc},
  {"ImageSelector", M_Window_ImageSelector, METH_VARARGS,
          M_Window_ImageSelector_doc},
  {"DrawProgressbar", M_Window_DrawProgressbar,  METH_VARARGS,
          M_Window_DrawProgressbar_doc},
  {NULL, NULL, 0, NULL}
};

#endif /* EXPP_WINDOW_H */
