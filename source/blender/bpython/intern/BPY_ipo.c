/** Ipo module; access to Ipo datablocks in Blender
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
  *
  */
#include "MEM_guardedalloc.h"

#include "Python.h"
#include "BPY_macros.h"
#include "BPY_tools.h"

#include "b_interface.h" // most datatypes

#include "opy_datablock.h"

#include "DNA_curve_types.h"

#include "BSE_editipo.h"

/* GLOBALS */

/* These should be put into a proper dictionary for quicker retrieval..*/

NamedEnum g_OB_ipocodes[] = {

	{ "LocX",   OB_LOC_X },
	{ "LocY",   OB_LOC_Y },
	{ "LocZ",   OB_LOC_Z },
	{ "dLocX",  OB_DLOC_X },
	{ "dLocY",  OB_DLOC_Y },
	{ "dLocZ",  OB_DLOC_Z },
	{ "RotX",   OB_ROT_X },
	{ "RotY",   OB_ROT_Y },
	{ "RotZ",   OB_ROT_Z },
	{ "dRotX",  OB_DROT_X },
	{ "dRotY",  OB_DROT_Y },
	{ "dRotZ",  OB_DROT_Z },
	{ "SizeX",  OB_SIZE_X },
	{ "SizeY",  OB_SIZE_Y },
	{ "SizeY",  OB_SIZE_Z },
	{ "dSizeX", OB_DSIZE_X },
	{ "dSizeY", OB_DSIZE_Y },
	{ "dSizeY", OB_DSIZE_Z },
	{ "Layer",  OB_LAY },
	{ "Time",   OB_TIME },
	{ 0, 0 }
};

NamedEnum g_MA_ipocodes[] = {

	{ "R",      MA_COL_R },
	{ "G",      MA_COL_G },
	{ "B",      MA_COL_B },
	{ "Alpha",  MA_ALPHA},
	{ "SpecR",  MA_SPEC_R },
	{ "SpecG",  MA_SPEC_G },
	{ "SpecB",  MA_SPEC_B },
	{ "MirR",   MA_MIR_R },
	{ "MirG",   MA_MIR_G },
	{ "MirB",   MA_MIR_B },
	{ "Emit",   MA_EMIT },
	{ "Amb",    MA_AMB },
	{ "Spec",   MA_SPEC },
	{ "Hard",   MA_HARD },
	{ "SpTra",  MA_SPTR },
	{ "Ang",    MA_ANG },
	{ "HaSize", MA_HASIZE },
	{ 0, 0 }
};

NamedEnum g_WO_ipocodes[] = {
	{ "HorR",   WO_HOR_R },
	{ "HorG",   WO_HOR_G },
	{ "HorB",   WO_HOR_B },
	{ "ZenR",   WO_ZEN_R },
	{ "ZenG",   WO_ZEN_G },
	{ "ZenB",   WO_ZEN_B },
	{ "Expos",  WO_EXPOS },
	{ "Misi",   WO_MISI },
	{ "MisDi",  WO_MISTDI },
	{ "MisSta", WO_MISTSTA },
	{ "MisHi",  WO_MISTHI },
	{ "StarR",  WO_STAR_R },
	{ "StarG",  WO_STAR_G },
	{ "StarB",  WO_STAR_B },
	{ "StarDi", WO_STARDIST },
	{ "StarSi", WO_STARSIZE },
	{ 0, 0 }
};

NamedEnum g_CA_ipocodes[] = {
	{ "Lens",  CAM_LENS },
	{ "ClSta", CAM_STA },
	{ "ClEnd", CAM_END },
	{ 0, 0 }
};

PyObject *g_ipoBlockTypes;           // global for ipo type container
PyObject *g_interpolationTypes; // global for interpolation type container
PyObject *g_extrapolationTypes; // global for extrapolation type container

typedef struct _PyBezTriple {
	PyObject_VAR_HEAD
	
	BezTriple bzt;
} PyBezTriple;


void pybzt_dealloc(PyObject *self) {
	PyMem_DEL(self);
}

PyObject *pybzt_repr(PyObject *self) {
	return PyString_FromString("[BezTriple]");
}

/* XXX */

NamedEnum bez_triple_flags[]= {
	{"Free", HD_FREE},
	{"Auto", HD_AUTO},
	{"Vect", HD_VECT},
	{"Align", HD_ALIGN},
	{NULL}
};

DataBlockProperty BezTriple_Properties[]= {
	{"h1", 	"vec[3][3]", 	DBP_TYPE_VEC, 0, 2.0, 0.0, {0,0}, {3,3,-sizeof(float)}}, 
	{"pt", 	"vec[3][3]", 	DBP_TYPE_VEC, 0, 2.0, 0.0, {1,0}, {3,3,-sizeof(float)}}, 
	{"h2", 	"vec[3][3]", 	DBP_TYPE_VEC, 0, 2.0, 0.0, {2,0}, {3,3,-sizeof(float)}}, 
	
	{"f1", 	"f1", 			DBP_TYPE_CHA, 0, 0.0, 1.0},
	{"f2", 	"f2", 			DBP_TYPE_CHA, 0, 0.0, 1.0},
	{"f3", 	"f3", 			DBP_TYPE_CHA, 0, 0.0, 1.0},

	{"h1Type", "h1", 		DBP_TYPE_SHO, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_NENM, bez_triple_flags},
	{"h2Type", "h2", 		DBP_TYPE_SHO, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_NENM, bez_triple_flags},

	{"h1t", "h1", 			DBP_TYPE_SHO, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_NENM, bez_triple_flags},
	{"h2t", "h2", 			DBP_TYPE_SHO, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_NENM, bez_triple_flags},
	
	{NULL}
};

PyObject *pybzt_getattr(PyObject *self, char *name) {
	PyBezTriple *pybzt= (PyBezTriple *) self;

	return datablock_getattr(BezTriple_Properties, "BezTriple", name, &pybzt->bzt);
}

int pybzt_setattr(PyObject *self, char *name, PyObject *ob) {
	PyBezTriple *pybzt= (PyBezTriple *) self;

	return datablock_setattr(BezTriple_Properties, "BezTriple", name, &pybzt->bzt, ob);
}

PyTypeObject PyBezTriple_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"BezTriple",					/*tp_name*/
	sizeof(PyBezTriple),			/*tp_basicsize*/
	0,								/*tp_itemsize*/
	/* methods */
	(destructor)	pybzt_dealloc,	/*tp_dealloc*/
	(printfunc)		0,	/*tp_print*/
	(getattrfunc)	pybzt_getattr,	/*tp_getattr*/
	(setattrfunc)	pybzt_setattr,	/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc) pybzt_repr,								/*tp_repr*/
	0,								/*tp_as_number*/
	0,								/*tp_as_sequence*/
	0,								/*tp_as_mapping*/
	0,								/*tp_hash*/	
};

static char pybzt_create_doc[]= "() - Create a new BezTriple object";
PyObject *pybzt_create(PyObject *self, PyObject *args) {
	PyBezTriple *py_bzt= PyObject_NEW(PyBezTriple, &PyBezTriple_Type);
	// BezTriple *bzt= &py_bzt->bzt;
	
	BPY_TRY(PyArg_ParseTuple(args, ""));
	
	memset(&py_bzt->bzt,0,sizeof(py_bzt->bzt));
	
	return (PyObject *) py_bzt;
}

PyObject *pybzt_from_bzt(BezTriple *bzt) {
	PyBezTriple *py_bzt= PyObject_NEW(PyBezTriple, &PyBezTriple_Type);
	
	memcpy(&py_bzt->bzt, bzt, sizeof(*bzt));
	
	return (PyObject *) py_bzt;
}


typedef struct _PyIpoCurve {
	PyObject_VAR_HEAD

	IpoCurve *icu;
} PyIpoCurve;


/********************/
/* IpoCurve methods */

#undef MethodDef
#define MethodDef(func) _MethodDef(func, IpoCurve)

#define DICT_FROM_CONSTDICT(x) \
	((constobject *) x)->dict

/** sets an enum int value 'by name' from the dictionary dict */

static PyObject *setEnum_fromDict(short *i, PyObject *dict, char *key, char *errmsg)
{
	PyObject *p;
	p = PyDict_GetItemString(dict, key);
	if (!p) {
		PyErr_SetString(PyExc_TypeError, errmsg);
		return NULL;
	}

	*i = (short) PyInt_AsLong(p);
	return BPY_incr_ret(Py_None);
}

static char IpoCurve_setInterpolation_doc[] = 
"(type) - Set interpolation to one of: ['Constant', 'Linear', 'Bezier']";

static PyObject *IpoCurve_setInterpolation(PyObject *self, PyObject *args)
{
	char *typename;
	IpoCurve *ipocurve = (IpoCurve *) ((PyIpoCurve *) self)->icu;

	BPY_TRY(PyArg_ParseTuple(args, "s", &typename));

	return setEnum_fromDict(&ipocurve->ipo, DICT_FROM_CONSTDICT(g_interpolationTypes),
		typename, "Improper interpolation type, see Ipo.InterpolationTypes");

}

static char IpoCurve_setExtrapolation_doc[] = 
"(type) - Set interpolation to one of: ['Constant', 'Linear', 'Cyclic', 'CyclicLinear']";

static PyObject *IpoCurve_setExtrapolation(PyObject *self, PyObject *args)
{
	char *typename;
	IpoCurve *ipocurve = (IpoCurve *) ((PyIpoCurve *) self)->icu;

	BPY_TRY(PyArg_ParseTuple(args, "s", &typename));


	return setEnum_fromDict(&ipocurve->extrap, DICT_FROM_CONSTDICT(g_extrapolationTypes), 
		typename, "Improper extrapolation type, see Ipo.ExtrapolationTypes");
}

static char IpoCurve_getInterpolation_doc[] = 
"() - Returns interpolation type";

static PyObject *IpoCurve_getInterpolation(PyObject *self, PyObject *args)
{
	IpoCurve *ipocurve = (IpoCurve *) ((PyIpoCurve *) self)->icu;

	switch (ipocurve->ipo) {
	case IPO_CONST:	return PyString_FromString("Constant");
	case IPO_LIN:	return PyString_FromString("Linear");
	case IPO_BEZ:	return PyString_FromString("Bezier");			
	default:        return PyString_FromString("<not defined>"); 
	}
}

static char IpoCurve_getExtrapolation_doc[] = 
"() - Returns extrapolation type";

static PyObject *IpoCurve_getExtrapolation(PyObject *self, PyObject *args)
{
	IpoCurve *ipocurve = (IpoCurve *) ((PyIpoCurve *) self)->icu;

	switch (ipocurve->extrap) {
	case IPO_HORIZ:	return PyString_FromString("Constant");
	case IPO_DIR:	return PyString_FromString("Linear");
	case IPO_CYCL:	return PyString_FromString("Cyclic");					
	case IPO_CYCLX:	return PyString_FromString("CyclicLinear");					
	default:        return PyString_FromString("<not defined>"); 
	}

}

static char IpoCurve_eval_doc[] = 
"(time = <current frame>) - evaluates ipo at time 'time' and returns result\n\
(float). If 'time' is not specified, the current frame value is taken";

static PyObject *IpoCurve_eval(PyObject *self, PyObject *args)
{
	PyIpoCurve *pIpocurve = (PyIpoCurve *) self;
	float time = CurrentFrame;

	BPY_TRY(PyArg_ParseTuple(args, "|f", &time));
	
	return PyFloat_FromDouble(eval_icu(pIpocurve->icu, time));
}

static char IpoCurve_update_doc[] = 
"() - update and recalculate IpoCurve";

static PyObject *IpoCurve_update(PyObject *self, PyObject *args)
{

	BPY_TRY(PyArg_ParseTuple(args, ""));

	testhandles_ipocurve(((PyIpoCurve *) self)->icu); // recalculate IPO

	Py_INCREF(Py_None);
	return Py_None;
}

static struct PyMethodDef IpoCurve_methods[] = {
	MethodDef(setInterpolation), 
	MethodDef(getInterpolation), 
	MethodDef(setExtrapolation), 
	MethodDef(getExtrapolation), 
	MethodDef(eval), 
	MethodDef(update), 
	{NULL, NULL}
};


PyObject *IpoCurve_getattr(PyObject *self, char *name) {
	PyIpoCurve *py_icu= (PyIpoCurve *) self;
	IpoCurve *icu= py_icu->icu;

	if (STREQ(name, "type")) {
		return IpoCurve_getInterpolation(self, Py_BuildValue(""));
	} else if (STREQ(name, "extend")) {
		return IpoCurve_getExtrapolation(self, Py_BuildValue(""));
	} else if (STREQ(name, "name")) {
		char icu_name[32]= "";
		
		switch (icu->blocktype) {
		case ID_OB:
			getname_ob_ei(icu->adrcode, icu_name, 0);
			break;
		case ID_MA:
			getname_mat_ei(icu->adrcode, icu_name);
			break;
		case ID_WO:
			getname_world_ei(icu->adrcode, icu_name);
			break;
		case ID_SEQ:
			getname_seq_ei(icu->adrcode, icu_name);
			break;
		case ID_CU:
			getname_cu_ei(icu->adrcode, icu_name);
			break;
		case ID_KE:
			getname_key_ei(icu->adrcode, icu_name);
			break;
		case ID_LA:
			getname_la_ei(icu->adrcode, icu_name);
			break;
		case ID_CA:
			getname_cam_ei(icu->adrcode, icu_name);
			break;
		default:
			return PyString_FromString("<unknown>");
		}
		
		return PyString_FromString(icu_name);
	} else if (STREQ(name, "points")) {
		PyObject *list= PyList_New(icu->totvert);
		BezTriple *bzt= icu->bezt;
		int i;
		
		for (i=0; i<icu->totvert; i++) {
			PyList_SetItem(list, i, pybzt_from_bzt(bzt));
			bzt++;
		}
		
		return list;
	} 
	return Py_FindMethod(IpoCurve_methods, (PyObject*)self, name);
}

int IpoCurve_setattr(PyObject *self, char *name, PyObject *ob) {
	PyIpoCurve *py_icu= (PyIpoCurve *) self;
	IpoCurve *icu= py_icu->icu;

	if (STREQ(name, "points")) {
		int i, len;
		BezTriple *bzt;
		
		if (!PySequence_Check(ob) || !BPY_check_sequence_consistency(ob, &PyBezTriple_Type))
			return py_err_ret_int(PyExc_AttributeError, "Expected list of BezTriples");

		len= PySequence_Length(ob);

		if (icu->bezt) // free existing (IF)
			MEM_freeN(icu->bezt);

		icu->totvert= len;
		if (len) icu->bezt= MEM_mallocN(len*sizeof(BezTriple), "beztriples");
		
		bzt= icu->bezt;
		for (i=0; i<len; i++) {
			PyBezTriple *pybzt= (PyBezTriple*) PySequence_GetItem(ob, i);
			
			memcpy(bzt, &pybzt->bzt, sizeof(BezTriple));
			bzt++;
			
			Py_DECREF(pybzt);
		}

		/* Twice for auto handles */
		calchandles_ipocurve(icu);
		calchandles_ipocurve(icu);

		boundbox_ipocurve(icu);
		sort_time_ipocurve(icu);
		
		return 0;
	}

	PyErr_SetString(PyExc_AttributeError, name);
	return -1;	
}

void IpoCurve_dealloc(PyObject *self) {
	PyMem_DEL(self);
}

PyObject *IpoCurve_repr(PyObject *self) {
	char s[256];
	sprintf (s, "[IpoCurve %.32s]", 
	         PyString_AsString(IpoCurve_getattr(self, "name")));
	return Py_BuildValue("s", s);
}

PyTypeObject PyIpoCurve_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"IpoCurve",						/*tp_name*/
	sizeof(PyIpoCurve),				/*tp_basicsize*/
	0,								/*tp_itemsize*/
	/* methods */
	(destructor)	IpoCurve_dealloc,	/*tp_dealloc*/
	(printfunc)		0,	/*tp_print*/
	(getattrfunc)	IpoCurve_getattr,	/*tp_getattr*/
	(setattrfunc)	IpoCurve_setattr,	/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc) IpoCurve_repr,		/*tp_repr*/
	0,								/*tp_as_number*/
	0,								/*tp_as_sequence*/
	0,								/*tp_as_mapping*/
	0,								/*tp_hash*/	
};

PyObject *IpoCurve_from_icu(IpoCurve *icu) {
	PyIpoCurve *ob= PyObject_NEW(PyIpoCurve, &PyIpoCurve_Type);
	
	ob->icu= icu;
	
	return (PyObject *) ob;
}

PyObject *make_icu_list (ListBase *curves) {
	ListBase lb= *curves;
	IpoCurve *icu= (IpoCurve *) lb.first;
	PyObject *list= PyList_New(0);
	PyObject *pyipo;
	
	while (icu) {
		pyipo = IpoCurve_from_icu(icu);
		PyList_Append(list, pyipo);
		Py_DECREF(pyipo);
		icu= icu->next;
	}
	
	return list;	
}

DataBlockProperty Ipo_Properties[]= {
	{"curves", "curve", DBP_TYPE_FUN, 0, 0.0, 0.0, {0}, {0}, 0, 0, make_icu_list}, 
	{NULL}	
};


/**********************/
/* Ipo module methods */

DATABLOCK_GET(Ipomodule, ipo, getIpoList())

static char Ipomodule_New_doc[] = 
"(type, name = <default>) - Creates a new Ipo block of the specified type,\n\
which must be of the appropriate datablock ID type (e.g. ID_OB, ID_MA, ...)";


static PyObject *Ipomodule_New(PyObject *self, PyObject *args)
{
	Ipo *ipo;
	int type;
	PyObject *p;
	char *name = NULL, *typename;

	BPY_TRY(PyArg_ParseTuple(args, "s|s", &typename, &name));
	p = PyDict_GetItemString(((constobject *)g_ipoBlockTypes)->dict, typename);
	if (!p) {
		PyErr_SetString(PyExc_TypeError, "Improper Ipo type, see Ipo.Types");
		return NULL;
	}
		
	type = PyInt_AsLong(p);

	if (!name) {
		switch(type) {
			case ID_OB: name = "Objpo"; break;
			case ID_MA: name = "MatIpo"; break;
			case ID_SEQ: name = "SeqIpo"; break;
			case ID_CU: name = "CurveIpo"; break;
			case ID_KE: name = "KeyIpo"; break;
			case ID_WO: name = "WorldIpo"; break;
			case ID_LA: name = "LampIpo"; break;
			case ID_CA: name = "CamIpo"; break;
			case ID_SO: name = "SndIpo"; break;
			case ID_AC: name = "ActionIpo"; break;
			default:
				PyErr_SetString(PyExc_TypeError, "Internal error, illegal type");
				return NULL;
		}		
	}		

	ipo = ipo_new(type, name);
	return DataBlock_fromData(ipo);
}

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Ipomodule)
struct PyMethodDef Ipomodule_methods[] = {

	MethodDef(New),
	MethodDef(get),
	{"BezTriple", pybzt_create, METH_VARARGS, pybzt_create_doc},
	
	{NULL, NULL}
};

/********************/
/* Ipoblock methods */

/* slow and inefficient lookup function , use proper dictionaries in future */
short code_lookup(NamedEnum *codetab, char *name)
{
	int i = 0;
	
	while(codetab[i].name)
	{
		if (!strcmp(codetab[i].name, name))
			return codetab[i].num;
		i++;		
	}	
	return -1;
}

static char Ipo_addCurve_doc[]=
"(type, curve = None) - adds IpoCurve 'curve' to the IpoBlock under type id 'type'";
PyObject *Ipo_addCurve(PyObject *self, PyObject *args) 
{
	
	Ipo *ipo = (Ipo *) ((DataBlock *) self)->data;
	NamedEnum *lookup;

	short code;

	char *type;
	PyIpoCurve *curve = NULL;
	IpoCurve *ipocurve, *existingIpoCurve;

	BPY_TRY(PyArg_ParseTuple(args, "s|O!", &type, &PyIpoCurve_Type, &curve));

	switch (ipo->blocktype) {
	case ID_OB:
		lookup = g_OB_ipocodes;
		break;
	case ID_CA:
		lookup = g_CA_ipocodes;
		break;
	case ID_MA:
		lookup = g_MA_ipocodes;
		break;
	case ID_WO:
		lookup = g_WO_ipocodes;
		break;
	default:
		PyErr_SetString(PyExc_TypeError, "Ipo type not (YET) supported");
		return NULL;
	}
	code = code_lookup(lookup, type);
	if (code == -1) {
		PyErr_SetString(PyExc_TypeError, "Unknown IpoCurve type");
		return NULL;
	}	

	if (!curve) {                             
		ipocurve = ipocurve_new();            // malloc new ipocurve
	} else {                                  // add existing curve:
		ipocurve = ipocurve_copy(curve->icu); // copy ipocurve
	}

	ipocurve->adrcode = code;             // re-code ipo
	ipocurve->blocktype = ipo->blocktype;  

	existingIpoCurve = ipo_findcurve(ipo, code);
	if (existingIpoCurve) {
		BLI_remlink(&(ipo->curve), existingIpoCurve); // remove existing
		MEM_freeN(existingIpoCurve);
	}
	BLI_addtail(&(ipo->curve), ipocurve); // add curve to list
	return IpoCurve_from_icu(ipocurve);
}

static char Ipo_update_doc[]=
"() - Recalculate the ipo and update linked objects";

PyObject *Ipo_update(PyObject *self, PyObject *args) {
	DataBlock *ipoblock = (DataBlock *) self;
	Key *key;
	
	do_ipo((Ipo *) ipoblock->data);

	/* here we should signal all objects with keys that the ipo changed */
	
	key= getKeyList()->first;
	while(key) {
		if(key->ipo == (Ipo *)ipoblock->data) do_spec_key(key);
		key= key->id.next;
	}
	
	return BPY_incr_ret(Py_None);
}

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Ipo)
struct PyMethodDef Ipo_methods[] = {
	MethodDef(addCurve),
	MethodDef(update),
	{NULL, NULL}
};


PyObject *initIpo(void)
{
	PyObject *mod, *dict, *d;

	mod= Py_InitModule(MODNAME(BLENDERMODULE) ".Ipo", Ipomodule_methods);
	dict = PyModule_GetDict(mod);

	// ipo block types
	d = ConstObject_New();
	PyDict_SetItemString(dict, "Types", d);
	g_ipoBlockTypes = d;

	insertConst(d, "Object", PyInt_FromLong(ID_OB));
	insertConst(d, "Material", PyInt_FromLong(ID_MA));
	insertConst(d, "Sequence", PyInt_FromLong(ID_SEQ));
	insertConst(d, "Curve", PyInt_FromLong(ID_CU));
	insertConst(d, "Key", PyInt_FromLong(ID_KE));
	insertConst(d, "World", PyInt_FromLong(ID_WO));
	insertConst(d, "Lamp", PyInt_FromLong(ID_LA));
	insertConst(d, "Camera", PyInt_FromLong(ID_CA));
	insertConst(d, "Sound", PyInt_FromLong(ID_SO));
	insertConst(d, "Action", PyInt_FromLong(ID_AC));

	// interpolation types:
	d = ConstObject_New();
	g_interpolationTypes = d;
	PyDict_SetItemString(dict, "InterpolationTypes", d);
	insertConst(d, "Constant", PyInt_FromLong(IPO_CONST));
	insertConst(d, "Linear", PyInt_FromLong(IPO_LIN));
	insertConst(d, "Bezier", PyInt_FromLong(IPO_BEZ));

	d = ConstObject_New();
	g_extrapolationTypes = d;
	PyDict_SetItemString(dict, "ExtrapolationTypes", d);
	insertConst(d, "Constant", PyInt_FromLong(IPO_HORIZ));
	insertConst(d, "Linear", PyInt_FromLong(IPO_DIR));
	insertConst(d, "Cyclic", PyInt_FromLong(IPO_CYCL));
	insertConst(d, "CyclicLinear", PyInt_FromLong(IPO_CYCLX));

	return mod;
}
