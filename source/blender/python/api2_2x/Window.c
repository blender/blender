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

#include "Window.h"

/* Many parts of the code here come from the older bpython implementation
 * (file opy_window.c) */

/*****************************************************************************/
/* Function:              M_Window_Redraw                                    */
/* Python equivalent:     Blender.Window.Redraw                              */
/*****************************************************************************/
PyObject *M_Window_Redraw(PyObject *self, PyObject *args)
{ /* not static so py_slider_update in Draw.[ch] can use it */
  ScrArea *tempsa, *sa;
  SpaceText *st;
  int wintype = SPACE_VIEW3D;
  short redraw_all = 0;

  if (!PyArg_ParseTuple(args, "|i", &wintype))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected int argument (or nothing)"));

  if (wintype < 0)
    redraw_all = 1;

  if (!during_script()) { /* XXX check this */
    tempsa= curarea;
    sa = G.curscreen->areabase.first;

    while (sa) {

      if (sa->spacetype == wintype || redraw_all) {
        /* don't force-redraw Text window (Python GUI) when
           redraw is called out of a slider update */
        if (sa->spacetype == SPACE_TEXT) {
          st = sa->spacedata.first;
          if (st->text->flags & TXT_FOLLOW) /* follow cursor display */
            pop_space_text(st);
          if (EXPP_disable_force_draw) { /* from Draw.[ch] ... */
            scrarea_queue_redraw(sa);
          }


        } else {
          scrarea_do_windraw(sa);
          if (sa->headwin) scrarea_do_headdraw(sa);
        }
      }

      sa= sa->next;
    }

    if (curarea != tempsa) areawinset (tempsa->win);

    if (curarea->headwin) scrarea_do_headdraw (curarea);

    screen_swapbuffers();
  }

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:              M_Window_RedrawAll                                 */
/* Python equivalent:     Blender.Window.RedrawAll                           */
/*****************************************************************************/
static PyObject *M_Window_RedrawAll(PyObject *self, PyObject *args)
{
  return M_Window_Redraw(self, Py_BuildValue("(i)", -1));
}

/*****************************************************************************/
/* Function:              M_Window_QRedrawAll                                */
/* Python equivalent:     Blender.Window.QRedrawAll                          */
/*****************************************************************************/
static PyObject *M_Window_QRedrawAll(PyObject *self, PyObject *args)
{
  allqueue(REDRAWALL, 0);

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:              M_Window_FileSelector                              */
/* Python equivalent:     Blender.Window.FileSelector                        */
/*****************************************************************************/

/* This is the callback to "activate_fileselect" below.  It receives the
 * selected filename and (using it as argument) calls the Python callback
 * provided by the script writer and stored in EXPP_FS_PyCallback. */

static void getSelectedFile(char *name)
{
  if (EXPP_FS_PyCallback) {
	  SpaceText *st= curarea->spacedata.first;

    PyObject_CallFunction((PyObject *)EXPP_FS_PyCallback, "s", name);

		EXPP_FS_PyCallback = NULL;
		st->flags &= ST_CLEAR_NAMESPACE; /* free global dictionary */
  }
}

static PyObject *M_Window_FileSelector(PyObject *self, PyObject *args)
{
  char *title = "SELECT FILE";
	SpaceText *st = curarea->spacedata.first;

  if (!PyArg_ParseTuple(args, "O!|s",
                        &PyFunction_Type, &EXPP_FS_PyCallback, &title))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
    "\nexpected a callback function (and optionally a string) as argument(s)"));

	st->flags &= ~ST_CLEAR_NAMESPACE; /* hold global dictionary */

  activate_fileselect(FILE_BLENDER, title, G.sce, getSelectedFile);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *M_Window_ImageSelector(PyObject *self, PyObject *args)
{
  char *title = "SELECT IMAGE";
	SpaceText *st = curarea->spacedata.first;

  if (!PyArg_ParseTuple(args, "O!|s",
                        &PyFunction_Type, &EXPP_FS_PyCallback, &title))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
    "\nexpected a callback function (and optionally a string) as argument(s)"));

	st->flags &= ~ST_CLEAR_NAMESPACE; /* hold global dictionary */

  activate_imageselect(FILE_BLENDER, title, G.sce, getSelectedFile);

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:              M_Window_DrawProgressBar                           */
/* Python equivalent:     Blender.Window.DrawProgressBar                     */
/*****************************************************************************/
static PyObject *M_Window_DrawProgressBar(PyObject *self, PyObject *args)
{
  float done;
  char *info = 0;
  int retval;

  if(!PyArg_ParseTuple(args, "fs", &done, &info))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected a float and a string as arguments"));

  retval = progress_bar(done, info);

  return Py_BuildValue("i", retval);
}

/*****************************************************************************/
/* Function:              Window_Init                                        */
/*****************************************************************************/
PyObject *Window_Init (void)
{
  PyObject  *submodule, *Types;

  printf ("In M_Window_Init()\n");

  submodule = Py_InitModule3("Blender.Window", M_Window_methods, M_Window_doc);

  Types = Py_BuildValue("{s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h}",
                  "VIEW3D", SPACE_VIEW3D, "IPO", SPACE_IPO, "OOPS", SPACE_OOPS,
                  "BUTS", SPACE_BUTS, "FILE", SPACE_FILE, "IMAGE", SPACE_IMAGE,
                  "INFO", SPACE_INFO, "SEQ", SPACE_SEQ, "IMASEL", SPACE_IMASEL,
                  "SOUND", SPACE_SOUND, "ACTION", SPACE_ACTION,
                  "TEXT", SPACE_TEXT, "NLA", SPACE_NLA);

  if (Types) PyModule_AddObject(submodule, "Types", Types);

  return submodule;
}

