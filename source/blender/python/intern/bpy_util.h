/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <Python.h>

#include "bpy_compat.h"

/* for internal use only, so python can interchange a sequence of strings with flags */
typedef struct BPY_flag_def {
    const char	*name;
    int			flag;
} BPY_flag_def;


PyObject *BPY_flag_to_list(BPY_flag_def *flagdef, int flag);
int BPY_flag_from_seq(BPY_flag_def *flagdef, PyObject *seq, int *flag);

void PyObSpit(char *name, PyObject *var);
void PyLineSpit(void);
void BPY_getFileAndNum(char **filename, int *lineno);

/* own python like utility function */
PyObject *PyObject_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...);
