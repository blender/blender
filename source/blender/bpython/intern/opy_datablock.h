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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BSE_edit.h" // for getname_<  >_ei()
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

/* for a few protos: only*/
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_ika.h"
#include "BKE_ipo.h"
#include "BKE_key.h"

#include "BLI_blenlib.h"
#include "BLO_genfile.h" // BLO_findstruct_offset()
#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_ika_types.h"

#include "BPY_constobject.h"

typedef struct _NamedEnum {
	char *name;
	int num;
} NamedEnum;

extern PyTypeObject DataBlock_Type;

#define DataBlock_Check(v)      ((v)->ob_type == &DataBlock_Type)

typedef struct _DataBlockProperty {
	char *public_name;
	char *struct_name;

	int type;
#define DBP_TYPE_CHA	1	/* Char item				*/
#define DBP_TYPE_SHO	2	/* Short item				*/
#define DBP_TYPE_INT	3	/* Int item					*/
#define DBP_TYPE_FLO	4	/* Float item				*/
#define DBP_TYPE_VEC	5	/* Float vector object		*/
#define DBP_TYPE_FUN	6	/* Extra2 hold function to convert ptr->ob
								extra3 holds function to convert ob->ptr */

	int stype;
#define DBP_TYPE_NON	0
	
	float min;				/* Minimum allowed value	*/
	float max;				/* Maximum allowed value	*/
	
	int idx[4];
	int dlist[4];

	int handling;
#define DBP_HANDLING_NONE	0	/* No special handling required			*/
#define DBP_HANDLING_FUNC	1	/* Extra1 is used to retrieve ptr		*/
#define DBP_HANDLING_NENM	2	/* Extra1 holds named enum to resolve 
									values from/to. */
	
	void *extra1;
	void *extra2;
	void *extra3;
} DataBlockProperty;


/* function pointers needed for callbacks */

typedef void *(*DBGetPtrFP) (void *struct_ptr, char *name, int forsetting);
typedef PyObject *	(*DBPtrToObFP) (void **ptr);
typedef int	(*DBSetPtrFP) (void *struct_ptr, char *name, PyObject *ob);
typedef PyObject *(*DBConvertfunc) (void *data);


typedef struct {
	PyObject_HEAD
	void            *data;
	char            *type;
	ListBase        *type_list;
	DataBlockProperty *properties;
} DataBlock;

/* PROTOS */

/* opy_datablock.c */
PyObject  *BPY_PyList_FromIDList(ListBase *list, DBConvertfunc convertfunc);
PyObject  *get_DataBlock_func(void **p);

PyObject  *py_find_from_list (ListBase *list, PyObject *args);
PyObject  *named_enum_get (int val, NamedEnum *enums);
int        named_enum_set (char *name, NamedEnum *enums);
PyObject  *datablock_getattr (DataBlockProperty *props, char *structname,
           char *name, void *struct_ptr);
int        datablock_setattr (DataBlockProperty *props, char *structname,
           char *name, void *struct_ptr, PyObject *setto);
PyObject   *datablock_assignIpo(DataBlock *block, DataBlock *ipoblock);


/* DataBlock Methods */

void       DataBlock_dealloc (DataBlock *self);
int	       DataBlock_print (PyObject *self, FILE *fp, int flags);
PyObject  *DataBlock_getattr (PyObject *self, char *name);
int        DataBlock_setattr (PyObject *self, char *name, PyObject *ob);
int        DataBlock_type(DataBlock *block);
int        DataBlock_isType (DataBlock *block, int type);
PyObject  *DataBlock_fromData (void *data);
PyObject  *DataBlock_link(PyObject *self, PyObject *args);

PyObject  *make_icu_list (ListBase *curves);
void       pybzt_dealloc (PyObject *self);
int        pybzt_print (PyObject *self, FILE *fp, int flags);
PyObject  *pybzt_getattr (PyObject *self, char *name);
int        pybzt_setattr (PyObject *self, char *name, PyObject *ob);
PyObject  *pybzt_create (PyObject *self, PyObject *args);
PyObject  *pybzt_from_bzt (BezTriple *bzt);
void       pyicu_dealloc (PyObject *self);
int        pyicu_print (PyObject *self, FILE *fp, int flags);
PyObject  *pyicu_getattr (PyObject *self, char *name);
int        pyicu_setattr (PyObject *self, char *name, PyObject *ob);
PyObject  *pyicu_from_icu (IpoCurve *icu);
PyObject  *Ipo_Recalc (PyObject *self, PyObject *args);
PyObject  *Ipo_Eval (PyObject *self, PyObject *args);
void       init_types (PyObject *dict);
void       init_Datablockmodules (PyObject *dict);
PyObject  *getInverseMatrix(void *vdata);


/* Object module */
void      *Object_special_getattr(void *vdata, char *name);
int        Object_special_setattr(void *vdata, char *name, PyObject *py_ob);

extern PyObject           *initObject(void);
extern struct PyMethodDef  Objectmodule_methods[];
extern struct PyMethodDef  Object_methods[];
extern DataBlockProperty   Object_Properties[];

extern struct PyMethodDef Imagemodule_methods[];
extern DataBlockProperty Image_Properties[];
extern PyObject *initScene(void);
extern struct PyMethodDef Scenemodule_methods[];

extern PyObject *initIpo(void);
extern struct PyMethodDef Ipo_methods[];
extern struct PyMethodDef Ipomodule_methods[];
extern DataBlockProperty Ipo_Properties[];


extern struct PyMethodDef Textmodule_methods[];
extern struct PyMethodDef Text_methods[];
extern DataBlockProperty Text_Properties[];



struct Material;

struct 
Material **newMaterialList_fromPyList(PyObject *list);
PyObject  *PyList_fromMaterialList(struct Material **matlist, int len);

