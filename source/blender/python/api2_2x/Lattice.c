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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Lattice.h"

//***************************************************************************
// Function:							Lattice_CreatePyObject																
//***************************************************************************
PyObject *Lattice_CreatePyObject (Lattice *lt)
{
	BPy_Lattice *pyLat;

	pyLat = (BPy_Lattice *)PyObject_NEW (BPy_Lattice, &Lattice_Type);

	if (!pyLat)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						 "couldn't create BPy_Lattice PyObject");

	pyLat->Lattice = lt;

	return (PyObject *)pyLat;
}

//***************************************************************************
// Function:							Lattice_FromPyObject																
//***************************************************************************

Lattice *Lattice_FromPyObject (PyObject *pyobj)
{
		return ((BPy_Lattice *)pyobj)->Lattice;
}

//***************************************************************************
// Function:							Lattice_CheckPyObject																 
//***************************************************************************
int Lattice_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &Lattice_Type);
}
	
//***************************************************************************
// Function:							M_Lattice_New																					
// Python equivalent:			Blender.Lattice.New																		
//***************************************************************************
static PyObject *M_Lattice_New(PyObject *self, PyObject *args)
{
	char *name = NULL;
	char buf[21];
	Lattice  * bl_Lattice; // blender Lattice object 
	PyObject * py_Lattice; // python wrapper 

	if (!PyArg_ParseTuple(args, "|s", &name))
				return EXPP_ReturnPyObjError (PyExc_AttributeError,
					"expected string and int arguments (or nothing)");

	bl_Lattice = add_lattice();
	bl_Lattice->id.us = 0;

	if (bl_Lattice)
		py_Lattice = Lattice_CreatePyObject (bl_Lattice);
	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
										"couldn't create Lattice Object in Blender");
	if (!py_Lattice)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
										"couldn't create Lattice Object wrapper");

	if (name) {
		PyOS_snprintf(buf, sizeof(buf), "%s", name);
		rename_id(&bl_Lattice->id, buf);
	}

	return py_Lattice;
}

//***************************************************************************
// Function:							M_Lattice_Get																					
// Python equivalent:			Blender.Lattice.Get																	
//***************************************************************************
static PyObject *M_Lattice_Get(PyObject *self, PyObject *args)
{
  char *name = NULL;
  Lattice *lat_iter;

  if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument (or nothing)"));

  lat_iter = G.main->latt.first;

  if (name) { /* (name) - Search Lattice by name */

    PyObject *wanted_lat = NULL;

    while ((lat_iter) && (wanted_lat == NULL)) {
      if (strcmp (name, lat_iter->id.name+2) == 0) {
        wanted_lat = Lattice_CreatePyObject (lat_iter);
      }

      lat_iter = lat_iter->id.next;
    }

    if (wanted_lat == NULL) { /* Requested Lattice doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                      "Lattice \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return wanted_lat;
  }

  else { /* () - return a list of all Lattices in the scene */
    int index = 0;
    PyObject *latlist, *pyobj;

    latlist = PyList_New (BLI_countlist (&(G.main->latt)));

    if (latlist == NULL)
      return (EXPP_ReturnPyObjError (PyExc_MemoryError,
              "couldn't create PyList"));

    while (lat_iter) {
      pyobj = Lattice_CreatePyObject(lat_iter);

      if (!pyobj)
        return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                   "couldn't create PyString"));

      PyList_SET_ITEM (latlist, index, pyobj);

      lat_iter = lat_iter->id.next;
      index++;
    }

    return (latlist);
  }
}
//***************************************************************************
// Function:							Lattice_Init																					
//***************************************************************************
PyObject *Lattice_Init (void)
{
	PyObject *mod= Py_InitModule3("Blender.Lattice", M_Lattice_methods, M_Lattice_doc);
	PyObject *dict= PyModule_GetDict(mod);

	Lattice_Type.ob_type = &PyType_Type;

	//Module dictionary
	#define EXPP_ADDCONST(x) PyDict_SetItemString(dict, #x, PyInt_FromLong(LT_##x))
	EXPP_ADDCONST(GRID);
	EXPP_ADDCONST(OUTSIDE);

	#undef EXPP_ADDCONST
	#define EXPP_ADDCONST(x) PyDict_SetItemString(dict, #x, PyInt_FromLong(KEY_##x))
	EXPP_ADDCONST(LINEAR);
	EXPP_ADDCONST(CARDINAL);
	EXPP_ADDCONST(BSPLINE);

	return(mod);
}

//***************************************************************************
// Python BPy_Lattice methods:																									
//***************************************************************************
static PyObject *Lattice_getName(BPy_Lattice *self)
{
	PyObject *attr = PyString_FromString(self->Lattice->id.name+2);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"couldn't get Lattice.name attribute");
}

static PyObject *Lattice_setName(BPy_Lattice *self, PyObject *args)
{
	char *name;
	char buf[21];

	if (!PyArg_ParseTuple(args, "s", &name))
	return (EXPP_ReturnPyObjError (PyExc_TypeError,
		"expected string argument"));

	PyOS_snprintf(buf, sizeof(buf), "%s", name);

	rename_id(&self->Lattice->id, buf);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lattice_setPartitions(BPy_Lattice *self, PyObject *args)
{
	int x = 0;
	int y = 0;
	int z = 0;
	Lattice  * bl_Lattice;

	if (!PyArg_ParseTuple(args, "iii", &x, &y, &z))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected int,int,int argument"));

	bl_Lattice = self->Lattice;

	if(x <2 || y < 2 || z < 2)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"partition values must be 2 or greater"));

	bl_Lattice->pntsu = (short)x;
	bl_Lattice->pntsv = (short)y;
	bl_Lattice->pntsw = (short)z;
	resizelattice(bl_Lattice);

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *Lattice_getPartitions(BPy_Lattice *self, PyObject *args)
{
	Lattice  * bl_Lattice;
	bl_Lattice = self->Lattice;

	return Py_BuildValue("[i,i,i]", (int)bl_Lattice->pntsu,
									(int)bl_Lattice->pntsv,
									(int)bl_Lattice->pntsw);
}
		
static PyObject *Lattice_getKeyTypes(BPy_Lattice *self, PyObject *args)
{
	Lattice  * bl_Lattice;
	char *linear = "linear";
	char *cardinal = "cardinal";
	char *bspline = "bspline";
	char *s_x = NULL, *s_y = NULL, *s_z = NULL;

	bl_Lattice = self->Lattice;

	if ((bl_Lattice->typeu) == KEY_LINEAR)
		s_x = linear;
	else if ((bl_Lattice->typeu) == KEY_CARDINAL)
		s_x = cardinal;
	else if ((bl_Lattice->typeu) == KEY_BSPLINE)
		s_x = bspline;
	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			 "bad key type...");

	if ((bl_Lattice->typev) == KEY_LINEAR)
		s_y = linear;
	else if ((bl_Lattice->typev) == KEY_CARDINAL)
		s_y = cardinal;
	else if ((bl_Lattice->typev) == KEY_BSPLINE)
		s_z = bspline;
	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			 "bad key type...");

	if ((bl_Lattice->typew) == KEY_LINEAR)
		s_z = linear;
	else if ((bl_Lattice->typew) == KEY_CARDINAL)
		s_z = cardinal;
	else if ((bl_Lattice->typew) == KEY_BSPLINE)
		s_z = bspline;
	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			 "bad key type...");

	/* we made sure no s_[xyz] is NULL */
	return Py_BuildValue("[s,s,s]", s_x, s_y, s_z);
}

static PyObject *Lattice_setKeyTypes(BPy_Lattice *self, PyObject *args)
{
	int x;
	int y;
	int z;
	Lattice  * bl_Lattice;

	if (!PyArg_ParseTuple(args, "iii", &x, &y, &z))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int,int,int argument"));

	bl_Lattice = self->Lattice;
	
	if (x == KEY_LINEAR)
		bl_Lattice->typeu = KEY_LINEAR;
	else if (x == KEY_CARDINAL)
		bl_Lattice->typeu = KEY_CARDINAL;
	else if (x == KEY_BSPLINE)
		bl_Lattice->typeu = KEY_BSPLINE;
	else
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "type must be LINEAR, CARDINAL OR BSPLINE");

	if (y == KEY_LINEAR)
		bl_Lattice->typev = KEY_LINEAR;
	else if (y == KEY_CARDINAL)
		bl_Lattice->typev = KEY_CARDINAL;
	else if (y == KEY_BSPLINE)
		bl_Lattice->typev = KEY_BSPLINE;
	else
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "type must be LINEAR, CARDINAL OR BSPLINE");

	if (z == KEY_LINEAR)
		bl_Lattice->typew = KEY_LINEAR;
	else if (z == KEY_CARDINAL)
		bl_Lattice->typew = KEY_CARDINAL;
	else if (z == KEY_BSPLINE)
		bl_Lattice->typew = KEY_BSPLINE;
	else
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "type must be LINEAR, CARDINAL OR BSPLINE");

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lattice_setMode(BPy_Lattice *self, PyObject *args)
{
	short type;
	Lattice  * bl_Lattice;
	bl_Lattice = self->Lattice;

	if (!PyArg_ParseTuple(args, "h", &type))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument"));

	if (type == LT_GRID)
		bl_Lattice->flag = LT_GRID;
	else if (type == LT_OUTSIDE)
	{
		bl_Lattice->flag = LT_OUTSIDE + LT_GRID;
		outside_lattice(bl_Lattice);
	}
	 else
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "type must be either GRID or OUTSIDE");

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lattice_getMode(BPy_Lattice *self, PyObject *args)
{
	char type[24];
	Lattice  * bl_Lattice;
	bl_Lattice = self->Lattice;

	if (bl_Lattice->flag & LT_GRID)
	sprintf(type, "Grid");
	else if (bl_Lattice->flag & LT_OUTSIDE)
	sprintf(type, "Outside");
	else
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "bad mode type...");
 
	return Py_BuildValue("s", type);
}

static PyObject *Lattice_setPoint(BPy_Lattice *self, PyObject *args)
{
	BPoint *bp, *bpoint;
	short size;
	Lattice  * bl_Lattice;
	int index, x;
	float tempInt;
	PyObject * listObject;

	if (!PyArg_ParseTuple(args, "iO!", &index, &PyList_Type, &listObject))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int & list argument"));

	if(!PyList_Check(listObject))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"2nd parameter should be a python list"));

	if (!(PyList_Size(listObject) == 3))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"Please pass 3 parameters in the list [x,y,z]"));

	//init
	bp= 0;
	bl_Lattice = self->Lattice;

	//get bpoints
	bp= bl_Lattice->def;

	if(bp == 0)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"no lattice points!"));
	
	//calculate size of lattice
	size= bl_Lattice->pntsu*bl_Lattice->pntsv*bl_Lattice->pntsw;

	if (index < 0 || index > size)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"index outside of lattice size!"));
	
	//get the bpoint
	while (index)
	{
		index--;
		bp++;
	}
	bpoint = bp;

	for (x = 0; x < PyList_Size(listObject); x++) {
		if (!(PyArg_Parse((PyList_GetItem(listObject, x)), "f", &tempInt)))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
											"python list integer not parseable");
		bpoint->vec[x] = tempInt;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lattice_getPoint(BPy_Lattice *self, PyObject *args)
{
	BPoint *bp, *bpoint;
	short size;
	Lattice  * bl_Lattice;
	int index;

	if (!PyArg_ParseTuple(args, "i", &index))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument"));

	//init
	bp= 0;
	bl_Lattice = self->Lattice;

	//get bpoints
	bp= bl_Lattice->def;

	if(bp == 0)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"no lattice points!"));
	
	//calculate size of lattice
	size= bl_Lattice->pntsu*bl_Lattice->pntsv*bl_Lattice->pntsw;

	if (index < 0 || index > size)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"index outside of lattice size!"));
	
	//get the bpoint
	while (index)
	{
		index--;
		bp++;
	}
	bpoint = bp;

	if(bpoint == 0)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"bpoint does not exist"));

	return Py_BuildValue("[f,f,f]", bp->vec[0] ,bp->vec[1] ,bp->vec[2]);
}

//This function will not do anything if there are no children
static PyObject *Lattice_applyDeform(BPy_Lattice *self)
{
	Object* ob;
	Base *base;
	Object *par;

	if (!Lattice_IsLinkedToObject(self))
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Lattice must be linked to an object to apply it's deformation!"));

	//deform children
	base= FIRSTBASE;
	while(base) {
		if( (par= base->object->parent) ) {
			if(par->type==OB_LATTICE) {		
					object_deform(base->object);			
				}
			}			
		base= base->next;
	}

	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lattice_insertKey(BPy_Lattice *self, PyObject *args)
{
	Lattice *lt;
	int frame, oldfra;

	if (!PyArg_ParseTuple(args, "i", &frame))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument"));

	lt = self->Lattice;

	//set the current frame
	if (frame > 0) {
		oldfra = G.scene->r.cfra;
		G.scene->r.cfra = frame;
	}
	else
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"frame value has to be greater than 0"));

	//insert a keybock for the lattice
	insert_lattkey(lt);

	G.scene->r.cfra = oldfra;

	Py_INCREF(Py_None);
	return Py_None;
}

//***************************************************************************
// Function:		Lattice_dealloc																									
// Description: This is a callback function for the BPy_Lattice type. It is			
//							the destructor function.																		 
//***************************************************************************
static void Lattice_dealloc (BPy_Lattice *self)
{
	PyObject_DEL (self);
}

//***************************************************************************
// Function:		Lattice_getAttr																									
// Description: This is a callback function for the BPy_Lattice type. It is			
//							the function that accesses BPy_Lattice member variables and			
//							methods.																										 
//***************************************************************************
static PyObject *Lattice_getAttr (BPy_Lattice *self, char *name)
{
	PyObject *attr = Py_None;

	if (!self->Lattice || !Lattice_InLatList(self))
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			 "Lattice was already deleted!");

	if (strcmp(name, "name") == 0)
		attr = PyString_FromString(self->Lattice->id.name+2);
	else if (strcmp(name, "width") == 0)
		attr = Py_BuildValue("i",self->Lattice->pntsu);
	else if (strcmp(name, "height") == 0)
		attr = Py_BuildValue("i",self->Lattice->pntsv);
	else if (strcmp(name, "depth") == 0)
		attr = Py_BuildValue("i",self->Lattice->pntsw);
	else if (strcmp(name, "widthType") == 0)
	{
		if(self->Lattice->typeu == 0)
			attr = Py_BuildValue("s","Linear");
		else if (self->Lattice->typeu == 1)
			attr = Py_BuildValue("s","Cardinal");
		else if (self->Lattice->typeu == 2)
			attr = Py_BuildValue("s","Bspline");
		else
			return EXPP_ReturnPyObjError (PyExc_ValueError,
				"bad widthType...");
	}
	else if (strcmp(name, "heightType") == 0)
	{
		if(self->Lattice->typev == 0)
			attr = Py_BuildValue("s","Linear");
		else if (self->Lattice->typev== 1)
			attr = Py_BuildValue("s","Cardinal");
		else if (self->Lattice->typev == 2)
			attr = Py_BuildValue("s","Bspline");
		else
			return EXPP_ReturnPyObjError (PyExc_ValueError,
				"bad widthType...");
	}
	else if (strcmp(name, "depthType") == 0)
	{
		if(self->Lattice->typew == 0)
			attr = Py_BuildValue("s","Linear");
		else if (self->Lattice->typew == 1)
			attr = Py_BuildValue("s","Cardinal");
		else if (self->Lattice->typew == 2)
			attr = Py_BuildValue("s","Bspline");
		else
			return EXPP_ReturnPyObjError (PyExc_ValueError,
				"bad widthType...");
	}
	else if (strcmp(name, "mode") == 0)
	{
		if(self->Lattice->flag == 1)
			attr = Py_BuildValue("s","Grid");
		else if (self->Lattice->flag == 3)
			attr = Py_BuildValue("s","Outside");
		else
			return EXPP_ReturnPyObjError (PyExc_ValueError,
				"bad mode...");
	}
	else if (strcmp(name, "latSize") == 0)
	{
		attr = Py_BuildValue("i",self->Lattice->pntsu * 
								 self->Lattice->pntsv *
								 self->Lattice->pntsw);
	}
	else if (strcmp(name, "__members__") == 0)
		attr = Py_BuildValue("[s,s,s,s,s,s,s,s,s]", "name","width","height","depth",
	"widthType","heightType","depthType","mode","latSize");

	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create PyObject"));

	if (attr != Py_None) return attr; // attribute found, return its value 

	// not an attribute, search the methods table 
	return Py_FindMethod(BPy_Lattice_methods, (PyObject *)self, name);
}

//***************************************************************************
// Function:		Lattice_setAttr																									
// Description: This is a callback function for the BPy_Lattice type. It is the 
//							function that changes Lattice Data members values. If this			
//							data is linked to a Blender Lattice, it also gets updated.			
//***************************************************************************
static int Lattice_setAttr (BPy_Lattice *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

	if (!self->Lattice || !Lattice_InLatList(self))
		return EXPP_ReturnIntError (PyExc_RuntimeError,
			 "Lattice was already deleted!");

	valtuple = Py_BuildValue("(O)", value);// the set* functions expect a tuple 

	if (!valtuple)
		return EXPP_ReturnIntError(PyExc_MemoryError,
									"LatticeSetAttr: couldn't create PyTuple");

	if (strcmp (name, "name") == 0)
		error = Lattice_setName (self, valtuple);
	else { // Error: no such member in the Lattice Data structure 
		Py_DECREF(value);
		Py_DECREF(valtuple);
		return (EXPP_ReturnIntError (PyExc_KeyError,
						"attribute not found or immutable"));
	}
	Py_DECREF(valtuple);

	if (error != Py_None) return -1;

	return 0; // normal exit 
}

//***************************************************************************
// Function:		Lattice_repr																										
// Description: This is a callback function for the BPy_Lattice type. It				
//							builds a meaninful string to represent Lattice objects.					
//***************************************************************************
static PyObject *Lattice_repr (BPy_Lattice *self)
{
	if (self->Lattice && Lattice_InLatList(self))
		return PyString_FromFormat("[Lattice \"%s\"]", self->Lattice->id.name+2);
	else
		return PyString_FromString("[Lattice <deleted>]");
}

//***************************************************************************
// Function:		Internal Lattice functions	
//***************************************************************************
// Internal function to confirm if a Lattice wasn't unlinked from main.
static int Lattice_InLatList(BPy_Lattice *self)
{
	Lattice *lat_iter = G.main->latt.first;

	while (lat_iter) {
		if (self->Lattice == lat_iter) return 1; // ok, still linked 

		lat_iter = lat_iter->id.next;
	}
	// uh-oh, it was already deleted 
	self->Lattice = NULL; // so we invalidate the pointer 
	return 0;
}

// Internal function to confirm if a Lattice has an object it's linked to.
static int Lattice_IsLinkedToObject(BPy_Lattice *self)
{
	//check to see if lattice is linked to an object
	Object * ob= G.main->object.first;
	while(ob) {
		if(ob->type==OB_LATTICE){
			if(self->Lattice == ob->data){
				return 1;
			}
		}			
		ob = ob->id.next;
	}
	return 0;
}