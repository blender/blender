/* BlenderPython Main routine header *
   $Id$
  
  ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version. The Blender
  Foundation also sells licenses for use in proprietary software under
  the Blender License.  See http://www.blender.org/BL/ for information
  about this.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
  The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
  All rights reserved.
 
  The Original Code is: all of this file.
 
  Contributor(s): none yet.
 
  ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
//   Note: Functions prefixed with BPY_ are called from blenkernel routines */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Python.h" /* The python includes themselves. */
#include "compile.h" /* to give us PyCodeObject */
#include "eval.h" /*  for PyEval_EvalCode.h */

/* blender stuff */
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_fileops.h" /* string handling of filenames */

#include "BKE_bad_level_calls.h"
// #include "BKE_editmesh.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BLO_genfile.h" // for BLO_findstruct_offset only
#include "BKE_text.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_text.h"

#include "b_interface.h"

/* prototypes of externally used functions are HERE */
#include "BPY_extern.h"

	/* I just chucked some prototypes
	 * here... not sure where they should
	 * really be. -zr
	 */
extern struct ID * script_link_id;

extern PyObject *g_blenderdict;
extern int g_window_redrawn;
extern int disable_force_draw;

void window_update_curCamera(Object *);
PyObject *ConstObject_New(void);
void insertConst(PyObject *dict, char *name, PyObject *item);
PyObject *Windowmodule_Redraw(PyObject *self, PyObject *args);

char *event_to_name(short event);
void syspath_append(PyObject *dir);
void init_syspath(void);
void set_scriptlinks(ID *id, short event);
void release_scriptlinks(ID *id);

