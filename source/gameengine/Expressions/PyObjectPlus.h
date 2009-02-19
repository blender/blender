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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef NO_EXP_PYTHON_EMBEDDING

#ifndef _adr_py_lib_h_				// only process once,
#define _adr_py_lib_h_				// even if multiply included

#ifndef __cplusplus				// c++ only
#error Must be compiled with C++
#endif

#include "KX_Python.h"
#include "STR_String.h"

/*------------------------------
 * Python defines
------------------------------*/

/*
   Py_RETURN_NONE
   Python 2.4 macro.
   defined here until we switch to 2.4
   also in api2_2x/gen_utils.h 
*/
#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE  return Py_BuildValue("O", Py_None)
#endif
#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE  return PyBool_FromLong(0)
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE  return PyBool_FromLong(1)
#endif

/*  for pre Py 2.5 */
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#define PY_METHODCHAR char *
#else
/* Py 2.5 and later */
#define  intargfunc  ssizeargfunc
#define intintargfunc  ssizessizeargfunc
#define PY_METHODCHAR const char *
#endif

								// some basic python macros
#define Py_Return { Py_INCREF(Py_None); return Py_None;}

static inline void Py_Fatal(const char *M) {
	//cout << M << endl; 
	exit(-1);
};

								// This must be the first line of each 
								// PyC++ class
#define Py_Header \
 public: \
  static PyTypeObject   Type; \
  static PyMethodDef    Methods[]; \
  static PyAttributeDef Attributes[]; \
  static PyParentObject Parents[]; \
  virtual PyTypeObject *GetType(void) {return &Type;}; \
  virtual PyParentObject *GetParents(void) {return Parents;}


								// This defines the _getattr_up macro
								// which allows attribute and method calls
								// to be properly passed up the hierarchy.
#define _getattr_up(Parent) \
  PyObject *rvalue = NULL; \
  if (!strcmp(attr, "__methods__")) { \
    PyObject *_attr_string = NULL; \
    PyMethodDef *meth = Methods; \
    rvalue = Parent::_getattr(attr); \
    if (rvalue==NULL) { \
    	PyErr_Clear(); \
    	rvalue = PyList_New(0); \
    } \
    if (meth) { \
      for (; meth->ml_name != NULL; meth++) { \
        _attr_string = PyString_FromString(meth->ml_name); \
		PyList_Append(rvalue, _attr_string); \
		Py_DECREF(_attr_string); \
	  } \
	} \
  } else { \
    rvalue = Py_FindMethod(Methods, this, attr); \
    if (rvalue == NULL) { \
      PyErr_Clear(); \
      rvalue = Parent::_getattr(attr); \
    } \
  } \
  return rvalue; \


/**
 * These macros are helpfull when embedding Python routines. The second
 * macro is one that also requires a documentation string
 */
#define KX_PYMETHOD(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		return ((class_name*) self)->Py##method_name(self, args, kwds);		\
	}; \

#define KX_PYMETHOD_VARARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args) { \
		return ((class_name*) self)->Py##method_name(self, args);		\
	}; \

#define KX_PYMETHOD_NOARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self); \
	static PyObject* sPy##method_name( PyObject* self) { \
		return ((class_name*) self)->Py##method_name(self);		\
	}; \
	
#define KX_PYMETHOD_O(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* value); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* value) { \
		return ((class_name*) self)->Py##method_name(self, value);		\
	}; \

#define KX_PYMETHOD_DOC(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		return ((class_name*) self)->Py##method_name(self, args, kwds);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_VARARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args) { \
		return ((class_name*) self)->Py##method_name(self, args);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_O(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* value); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* value) { \
		return ((class_name*) self)->Py##method_name(self, value);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_NOARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self); \
	static PyObject* sPy##method_name( PyObject* self) { \
		return ((class_name*) self)->Py##method_name(self);		\
	}; \
    static const char method_name##_doc[]; \


/* The line above should remain empty */
/**
 * Method table macro (with doc)
 */
#define KX_PYMETHODTABLE(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_VARARGS, (PY_METHODCHAR)class_name::method_name##_doc}

#define KX_PYMETHODTABLE_O(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_O, (PY_METHODCHAR)class_name::method_name##_doc}

#define KX_PYMETHODTABLE_NOARGS(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_NOARGS, (PY_METHODCHAR)class_name::method_name##_doc}

/**
 * Function implementation macro
 */
#define KX_PYMETHODDEF_DOC(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject*, PyObject* args, PyObject*)

#define KX_PYMETHODDEF_DOC_VARARGS(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject*, PyObject* args)

#define KX_PYMETHODDEF_DOC_O(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject*, PyObject* value)

#define KX_PYMETHODDEF_DOC_NOARGS(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject*)

/**
 * Attribute management
 */
enum KX_PYATTRIBUTE_TYPE {
	KX_PYATTRIBUTE_TYPE_BOOL,
	KX_PYATTRIBUTE_TYPE_ENUM,
	KX_PYATTRIBUTE_TYPE_SHORT,
	KX_PYATTRIBUTE_TYPE_INT,
	KX_PYATTRIBUTE_TYPE_FLOAT,
	KX_PYATTRIBUTE_TYPE_STRING,
	KX_PYATTRIBUTE_TYPE_DUMMY,
};

enum KX_PYATTRIBUTE_ACCESS {
	KX_PYATTRIBUTE_RW,
	KX_PYATTRIBUTE_RO
};

struct KX_PYATTRIBUTE_DEF;
typedef int (*KX_PYATTRIBUTE_FUNCTION)(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);

typedef struct KX_PYATTRIBUTE_DEF {
	const char *m_name;				// name of the python attribute
	KX_PYATTRIBUTE_TYPE m_type;		// type of value
	KX_PYATTRIBUTE_ACCESS m_access;	// read/write access or read-only
	int m_imin;						// minimum value in case of integer attributes (for string: minimum string length)
	int m_imax;						// maximum value in case of integer attributes (for string: maximum string length)
	float m_fmin;					// minimum value in case of float attributes
	float m_fmax;					// maximum value in case of float attributes
	bool   m_clamp;					// enforce min/max value by clamping
	size_t m_offset;				// position of field in structure
	size_t m_size;					// size of field for runtime verification (enum only)
	size_t m_length;				// length of array, 1=simple attribute
	KX_PYATTRIBUTE_FUNCTION m_function;	// static function to check the assignment, returns 0 if no error
	// The following pointers are just used to have compile time check for attribute type.
	// It would have been good to use a union but that would require C99 compatibility
	// to initialize specific union fields through designated initializers.
	struct {
		bool *m_boolPtr;
		short int *m_shortPtr;
		int *m_intPtr;
		float *m_floatPtr;
		STR_String *m_stringPtr;
	} m_typeCheck;
} PyAttributeDef;

#define KX_PYATTRIBUTE_DUMMY(name) \
	{ name, KX_PYATTRIBUTE_TYPE_DUMMY, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, 0, 0, 1, NULL, {NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_BOOL_RW(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RW_CHECK(name,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, &object::function, {&((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RO, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL} }

// enum field cannot be mapped to pointer (because we would need a pointer for each enum)
// use field size to verify mapping at runtime only, assuming enum size is equal to int size.
#define KX_PYATTRIBUTE_ENUM_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, {NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, {NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, {NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_SHORT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, {NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, {NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_INT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, {NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, {NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL} }

// always clamp for float
#define KX_PYATTRIBUTE_FLOAT_RW(name,min,max,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RW_CHECK(name,min,max,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, &object::function, {NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW(name,min,max,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, length, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW_CHECK(name,min,max,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, length, &object::function, {NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL} }

#define KX_PYATTRIBUTE_STRING_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field} }
#define KX_PYATTRIBUTE_STRING_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, {NULL, NULL, NULL, NULL, &((object *)0)->field} }
#define KX_PYATTRIBUTE_STRING_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1 , NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field} }

/*------------------------------
 * PyObjectPlus
------------------------------*/
typedef PyTypeObject * PyParentObject;				// Define the PyParent Object

class PyObjectPlus : public PyObject 
{				// The PyObjectPlus abstract class
	Py_Header;							// Always start with Py_Header
	
public:
	PyObjectPlus(PyTypeObject *T);
	
	virtual ~PyObjectPlus();					// destructor
	static void PyDestructor(PyObject *P)				// python wrapper
	{  
		delete ((PyObjectPlus *) P);  
	};
	
//	void INCREF(void) {
//		  Py_INCREF(this);
//	  };				// incref method
//	void DECREF(void) {
//		  Py_DECREF(this);
//	  };				// decref method
	
	virtual PyObject *_getattr(const char *attr);			// _getattr method
	static  PyObject *__getattr(PyObject * PyObj, char *attr) 	// This should be the entry in Type. 
	{
		return ((PyObjectPlus*) PyObj)->_getattr(attr); 
	}
	static PyObject *_getattr_self(const PyAttributeDef attrlist[], void *self, const char *attr);
	static int _setattr_self(const PyAttributeDef attrlist[], void *self, const char *attr, PyObject *value);
	
	virtual int _delattr(const char *attr);
	virtual int _setattr(const char *attr, PyObject *value);		// _setattr method
	static  int __setattr(PyObject *PyObj, 			// This should be the entry in Type. 
				char *attr, 
				PyObject *value)
	{ 
		if (!value)
			return ((PyObjectPlus*) PyObj)->_delattr(attr);
		return ((PyObjectPlus*) PyObj)->_setattr(attr, value);  
	}
	
	virtual PyObject *_repr(void);				// _repr method
	static  PyObject *__repr(PyObject *PyObj)			// This should be the entry in Type.
	{
		return ((PyObjectPlus*) PyObj)->_repr();  
	}
	
									// isA methods
	bool isA(PyTypeObject *T);
	bool isA(const char *mytypename);
	PyObject *Py_isA(PyObject *args);
	static PyObject *sPy_isA(PyObject *self, PyObject *args, PyObject *kwd)
	{
		return ((PyObjectPlus*)self)->Py_isA(args);
	}
};

#endif //  _adr_py_lib_h_

#endif //NO_EXP_PYTHON_EMBEDDING

