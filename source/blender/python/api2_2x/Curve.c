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

#include "Curve.h"


/*****************************************************************************/
/* Function:              M_Curve_New                                       */
/* Python equivalent:     Blender.Curve.New                                 */
/*****************************************************************************/
static PyObject *M_Curve_New(PyObject *self, PyObject *args)
{
  char buf[24];
  char*name=NULL ;
  BPy_Curve    *pycurve; /* for Curve Data object wrapper in Python */
  Curve      *blcurve = 0; /* for actual Curve Data we create in Blender */
  
  if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected string argument or no argument"));

  blcurve = add_curve(OB_CURVE); /* first create the Curve Data in Blender */
  
  if (blcurve == NULL) /* bail out if add_curve() failed */
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't create Curve Data in Blender"));

  /* return user count to zero because add_curve() inc'd it */
  blcurve->id.us = 0;
  /* create python wrapper obj */
  pycurve = (BPy_Curve *)PyObject_NEW(BPy_Curve, &Curve_Type);

  if (pycurve == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create Curve Data object"));

  pycurve->curve = blcurve; /* link Python curve wrapper to Blender Curve */
  if (name)
    {
      PyOS_snprintf(buf, sizeof(buf), "%s", name);
      rename_id(&blcurve->id, buf);
    }

  return (PyObject *)pycurve;
}

/*****************************************************************************/
/* Function:              M_Curve_Get                                       */
/* Python equivalent:     Blender.Curve.Get                                 */
/*****************************************************************************/
static PyObject *M_Curve_Get(PyObject *self, PyObject *args)
{
 
  char     *name = NULL;
  Curve   *curv_iter;
  BPy_Curve *wanted_curv;

  if (!PyArg_ParseTuple(args, "|s", &name))//expects nothing or a string
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
														"expected string argument"));
  if(name){//a name has been given
    /* Use the name to search for the curve requested */
    wanted_curv = NULL;
    curv_iter = G.main->curve.first;

    while ((curv_iter) && (wanted_curv == NULL)) {

      if (strcmp (name, curv_iter->id.name+2) == 0) {
	wanted_curv = (BPy_Curve *)PyObject_NEW(BPy_Curve, &Curve_Type);
	if (wanted_curv) wanted_curv->curve = curv_iter;
      }

      curv_iter = curv_iter->id.next;
    }

    if (wanted_curv == NULL) { /* Requested curve doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                    "Curve \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }
  

    return (PyObject*)wanted_curv;
  }//if(name)
  else{//no name has been given; return a list of all curves by name. 
    PyObject *curvlist;

    curv_iter = G.main->curve.first;
    curvlist = PyList_New (0);

    if (curvlist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
				       "couldn't create PyList"));

    while (curv_iter) {
BPy_Curve *found_cur=(BPy_Curve*)PyObject_NEW(BPy_Curve,&Curve_Type);
			found_cur->curve = curv_iter;
      PyList_Append (curvlist,  (PyObject *)found_cur);

      curv_iter = curv_iter->id.next;
    }

    return (curvlist);
  }//else
}

/*****************************************************************************/
/* Function:              Curve_Init                                         */
/*****************************************************************************/
PyObject *Curve_Init (void)
{
  PyObject  *submodule;

  Curve_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.Curve",M_Curve_methods, M_Curve_doc);
  return (submodule);
}

/*****************************************************************************/
/* Python BPy_Curve methods:                                                   */
/* gives access to                                                           */
/* name, pathlen totcol flag bevresol                                        */
/* resolu resolv width ext1 ext2                                             */ 
/* controlpoint loc rot size                                                 */
/* numpts                                                                    */
/*****************************************************************************/


static PyObject *Curve_getName(BPy_Curve *self)
{
  PyObject *attr = PyString_FromString(self->curve->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.name attribute"));
}

static PyObject *Curve_setName(BPy_Curve *self, PyObject *args)
{
  char*name;
  char buf[50];
  
  if (!PyArg_ParseTuple(args, "s", &(name)))  
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
														"expected string argument"));
  PyOS_snprintf(buf, sizeof(buf), "%s", name);
  rename_id(&self->curve->id, buf); /* proper way in Blender */

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Curve_getPathLen(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->pathlen);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.pathlen attribute"));
}


static PyObject *Curve_setPathLen(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->pathlen)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Curve_getTotcol(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->totcol);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.totcol attribute"));
}


static PyObject *Curve_setTotcol(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->totcol)))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Curve_getMode(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->flag);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.flag attribute"));
}


static PyObject *Curve_setMode(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->flag)))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Curve_getBevresol(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->bevresol);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.bevresol attribute"));
}


static PyObject *Curve_setBevresol(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->bevresol)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Curve_getResolu(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->resolu);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.resolu attribute"));
}


static PyObject *Curve_setResolu(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->resolu)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}



static PyObject *Curve_getResolv(BPy_Curve *self)
{
  PyObject *attr = PyInt_FromLong((long)self->curve->resolv);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.resolv attribute"));
}


static PyObject *Curve_setResolv(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "i", &(self->curve->resolv)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected int argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}



static PyObject *Curve_getWidth(BPy_Curve *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->curve->width);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.width attribute"));
}


static PyObject *Curve_setWidth(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "f", &(self->curve->width)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected float argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Curve_getExt1(BPy_Curve *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->curve->ext1);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.ext1 attribute"));
}


static PyObject *Curve_setExt1(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "f", &(self->curve->ext1)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected float argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}



static PyObject *Curve_getExt2(BPy_Curve *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->curve->ext2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
													"couldn't get Curve.ext2 attribute"));
}


static PyObject *Curve_setExt2(BPy_Curve *self, PyObject *args)
{

  if (!PyArg_ParseTuple(args, "f", &(self->curve->ext2)))
					return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																	"expected float argument"));
 
  Py_INCREF(Py_None);
  return Py_None;
}


/*
static PyObject *Curve_setControlPoint(BPy_Curve *self, PyObject *args)
{
  Nurb*ptrnurb = self->curve->nurb.first;
  int numcourbe,numpoint,i,j;
  float x,y,z,w;
  float bez[9];
  if (!ptrnurb){ Py_INCREF(Py_None);return Py_None;}

  if (ptrnurb->bp)
    if (!PyArg_ParseTuple(args, "iiffff", &numcourbe,&numpoint,&x,&y,&z,&w))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
								"expected int int float float float float arguments"));
  if (ptrnurb->bezt)
    if (!PyArg_ParseTuple(args, "iifffffffff", &numcourbe,&numpoint,
						bez,bez+1,bez+2,bez+3,bez+4,bez+5,bez+6,bez+7,bez+8))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
					"expected int int float float float float float float "
					"float float float arguments"));

  for(i = 0;i< numcourbe;i++)
    ptrnurb=ptrnurb->next;
  if (ptrnurb->bp)
    {
      ptrnurb->bp[numpoint].vec[0] = x;
      ptrnurb->bp[numpoint].vec[1] = y;
      ptrnurb->bp[numpoint].vec[2] = z;
      ptrnurb->bp[numpoint].vec[3] = w;
    }
  if (ptrnurb->bezt)
    {
      for(i = 0;i<3;i++)
	for(j = 0;j<3;j++)
	  ptrnurb->bezt[numpoint].vec[i][j] = bez[i*3+j];
    }
	
  Py_INCREF(Py_None);
  return Py_None;
}
*/


static PyObject *Curve_setControlPoint(BPy_Curve *self, PyObject *args)
{	PyObject *listargs=0;
  Nurb*ptrnurb = self->curve->nurb.first;
  int numcourbe,numpoint,i,j;
  if (!ptrnurb){ Py_INCREF(Py_None);return Py_None;}

  if (ptrnurb->bp)
    if (!PyArg_ParseTuple(args, "iiO", &numcourbe,&numpoint,&listargs))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
								"expected int int list arguments"));
  if (ptrnurb->bezt)
    if (!PyArg_ParseTuple(args, "iiO", &numcourbe,&numpoint,&listargs))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																		 "expected int int list arguments"));

  for(i = 0;i< numcourbe;i++)
    ptrnurb=ptrnurb->next;
  if (ptrnurb->bp)
		for(i = 0;i<4;i++)
			ptrnurb->bp[numpoint].vec[i] = PyFloat_AsDouble(PyList_GetItem(listargs,i));
  if (ptrnurb->bezt)
		for(i = 0;i<3;i++)
			for(j = 0;j<3;j++)
				ptrnurb->bezt[numpoint].vec[i][j] = PyFloat_AsDouble(PyList_GetItem(listargs,i*3+j));
	
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Curve_getControlPoint(BPy_Curve *self, PyObject *args)
{
  PyObject* liste = PyList_New(0);  /* return values */

  Nurb*ptrnurb;
  int i,j;
  /* input args: requested curve and point number on curve */
  int numcourbe, numpoint; 
    
  if (!PyArg_ParseTuple(args, "ii", &numcourbe,&numpoint))  
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected int int arguments"));
  if( (numcourbe < 0) || (numpoint < 0) )
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   " arguments must be non-negative"));

  /* if no nurbs in this curve obj */
  if (!self->curve->nurb.first) return liste;

  /* walk the list of nurbs to find requested numcourbe */
  ptrnurb = self->curve->nurb.first;
  for(i = 0; i < numcourbe; i++) 
    {
      ptrnurb=ptrnurb->next;
      if( !ptrnurb ) /* if zero, we ran just ran out of curves */
	return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
					"curve index out of range"));
    }
  
  /* check numpoint param against pntsu */
  if( numpoint >= ptrnurb->pntsu )
    return (EXPP_ReturnPyObjError( PyExc_AttributeError,
				"point index out of range"));

  if (ptrnurb->bp) /* if we are a nurb curve, you get 4 values */
    {
      for(i = 0; i< 4; i++)
	PyList_Append(liste,  PyFloat_FromDouble( ptrnurb->bp[numpoint].vec[i]));
    }
     
  if (ptrnurb->bezt) /* if we are a bezier, you get 9 values */
    {
      /* note to jacques:  I commented out the PyList_New() since we are appending the values below.  this never gets filled and just returns 9 null objs at the front of the list */
      /* liste = PyList_New(9); */ 
      for(i = 0; i< 3; i++)
	for(j = 0; j< 3; j++)
	  PyList_Append(liste,
			PyFloat_FromDouble( ptrnurb->bezt[numpoint].vec[i][j]));
    }

  return liste;
}



static PyObject *Curve_getLoc(BPy_Curve *self)
{
  int i;
  PyObject* liste = PyList_New(3);
  for(i = 0;i< 3;i++)
    PyList_SetItem(liste, i, PyFloat_FromDouble( self->curve->loc[i]));
  return liste;
}

static PyObject *Curve_setLoc(BPy_Curve *self, PyObject *args)
{
	PyObject *listargs=0;
	int i;
  if (!PyArg_ParseTuple(args, "O", &listargs)) 
    return EXPP_ReturnPyObjError(PyExc_AttributeError,"expected list argument"); 
if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected a list"));
	for(i = 0;i<3;i++){
		PyObject * xx = PyList_GetItem(listargs,i);
		self->curve->loc[i] =PyFloat_AsDouble(xx);
	}
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Curve_getRot(BPy_Curve *self)
{

  int i;
  PyObject* liste = PyList_New(3);
  for(i = 0;i< 3;i++)
    PyList_SetItem(liste, i, PyFloat_FromDouble( self->curve->rot[i]));
  return liste;

}

static PyObject *Curve_setRot(BPy_Curve *self, PyObject *args)
{
	PyObject *listargs=0;
	int i;
  if (!PyArg_ParseTuple(args, "O", &listargs)) 
    return EXPP_ReturnPyObjError(PyExc_AttributeError,"expected list argument"); 
if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected a list"));
	for(i = 0;i<3;i++){
		PyObject * xx = PyList_GetItem(listargs,i);
		self->curve->rot[i] =PyFloat_AsDouble(xx);
	}
  Py_INCREF(Py_None);
  return Py_None;

}
static PyObject *Curve_getSize(BPy_Curve *self)
{
  int i;
  PyObject* liste = PyList_New(3);
  for(i = 0;i< 3;i++)
    PyList_SetItem(liste, i, PyFloat_FromDouble( self->curve->size[i]));
  return liste;

}

static PyObject *Curve_setSize(BPy_Curve *self, PyObject *args)
{
	PyObject *listargs=0;
	int i;
  if (!PyArg_ParseTuple(args, "O", &listargs)) 
    return EXPP_ReturnPyObjError(PyExc_AttributeError,"expected list argument"); 
if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected a list"));
	for(i = 0;i<3;i++){
		PyObject * xx = PyList_GetItem(listargs,i);
		self->curve->size[i] =PyFloat_AsDouble(xx);
	}
  Py_INCREF(Py_None);
  return Py_None;
}

/* sds */
/*
 * Count the number of splines in a Curve Object
 * int getNumCurves()
 */

static PyObject *Curve_getNumCurves(BPy_Curve *self)
{
  Nurb *ptrnurb;
  PyObject* ret_val;
  int num_curves = 0; /* start with no splines */

  /* get curve */
  ptrnurb = self->curve->nurb.first;
  if( ptrnurb ) /* we have some nurbs in this curve */
    {
      while( 1 )
	{
	  ++num_curves;
	  ptrnurb = ptrnurb->next;
	  if( !ptrnurb ) /* no more curves */
	    break;
	}
    }

  ret_val = PyInt_FromLong((long) num_curves);

  if (ret_val) return ret_val;

  /* oops! */
  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get number of curves"));
}


/*
 * count the number of points in a give spline
 * int getNumPoints( curve_num=0 )
 *
 */

static PyObject *Curve_getNumPoints(BPy_Curve *self, PyObject *args)
{
  Nurb *ptrnurb;
  PyObject* ret_val;
  int curve_num = 0; /* default spline number */
  int i;

  /* parse input arg */
  if( !PyArg_ParseTuple( args, "|i", &curve_num ))
    return( EXPP_ReturnPyObjError( PyExc_AttributeError,
				      "expected int argument"));

  /* check arg - must be non-negative */
  if( curve_num < 0 )
     return( EXPP_ReturnPyObjError( PyExc_AttributeError,
				      "argument must be non-negative"));
     
				      
  /* walk the list of curves looking for our curve */
  ptrnurb = self->curve->nurb.first;
  if( !ptrnurb ) /* no splines in this Curve */
    {
      return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
				      "no splines in this Curve"));
    }
      
  for( i = 0; i < curve_num; i++ )
    {
      ptrnurb = ptrnurb->next;
      if( !ptrnurb ) /* if zero, we ran just ran out of curves */
	return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
					"curve index out of range"));
    }
  
  /* pntsu is the number of points in curve */
  ret_val = PyInt_FromLong((long) ptrnurb->pntsu);

  if (ret_val) return ret_val;

  /* oops! */
  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get number of points for curve"));
}

/*
 * Test whether a given spline of a Curve is a nurb
 *  as opposed to a bezier
 * int isNurb( curve_num=0 )
 */

static PyObject *Curve_isNurb( BPy_Curve *self, PyObject *args )
{
  int curve_num=0; /* default value */
  int is_nurb;
  Nurb *ptrnurb;
  PyObject* ret_val;
  int i;

  /* parse and check input args */
  if( !PyArg_ParseTuple( args, "|i", &curve_num ))
    {
      return( EXPP_ReturnPyObjError( PyExc_AttributeError,
				      "expected int argument"));
    }
  if( curve_num < 0 )
    {
      return( EXPP_ReturnPyObjError( PyExc_AttributeError,
				     "curve number must be non-negative"));
    }

  ptrnurb = self->curve->nurb.first;

  if( !ptrnurb ) /* no splines in this curve */
    return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
				    "no splines in this Curve"));

  for( i = 0; i < curve_num; i++ )
    {
      ptrnurb = ptrnurb->next;
      if( !ptrnurb ) /* if zero, we ran just ran out of curves */
	return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
					"curve index out of range"));
    }
      
  /* right now, there are only two curve types, nurb and bezier. */
  is_nurb = ptrnurb->bp ? 1 : 0;
  
  ret_val = PyInt_FromLong( (long) is_nurb );
  if(  ret_val )
    return ret_val;

  /* oops */
  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get curve type"));
}

      



/*****************************************************************************/
/* Function:    CurveDeAlloc                                                 */
/* Description: This is a callback function for the BPy_Curve type. It is      */
/*              the destructor function.                                     */
/*****************************************************************************/
static void CurveDeAlloc (BPy_Curve *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    CurveGetAttr                                                 */
/* Description: This is a callback function for the BPy_Curve type. It is      */
/*              the function that accesses BPy_Curve "member variables" and    */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *CurveGetAttr (BPy_Curve *self, char *name)//getattr
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->curve->id.name+2);
  if (strcmp(name, "pathlen") == 0)
    attr = PyInt_FromLong(self->curve->pathlen);
  if (strcmp(name, "totcol") == 0)
    attr = PyInt_FromLong(self->curve->totcol);
  if (strcmp(name, "flag") == 0)
    attr = PyInt_FromLong(self->curve->flag);
  if (strcmp(name, "bevresol") == 0)
    attr = PyInt_FromLong(self->curve->bevresol);
  if (strcmp(name, "resolu") == 0)
    attr = PyInt_FromLong(self->curve->resolu);
  if (strcmp(name, "resolv") == 0)
    attr = PyInt_FromLong(self->curve->resolv);
  if (strcmp(name, "width") == 0)
    attr = PyFloat_FromDouble(self->curve->width);
  if (strcmp(name, "ext1") == 0)
    attr = PyFloat_FromDouble(self->curve->ext1);
  if (strcmp(name, "ext2") == 0)
    attr = PyFloat_FromDouble(self->curve->ext2);
  if (strcmp(name, "loc") == 0)
    return Curve_getLoc(self);
  if (strcmp(name, "rot") == 0)
    return Curve_getRot(self);
  if (strcmp(name, "size") == 0)
    return Curve_getSize(self);
#if 0
    if (strcmp(name, "numpts") == 0)
    return Curve_getNumPoints(self);
#endif





  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create PyObject"));

  if (attr != Py_None) return attr; /* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod(BPy_Curve_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    CurveSetAttr                                                 */
/* Description: This is a callback function for the BPy_Curve type. It is the  */
/*              function that sets Curve Data attributes (member variables). */
/*****************************************************************************/
static int CurveSetAttr (BPy_Curve *self, char *name, PyObject *value)
{ PyObject *valtuple; 
  PyObject *error = NULL;
  valtuple = Py_BuildValue("(O)", value);
  //resolu resolv width ext1 ext2  
  if (!valtuple) 
    return EXPP_ReturnIntError(PyExc_MemoryError,
                         "CurveSetAttr: couldn't create PyTuple");

  if (strcmp (name, "name") == 0)
    error = Curve_setName (self, valtuple);
  else if (strcmp (name, "pathlen") == 0)
    error = Curve_setPathLen(self, valtuple);
  else if (strcmp (name, "resolu") == 0)
    error = Curve_setResolu (self, valtuple);
  else if (strcmp (name, "resolv") == 0)
    error = Curve_setResolv (self, valtuple);
  else if (strcmp (name, "width") == 0)
    error = Curve_setWidth (self, valtuple);
  else if (strcmp (name, "ext1") == 0)
    error = Curve_setExt1 (self, valtuple);
  else if (strcmp (name, "ext2") == 0)
    error = Curve_setExt2 (self, valtuple);
  else if (strcmp (name, "loc") == 0)
    error = Curve_setLoc (self, valtuple);
  else if (strcmp (name, "rot") == 0)
    error = Curve_setRot (self, valtuple);
  else if (strcmp (name, "size") == 0)
    error = Curve_setSize (self, valtuple);

  else { /* Error */
    Py_DECREF(valtuple);

    if ((strcmp (name, "Types") == 0) || 
        (strcmp (name, "Modes") == 0))   
      return (EXPP_ReturnIntError (PyExc_AttributeError,
                   "constant dictionary -- cannot be changed"));

    else 
      return (EXPP_ReturnIntError (PyExc_KeyError,
                   "attribute not found"));
  }

  Py_DECREF(valtuple);

  if (error != Py_None) return -1;
  Py_DECREF(Py_None);
  return 0; 
}


/*****************************************************************************/
/* Function:    CurveRepr                                                    */
/* Description: This is a callback function for the BPy_Curve type. It         */
/*              builds a meaninful string to represent curve objects.        */
/*****************************************************************************/
static PyObject *CurveRepr (BPy_Curve *self) //used by 'repr'
{
 
  return PyString_FromFormat("[Curve \"%s\"]", self->curve->id.name+2);
}

PyObject* Curve_CreatePyObject (struct Curve *curve)
{
 BPy_Curve    * blen_object;

    blen_object = (BPy_Curve*)PyObject_NEW (BPy_Curve, &Curve_Type);

    if (blen_object == NULL)
    {
        return (NULL);
    }
    blen_object->curve = curve;
    return ((PyObject*)blen_object);

}

int Curve_CheckPyObject (PyObject *py_obj)
{
return (py_obj->ob_type == &Curve_Type);
}


struct Curve* Curve_FromPyObject (PyObject *py_obj)
{
 BPy_Curve    * blen_obj;

    blen_obj = (BPy_Curve*)py_obj;
    return (blen_obj->curve);

}

