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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Ipocurve.h"
#include "BezTriple.c"
/*****************************************************************************/
/* Function:              M_IpoCurve_New                                          */
/* Python equivalent:     Blender.IpoCurve.New                                    */
/*****************************************************************************/
static PyObject *M_IpoCurve_New(PyObject *self, PyObject *args)
{
	return 0;
}


/*****************************************************************************/
/* Function:              Ipo_Init                                           */
/*****************************************************************************/
PyObject *IpoCurve_Init (void)
{
  PyObject  *submodule;

  IpoCurve_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.IpoCurve", M_IpoCurve_methods, M_IpoCurve_doc);

  return (submodule);
}

/*****************************************************************************/
/* Function:              M_IpoCurve_Get                                       */
/* Python equivalent:     Blender.IpoCurve.Get                                 */
/* Description:           Receives a string and returns the ipo data obj  */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_IpoCurve_Get(PyObject *self, PyObject *args)
{
	return 0;
}


/*****************************************************************************/
/* Python C_IpoCurve methods:                                                  */
/*****************************************************************************/

static PyObject *IpoCurve_setName(C_IpoCurve *self, PyObject *args)
{
	return 0;
}

static PyObject *IpoCurve_Recalc(C_IpoCurve *self)
{
	void testhandles_ipocurve(IpoCurve *icu);
	IpoCurve *icu = self->ipocurve;
	testhandles_ipocurve(icu); 
}

static PyObject* IpoCurve_getName (C_IpoCurve *self)
{
	char * nametab[24] = {"LocX","LocY","LocZ","dLocX","dLocY","dLocZ","RotX","RotY","RotZ","dRotX","dRotY","dRotZ","SizeX","SizeY","SizeZ","dSizeX","dSizeY","dSizeZ","Layer","Time","ColR","ColG","ColB","ColA"};

	//	printf("IpoCurve_getName %d\n",self->ipocurve->vartype);
	if (self->ipocurve->adrcode <=0 )
return PyString_FromString("Index too small");
	if (self->ipocurve->adrcode >= 25 )
return PyString_FromString("Index too big");

return PyString_FromString(nametab[self->ipocurve->adrcode-1]);

}



static void IpoCurveDeAlloc (C_IpoCurve *self)
{
  PyObject_DEL (self);
}

static PyObject* IpoCurve_getPoints (C_IpoCurve *self)
{	
struct BezTriple *bezt;
        PyObject* l = PyList_New(0);
				int i;
				for(i = 0;i<self->ipocurve->totvert;i++)
					{
						bezt = self->ipocurve->bezt + i;
  PyList_Append( l, BezTriple_CreatePyObject(bezt));
					}
  return l;
}



int IpoCurve_setPoints (C_IpoCurve *self, PyObject *value )
{	
struct BezTriple *bezt;
        PyObject* l = PyList_New(0);
				int i;
				for(i = 0;i<self->ipocurve->totvert;i++)
					{
						bezt = self->ipocurve->bezt + i;
  PyList_Append( l, BezTriple_CreatePyObject(bezt));
					}
  return l;
}


/*****************************************************************************/
/* Function:    IpoCurveGetAttr                                                   */
/* Description: This is a callback function for the C_IpoCurve type. It is        */
/*              the function that accesses C_IpoCurve "member variables" and      */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *IpoCurveGetAttr (C_IpoCurve *self, char *name)
{
if (strcmp (name, "bezierPoints") == 0)return IpoCurve_getPoints(self);
if (strcmp (name, "name") == 0)return IpoCurve_getName(self);
  return Py_FindMethod(C_IpoCurve_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    IpoCurveSetAttr                                                */
/* Description: This is a callback function for the C_IpoCurve type. It is the */
/*              function that sets IpoCurve Data attributes (member variables).*/
/*****************************************************************************/
static int IpoCurveSetAttr (C_IpoCurve *self, char *name, PyObject *value)
{
if (strcmp (name, "bezierPoints") == 0)return IpoCurve_setPoints(self,value);
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    IpoCurveRepr                                                      */
/* Description: This is a callback function for the C_IpoCurve type. It           */
/*              builds a meaninful string to represent ipo objects.          */
/*****************************************************************************/
static PyObject *IpoCurveRepr (C_IpoCurve *self)
{
	char s[1024];	
	sprintf(s,"IpoCurve %d %d %d %d %d %d %d %d \n",self->ipocurve->blocktype,self->ipocurve->adrcode,self->ipocurve->vartype,self->ipocurve->totvert,self->ipocurve->ipo,self->ipocurve->extrap,self->ipocurve->flag,self->ipocurve->rt);
  return PyString_FromString(s);
}

/* Three Python IpoCurve_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    IpoCurve_CreatePyObject                                           */
/* Description: This function will create a new C_IpoCurve from an existing       */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *IpoCurve_CreatePyObject (IpoCurve *ipo)
{
	C_IpoCurve *pyipo;

	pyipo = (C_IpoCurve *)PyObject_NEW (C_IpoCurve, &IpoCurve_Type);

	if (!pyipo)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create C_IpoCurve object");

	pyipo->ipocurve = ipo;

	return (PyObject *)pyipo;
}

/*****************************************************************************/
/* Function:    IpoCurve_CheckPyObject                                            */
/* Description: This function returns true when the given PyObject is of the */
/*              type IpoCurve. Otherwise it will return false.                    */
/*****************************************************************************/
int IpoCurve_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &IpoCurve_Type);
}

/*****************************************************************************/
/* Function:    IpoCurve_FromPyObject                                             */
/* Description: This function returns the Blender ipo from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
IpoCurve *IpoCurve_FromPyObject (PyObject *pyobj)
{
	return ((C_IpoCurve *)pyobj)->ipocurve;
}
