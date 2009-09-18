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
#include "MT_Vector3.h"
#include "SG_QList.h"

/*------------------------------
 * Python defines
------------------------------*/

#ifdef USE_MATHUTILS
extern "C" {
#include "../../blender/python/generic/Mathutils.h" /* so we can have mathutils callbacks */
}
#endif

static inline void Py_Fatal(const char *M) {
	fprintf(stderr, "%s\n", M);
	exit(-1);
};


/* Use with ShowDeprecationWarning macro */
typedef struct {
	bool warn_done;
	void *link;
} WarnLink;

#define ShowDeprecationWarning(old_way, new_way) \
{ \
	static WarnLink wlink = {false, NULL}; \
	if ((m_ignore_deprecation_warnings || wlink.warn_done)==0) \
	{ \
		ShowDeprecationWarning_func(old_way, new_way); \
 \
		WarnLink *wlink_last= GetDeprecationWarningLinkLast(); \
		wlink.warn_done = true; \
		wlink.link = NULL; \
	 \
		if(wlink_last) { \
			wlink_last->link= (void *)&(wlink); \
			SetDeprecationWarningLinkLast(&(wlink)); \
		} else { \
			SetDeprecationWarningFirst(&(wlink)); \
			SetDeprecationWarningLinkLast(&(wlink)); \
		} \
	} \
} \



typedef struct PyObjectPlus_Proxy {
	PyObject_HEAD		/* required python macro   */
	class PyObjectPlus *ref;
	bool py_owns;
} PyObjectPlus_Proxy;

#define BGE_PROXY_ERROR_MSG "Blender Game Engine data has been freed, cannot use this python variable"
#define BGE_PROXY_REF(_self) (((PyObjectPlus_Proxy *)_self)->ref)
#define BGE_PROXY_PYOWNS(_self) (((PyObjectPlus_Proxy *)_self)->py_owns)

/* Note, sometimes we dont care what BGE type this is as long as its a proxy */
#define BGE_PROXY_CHECK_TYPE(_type) ((_type)->tp_dealloc == PyObjectPlus::py_base_dealloc)

/* Opposite of BGE_PROXY_REF */
#define BGE_PROXY_FROM_REF(_self) (((PyObjectPlus *)_self)->GetProxy())


								// This must be the first line of each 
								// PyC++ class
#define __Py_Header \
 public: \
  static PyTypeObject   Type; \
  static PyMethodDef    Methods[]; \
  static PyAttributeDef Attributes[]; \
  virtual PyTypeObject *GetType(void) {return &Type;}; \
  virtual PyObject *GetProxy() {return GetProxy_Ext(this, &Type);}; \
  virtual PyObject *NewProxy(bool py_owns) {return NewProxy_Ext(this, &Type, py_owns);}; \


#ifdef WITH_CXX_GUARDEDALLOC
#define Py_Header __Py_Header \
  void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, Type.tp_name); } \
  void operator delete( void *mem ) { MEM_freeN(mem); } \

#else
#define Py_Header __Py_Header
#endif

/*
 * nonzero values are an error for setattr
 * however because of the nested lookups we need to know if the errors
 * was because the attribute didnt exits of if there was some problem setting the value
 */

#define PY_SET_ATTR_COERCE_FAIL	 2
#define PY_SET_ATTR_FAIL		 1
#define PY_SET_ATTR_MISSING		-1
#define PY_SET_ATTR_SUCCESS		 0

/**
 * These macros are helpfull when embedding Python routines. The second
 * macro is one that also requires a documentation string
 */
#define KX_PYMETHOD(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "() - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(args, kwds);		\
	}; \

#define KX_PYMETHOD_VARARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* args); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "() - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(args);		\
	}; \

#define KX_PYMETHOD_NOARGS(class_name, method_name)			\
	PyObject* Py##method_name(); \
	static PyObject* sPy##method_name( PyObject* self) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "() - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name();		\
	}; \
	
#define KX_PYMETHOD_O(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* value); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* value) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "(value) - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(value);		\
	}; \

#define KX_PYMETHOD_DOC(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "(...) - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(args, kwds);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_VARARGS(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* args); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "(...) - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(args);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_O(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* value); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* value) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "(value) - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(value);		\
	}; \
    static const char method_name##_doc[]; \

#define KX_PYMETHOD_DOC_NOARGS(class_name, method_name)			\
	PyObject* Py##method_name(); \
	static PyObject* sPy##method_name( PyObject* self) { \
		if(BGE_PROXY_REF(self)==NULL) { PyErr_SetString(PyExc_SystemError, #class_name "." #method_name "() - " BGE_PROXY_ERROR_MSG); return NULL; } \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name();		\
	}; \
    static const char method_name##_doc[]; \


/* The line above should remain empty */
/**
 * Method table macro (with doc)
 */
#define KX_PYMETHODTABLE(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_VARARGS, (const char *)class_name::method_name##_doc}

#define KX_PYMETHODTABLE_O(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_O, (const char *)class_name::method_name##_doc}

#define KX_PYMETHODTABLE_NOARGS(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_NOARGS, (const char *)class_name::method_name##_doc}

/**
 * Function implementation macro
 */
#define KX_PYMETHODDEF_DOC(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject* args, PyObject*)

#define KX_PYMETHODDEF_DOC_VARARGS(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject* args)

#define KX_PYMETHODDEF_DOC_O(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject* value)

#define KX_PYMETHODDEF_DOC_NOARGS(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name()

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
	KX_PYATTRIBUTE_TYPE_FUNCTION,
	KX_PYATTRIBUTE_TYPE_VECTOR,
};

enum KX_PYATTRIBUTE_ACCESS {
	KX_PYATTRIBUTE_RW,
	KX_PYATTRIBUTE_RO
};

struct KX_PYATTRIBUTE_DEF;
typedef int (*KX_PYATTRIBUTE_CHECK_FUNCTION)(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
typedef int (*KX_PYATTRIBUTE_SET_FUNCTION)(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
typedef PyObject* (*KX_PYATTRIBUTE_GET_FUNCTION)(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);

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
	KX_PYATTRIBUTE_CHECK_FUNCTION m_checkFunction;	// static function to check the assignment, returns 0 if no error
	KX_PYATTRIBUTE_SET_FUNCTION m_setFunction;	// static function to check the assignment, returns 0 if no error
	KX_PYATTRIBUTE_GET_FUNCTION m_getFunction;	// static function to check the assignment, returns 0 if no error

	// The following pointers are just used to have compile time check for attribute type.
	// It would have been good to use a union but that would require C99 compatibility
	// to initialize specific union fields through designated initializers.
	struct {
		bool *m_boolPtr;
		short int *m_shortPtr;
		int *m_intPtr;
		float *m_floatPtr;
		STR_String *m_stringPtr;
		MT_Vector3 *m_vectorPtr;
	} m_typeCheck;
} PyAttributeDef;

#define KX_PYATTRIBUTE_DUMMY(name) \
	{ name, KX_PYATTRIBUTE_TYPE_DUMMY, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, 0, 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_BOOL_RW(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RW_CHECK(name,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RO, 0, 1, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }

// enum field cannot be mapped to pointer (because we would need a pointer for each enum)
// use field size to verify mapping at runtime only, assuming enum size is equal to int size.
#define KX_PYATTRIBUTE_ENUM_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_SHORT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
// SHORT_LIST
#define KX_PYATTRIBUTE_SHORT_LIST_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_LIST_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_LIST_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_INT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
// INT_LIST
#define KX_PYATTRIBUTE_INT_LIST_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_LIST_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_LIST_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }

// always clamp for float
#define KX_PYATTRIBUTE_FLOAT_RW(name,min,max,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RW_CHECK(name,min,max,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW(name,min,max,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW_CHECK(name,min,max,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL} }

#define KX_PYATTRIBUTE_STRING_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_STRING_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_STRING_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1 , NULL, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }

#define KX_PYATTRIBUTE_VECTOR_RW(name,min,max,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field} }
#define KX_PYATTRIBUTE_VECTOR_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field} }
#define KX_PYATTRIBUTE_VECTOR_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, offsetof(object,field), 0, 1 , NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field} }

#define KX_PYATTRIBUTE_RW_FUNCTION(name,object,getfunction,setfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0.f, false, 0, 0, 1, NULL, &object::setfunction, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_RO_FUNCTION(name,object,getfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, 0, 0, 1, NULL, NULL, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ARRAY_RW_FUNCTION(name,object,length,getfunction,setfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0,f, false, 0, 0, length, NULL, &object::setfunction, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ARRAY_RO_FUNCTION(name,object,length,getfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0,f, false, 0, 0, length, NULL, NULL, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL} }


/*------------------------------
 * PyObjectPlus
------------------------------*/
typedef PyTypeObject * PyParentObject;				// Define the PyParent Object

// By making SG_QList the ultimate parent for PyObjectPlus objects, it
// allows to put them in 2 different dynamic lists at the same time
// The use of these links is interesting because they free of memory allocation
// but it's very important not to mess up with them. If you decide that 
// the SG_QList or SG_DList component is used for something for a certain class,
// they cannot can be used for anything else at a parent level!
// What these lists are and what they are used for must be carefully documented
// at the level where they are used.
// DON'T MAKE ANY USE OF THESE LIST AT THIS LEVEL, they are already used
// at SCA_IActuator, SCA_ISensor, SCA_IController level which rules out the
// possibility to use them at SCA_ILogicBrick, CValue and PyObjectPlus level.
class PyObjectPlus : public SG_QList
{				// The PyObjectPlus abstract class
	Py_Header;							// Always start with Py_Header
	
public:
	PyObjectPlus();

	PyObject *m_proxy; /* actually a PyObjectPlus_Proxy */
	
	virtual ~PyObjectPlus();					// destructor
	
	/* These static functions are referenced by ALL PyObjectPlus_Proxy types
	 * they take the C++ reference from the PyObjectPlus_Proxy and call
	 * its own virtual py_repr, py_base_dealloc ,etc. functions.
	 */

	static PyObject*		py_base_new(PyTypeObject *type, PyObject *args, PyObject *kwds); /* allows subclassing */
	static void			py_base_dealloc(PyObject *self);
	static PyObject*		py_base_repr(PyObject *self);

	/* These are all virtual python methods that are defined in each class
	 * Our own fake subclassing calls these on each class, then calls the parent */
	virtual PyObject*		py_repr(void);

	static PyObject*		py_get_attrdef(PyObject *self_py, const PyAttributeDef *attrdef);
	static int				py_set_attrdef(PyObject *self_py, PyObject *value, const PyAttributeDef *attrdef);
	
	/* Kindof dumb, always returns True, the false case is checked for, before this function gets accessed */
	static PyObject*	pyattr_get_invalid(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	
	static PyObject *GetProxy_Ext(PyObjectPlus *self, PyTypeObject *tp);
	static PyObject *NewProxy_Ext(PyObjectPlus *self, PyTypeObject *tp, bool py_owns);
	
	void	InvalidateProxy();
	
	/**
	 * Makes sure any internal data owned by this class is deep copied.
	 */
	virtual void ProcessReplica();
	
	
	static bool			m_ignore_deprecation_warnings;
	
	static	WarnLink*		GetDeprecationWarningLinkFirst(void);
	static	WarnLink*		GetDeprecationWarningLinkLast(void);
	static	void			SetDeprecationWarningFirst(WarnLink* wlink);
	static	void			SetDeprecationWarningLinkLast(WarnLink* wlink);
	static void			NullDeprecationWarning();
	
	/** enable/disable display of deprecation warnings */
	static void			SetDeprecationWarnings(bool ignoreDeprecationWarnings);
 	/** Shows a deprecation warning */
	static void			ShowDeprecationWarning_func(const char* method,const char* prop);
	static void			ClearDeprecationWarning();
	
};


PyObject *py_getattr_dict(PyObject *pydict, PyObject *tp_dict);

#endif //  _adr_py_lib_h_

#endif //NO_EXP_PYTHON_EMBEDDING

