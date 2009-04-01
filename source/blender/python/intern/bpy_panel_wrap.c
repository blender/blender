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

#include "bpy_panel_wrap.h"
#include "bpy_rna.h"
#include "bpy_util.h"
#include "bpy_compat.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_listbase.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "DNA_screen_types.h"
#include "MEM_guardedalloc.h"
#include "ED_screen.h"

#define PYPANEL_ATTR_UINAME		"__label__"
#define PYPANEL_ATTR_IDNAME		"__name__"	/* use pythons class name */
#define PYPANEL_ATTR_CONTEXT	"__context__"

#define PYPANEL_DRAW 1
#define PYPANEL_POLL 2

static int PyPanel_generic(int mode, const bContext *C, Panel *pnl)
{
	PyObject *py_class= (PyObject *)(pnl->type->py_data);
	//uiLayout *layout= pnl->layout;

	PyObject *args;
	PyObject *ret= NULL, *py_class_instance, *item;
	int ret_flag= 0;

	PyGILState_STATE gilstate = PyGILState_Ensure();

	args = PyTuple_New(0);
	py_class_instance = PyObject_Call(py_class, args, NULL);
	Py_DECREF(args);

	if (py_class_instance) { /* Initializing the class worked, now run its invoke function */
		PointerRNA context_ptr;
		
		if (mode==PYPANEL_DRAW) {
			item= PyObject_GetAttrString(py_class, "draw");
		}
		else if (mode==PYPANEL_POLL) {
			item= PyObject_GetAttrString(py_class, "poll");
		}
		args = PyTuple_New(2);
		PyTuple_SET_ITEM(args, 0, py_class_instance);

		RNA_pointer_create(NULL, &RNA_Context, (void *)C, &context_ptr);

		PyTuple_SET_ITEM(args, 1, pyrna_struct_CreatePyObject(&context_ptr));

		ret = PyObject_Call(item, args, NULL);

		Py_DECREF(args);
		Py_DECREF(item);
	}

	if (ret == NULL) { /* covers py_class_instance failing too */
		PyErr_Print(); /* XXX use reporting api? */
	}
	else {
		if (mode==PYPANEL_POLL) {
			if (PyBool_Check(ret) == 0) {
				PyErr_SetString(PyExc_ValueError, "Python poll function return value ");
				PyErr_Print(); /* XXX use reporting api? */
			}
			else {
				ret_flag= ret==Py_True ? 1:0;
			}
		}
		else {
			//XXX - draw stuff
		}

		Py_DECREF(ret);
	}
	PyGILState_Release(gilstate);
	
	return ret_flag;
}

static void PyPanel_draw(const bContext *C, Panel *pnl)
{
	PyPanel_generic(PYPANEL_DRAW, C, pnl);
}

static int PyPanel_poll(const bContext *C)
{
	//return PyPanel_generic(PYPANEL_POLL, C, NULL);
	return 1; // XXX we need the panel type to access the PyObject grr!
}

/* pyOperators - Operators defined IN Python */
PyObject *PyPanel_wrap_add(PyObject *self, PyObject *args)
{
	PyObject *item;
	PyObject *py_class;
	char *space_identifier;
	char *region_identifier;
	int space_value;
	int region_value;

	PanelType *pt;
	SpaceType *st;
	ARegionType *art;

	static struct BPY_class_attr_check pypnl_class_attr_values[]= {
		{PYPANEL_ATTR_IDNAME,		's', 0,	0},
		{PYPANEL_ATTR_UINAME,		's', 0,	0},
		{PYPANEL_ATTR_CONTEXT,	's', 0,	0},
		{"draw",	'f', 2,	0}, /* Do we need the Panel struct?, could be an extra arg */
		{"poll",	'f', 2,	BPY_CLASS_ATTR_OPTIONAL},
		{NULL, 0, 0, 0}};

	enum {
		PYPANEL_ATTR_IDNAME_IDX=0,
		PYPANEL_ATTR_UINAME_IDX,
		PYPANEL_ATTR_CONTEXT_IDX,
		PYPANEL_ATTR_DRAW_IDX,
		PYPANEL_ATTR_POLL_IDX
	};

	PyObject *pypnl_class_attrs[6]= {NULL, NULL, NULL, NULL, NULL, NULL};

	if( !PyArg_ParseTuple( args, "Oss:addPanel", &py_class, &space_identifier, &region_identifier))
		return NULL;

	/* Should this use a base class? */
	if (BPY_class_validate("Panel", py_class, NULL, pypnl_class_attr_values, pypnl_class_attrs) < 0) {
		return NULL; /* BPY_class_validate sets the error */
	}

	if (RNA_enum_value_from_id(space_type_items, space_identifier, &space_value)==0) {
		char *cstring= BPy_enum_as_string(space_type_items);
		PyErr_Format( PyExc_AttributeError, "SpaceType \"%s\" is not one of [%s]", space_identifier, cstring);
		MEM_freeN(cstring);
		return NULL;
	}

	if (RNA_enum_value_from_id(region_type_items, region_identifier, &region_value)==0) {
		char *cstring= BPy_enum_as_string(region_type_items);
		PyErr_Format( PyExc_AttributeError, "RegionType \"%s\" is not one of [%s]", region_identifier, cstring);
		MEM_freeN(cstring);
		return NULL;
	}

	st= BKE_spacetype_from_id(space_value);

#if 0
	// for printing what panels we have
	for(art= st->regiontypes.first; art; art= art->next) {
		
		printf("REG %d\n", art->regionid);

		for(pt= art->paneltypes.first; pt; pt= pt->next) {
			printf("\tREG %s %s - %s\n", pt->idname, pt->name, pt->context);
		}
	}
#endif

	for(art= st->regiontypes.first; art; art= art->next) {
		if (art->regionid==region_value)
			break;
	}
	
	if (art==NULL) {
		PyErr_Format( PyExc_AttributeError, "SpaceType \"%s\" does not have a UI region '%s'", space_identifier, region_identifier);
		return NULL;
	}

	pt= MEM_callocN(sizeof(PanelType), "python buttons panel");
	pt->idname= _PyUnicode_AsString(pypnl_class_attrs[PYPANEL_ATTR_IDNAME_IDX]);

	item= pypnl_class_attrs[PYPANEL_ATTR_UINAME_IDX];
	pt->name= item ? _PyUnicode_AsString(item):pt->idname;

	pt->context= _PyUnicode_AsString(pypnl_class_attrs[PYPANEL_ATTR_CONTEXT_IDX]);

	if (pypnl_class_attrs[PYPANEL_ATTR_POLL_IDX])
		pt->poll= PyPanel_poll;
	
	pt->draw= PyPanel_draw;

	Py_INCREF(py_class);
	pt->py_data= (void *)py_class;

	BLI_addtail(&art->paneltypes, pt);
	Py_RETURN_NONE;
}

PyObject *PyPanel_wrap_remove(PyObject *self, PyObject *args)
{
	// XXX - todo
	Py_RETURN_NONE;
}

