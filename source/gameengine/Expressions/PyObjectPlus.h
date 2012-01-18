/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file PyObjectPlus.h
 *  \ingroup expressions
 */

#ifndef _PY_OBJECT_PLUS_H
#define _PY_OBJECT_PLUS_H

/* for now keep weakrefs optional */
#define USE_WEAKREFS


#ifndef __cplusplus				// c++ only
#error Must be compiled with C++
#endif

#include "KX_Python.h"
#include "STR_String.h"
#include "MT_Vector3.h"
#include "SG_QList.h"
#include <stddef.h>

#ifdef WITH_PYTHON
#ifdef USE_MATHUTILS
extern "C" {
#include "../../blender/python/mathutils/mathutils.h" /* so we can have mathutils callbacks */
#include "../../blender/python/generic/py_capi_utils.h" /* for PyC_LineSpit only */
}
#endif

#define MAX_PROP_NAME 64

static inline void Py_Fatal(const char *M)
{
	fprintf(stderr, "%s\n", M);
	exit(-1);
};


/* Use with ShowDeprecationWarning macro */
typedef struct {
	bool warn_done;
	void *link;
} WarnLink;

#define ShowDeprecationWarning(old_way, new_way)                              \
{                                                                             \
	static WarnLink wlink = {false, NULL};                                    \
	if ((m_ignore_deprecation_warnings || wlink.warn_done)==0)                \
	{                                                                         \
		ShowDeprecationWarning_func(old_way, new_way);                        \
		                                                                      \
		WarnLink *wlink_last= GetDeprecationWarningLinkLast();                \
		wlink.warn_done = true;                                               \
		wlink.link = NULL;                                                    \
		                                                                      \
		if(wlink_last) {                                                      \
			wlink_last->link= (void *)&(wlink);                               \
			SetDeprecationWarningLinkLast(&(wlink));                          \
		}                                                                     \
		else {                                                                \
			SetDeprecationWarningFirst(&(wlink));                             \
			SetDeprecationWarningLinkLast(&(wlink));                          \
		}                                                                     \
	}                                                                         \
}                                                                             \



typedef struct PyObjectPlus_Proxy {
	PyObject_HEAD		/* required python macro   */
	class PyObjectPlus *ref;	// pointer to GE object, it holds a reference to this proxy
	void *ptr;					// optional pointer to generic structure, the structure holds no reference to this proxy
	bool py_owns;		// true if the object pointed by ref should be deleted when the proxy is deleted
	bool py_ref;		// true if proxy is connected to a GE object (ref is used)
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist; // weak reference enabler
#endif
} PyObjectPlus_Proxy;

#define BGE_PROXY_ERROR_MSG "Blender Game Engine data has been freed, cannot use this python variable"
#define BGE_PROXY_REF(_self) (((PyObjectPlus_Proxy *)_self)->ref)
#define BGE_PROXY_PTR(_self) (((PyObjectPlus_Proxy *)_self)->ptr)
#define BGE_PROXY_PYOWNS(_self) (((PyObjectPlus_Proxy *)_self)->py_owns)
#define BGE_PROXY_PYREF(_self) (((PyObjectPlus_Proxy *)_self)->py_ref)
#ifdef USE_WEAKREFS
#  define BGE_PROXY_WKREF(_self) (((PyObjectPlus_Proxy *)_self)->in_weakreflist)
#endif

/* Note, sometimes we dont care what BGE type this is as long as its a proxy */
#define BGE_PROXY_CHECK_TYPE(_type) ((_type)->tp_dealloc == PyObjectPlus::py_base_dealloc)

/* Opposite of BGE_PROXY_REF */
#define BGE_PROXY_FROM_REF(_self) (((PyObjectPlus *)_self)->GetProxy())


// This must be the first line of each 
// PyC++ class
// AttributesPtr correspond to attributes of proxy generic pointer 
// each PyC++ class must be registered in KX_PythonInitTypes.cpp
#define __Py_Header                                                           \
public:                                                                       \
	static PyTypeObject   Type;                                               \
	static PyMethodDef    Methods[];                                          \
	static PyAttributeDef Attributes[];                                       \
	virtual PyTypeObject *GetType(void) {                                     \
		return &Type;                                                         \
	}                                                                         \
	virtual PyObject *GetProxy() {                                            \
		return GetProxyPlus_Ext(this, &Type, NULL);                           \
	}                                                                         \
	virtual PyObject *NewProxy(bool py_owns) {                                \
		return NewProxyPlus_Ext(this, &Type, NULL, py_owns);                  \
	}                                                                         \

// leave above line empty (macro)!
// use this macro for class that use generic pointer in proxy
// GetProxy() and NewProxy() must be defined to set the correct pointer in the proxy
#define __Py_HeaderPtr                                                        \
public:                                                                       \
	static PyTypeObject   Type;                                               \
	static PyMethodDef    Methods[];                                          \
	static PyAttributeDef Attributes[];                                       \
	static PyAttributeDef AttributesPtr[];                                    \
	virtual PyTypeObject *GetType(void) {                                     \
		return &Type;                                                         \
	}                                                                         \
	virtual PyObject *GetProxy();                                             \
	virtual PyObject *NewProxy(bool py_owns);                                 \

// leave above line empty (macro)!
#ifdef WITH_CXX_GUARDEDALLOC
#define Py_Header __Py_Header                                                 \
	void *operator new(size_t num_bytes) {                                    \
		return MEM_mallocN(num_bytes, Type.tp_name);                          \
	}                                                                         \
	void operator delete(void *mem) {                                         \
		MEM_freeN(mem);                                                       \
	}                                                                         \

#else
#  define Py_Header __Py_Header
#endif

#ifdef WITH_CXX_GUARDEDALLOC
#define Py_HeaderPtr __Py_HeaderPtr                                           \
	void *operator new(size_t num_bytes) {                                    \
		return MEM_mallocN(num_bytes, Type.tp_name);                          \
	}                                                                         \
	void operator delete( void *mem ) {                                       \
		MEM_freeN(mem);                                                       \
	}                                                                         \

#else
#  define Py_HeaderPtr __Py_HeaderPtr
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
#define KX_PYMETHOD(class_name, method_name)                                   \
	PyObject* Py##method_name(PyObject* args, PyObject* kwds);                 \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* args, PyObject* kwds) {         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "() - "               \
			                BGE_PROXY_ERROR_MSG);                              \
			return NULL;                                                       \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name(args, kwds); \
	}                                                                          \

#define KX_PYMETHOD_VARARGS(class_name, method_name)                           \
	PyObject* Py##method_name(PyObject* args);                                 \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* args) {                         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "() - "               \
			                BGE_PROXY_ERROR_MSG); return NULL;                 \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name(args);       \
	}                                                                          \

#define KX_PYMETHOD_NOARGS(class_name, method_name)                            \
	PyObject* Py##method_name();                                               \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self) {                                         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "() - "               \
			                BGE_PROXY_ERROR_MSG); return NULL;                 \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name();           \
	}                                                                          \

#define KX_PYMETHOD_O(class_name, method_name)                                 \
	PyObject* Py##method_name(PyObject* value);                                \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* value) {                        \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "(value) - "          \
			                BGE_PROXY_ERROR_MSG); return NULL;                 \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name(value);      \
	}                                                                          \

#define KX_PYMETHOD_DOC(class_name, method_name)                               \
	PyObject* Py##method_name(PyObject* args, PyObject* kwds);                 \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* args, PyObject* kwds) {         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "(...) - "            \
			                BGE_PROXY_ERROR_MSG); return NULL;                 \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name(args, kwds); \
	}                                                                          \
	static const char method_name##_doc[];                                     \

#define KX_PYMETHOD_DOC_VARARGS(class_name, method_name)                       \
	PyObject* Py##method_name(PyObject* args);                                 \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* args) {                         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "(...) - "            \
			                BGE_PROXY_ERROR_MSG);                              \
			return NULL;                                                       \
		}                                                                      \
		return((class_name*)BGE_PROXY_REF(self))->Py##method_name(args);       \
	}                                                                          \
	static const char method_name##_doc[];                                     \

#define KX_PYMETHOD_DOC_O(class_name, method_name)                             \
	PyObject* Py##method_name(PyObject* value);                                \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self, PyObject* value) {                        \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "(value) - "          \
			                BGE_PROXY_ERROR_MSG);                              \
			return NULL;                                                       \
		}                                                                      \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name(value);     \
	}                                                                          \
	static const char method_name##_doc[];                                     \

#define KX_PYMETHOD_DOC_NOARGS(class_name, method_name)                        \
	PyObject* Py##method_name();                                               \
	static PyObject*                                                           \
	sPy##method_name(PyObject* self) {                                         \
		if(BGE_PROXY_REF(self)==NULL) {                                        \
			PyErr_SetString(PyExc_RuntimeError,                                \
			                #class_name "." #method_name "() - "               \
			                BGE_PROXY_ERROR_MSG);                              \
			return NULL;                                                       \
		}                                                                      \
		return ((class_name*)BGE_PROXY_REF(self))->Py##method_name();          \
	}                                                                          \
	static const char method_name##_doc[];                                     \


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

#define KX_PYMETHODTABLE_KEYWORDS(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_VARARGS|METH_KEYWORDS, (const char *)class_name::method_name##_doc}

/**
 * Function implementation macro
 */
#define KX_PYMETHODDEF_DOC(class_name, method_name, doc_string) \
const char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject* args, PyObject* kwds)

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
	KX_PYATTRIBUTE_TYPE_FLAG,
	KX_PYATTRIBUTE_TYPE_CHAR
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
	int m_imin;						// minimum value in case of integer attributes 
									// (for string: minimum string length, for flag: mask value, for float: matrix row size)
	int m_imax;						// maximum value in case of integer attributes 
									// (for string: maximum string length, for flag: 1 if flag is negative, float: vector/matrix col size)
	float m_fmin;					// minimum value in case of float attributes
	float m_fmax;					// maximum value in case of float attributes
	bool   m_clamp;					// enforce min/max value by clamping
	bool   m_usePtr;				// the attribute uses the proxy generic pointer, set at runtime
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
		char *m_charPtr;
	} m_typeCheck;
} PyAttributeDef;

#define KX_PYATTRIBUTE_BOOL_RW(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RW_CHECK(name,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RW, 0, 1, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_BOOL_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_BOOL, KX_PYATTRIBUTE_RO, 0, 1, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {&((object *)0)->field, NULL, NULL, NULL, NULL, NULL, NULL} }

/* attribute points to a single bit of an integer field, attribute=true if bit is set */
#define KX_PYATTRIBUTE_FLAG_RW(name,object,field,bit) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RW, bit, 0, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLAG_RW_CHECK(name,object,field,bit,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RW, bit, 0, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLAG_RO(name,object,field,bit) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RO, bit, 0, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }

/* attribute points to a single bit of an integer field, attribute=true if bit is set*/
#define KX_PYATTRIBUTE_FLAG_NEGATIVE_RW(name,object,field,bit) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RW, bit, 1, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLAG_NEGATIVE_RW_CHECK(name,object,field,bit,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RW, bit, 1, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLAG_NEGATIVE_RO(name,object,field,bit) \
	{ name, KX_PYATTRIBUTE_TYPE_FLAG, KX_PYATTRIBUTE_RO, bit, 1, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }

// enum field cannot be mapped to pointer (because we would need a pointer for each enum)
// use field size to verify mapping at runtime only, assuming enum size is equal to int size.
#define KX_PYATTRIBUTE_ENUM_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ENUM_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_ENUM, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_SHORT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, ((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
// SHORT_LIST
#define KX_PYATTRIBUTE_SHORT_LIST_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_LIST_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_SHORT_LIST_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_SHORT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, &((object *)0)->field, NULL, NULL, NULL, NULL, NULL} }

#define KX_PYATTRIBUTE_INT_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, ((object *)0)->field, NULL, NULL, NULL, NULL} }
// INT_LIST
#define KX_PYATTRIBUTE_INT_LIST_RW(name,min,max,clamp,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_LIST_RW_CHECK(name,min,max,clamp,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_INT_LIST_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_INT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, &((object *)0)->field, NULL, NULL, NULL, NULL} }

// always clamp for float
#define KX_PYATTRIBUTE_FLOAT_RW(name,min,max,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RW_CHECK(name,min,max,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, &((object *)0)->field, NULL, NULL, NULL} }
// field must be float[n], returns a sequence
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW(name,min,max,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RW_CHECK(name,min,max,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, length, &object::function, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_ARRAY_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, length, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
// field must be float[n], returns a vector
#define KX_PYATTRIBUTE_FLOAT_VECTOR_RW(name,min,max,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, length, min, max, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_VECTOR_RW_CHECK(name,min,max,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, 0, length, min, max, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_VECTOR_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, 0, length, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field, NULL, NULL, NULL} }
// field must be float[n][n], returns a matrix
#define KX_PYATTRIBUTE_FLOAT_MATRIX_RW(name,min,max,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, length, length, min, max, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field[0], NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_MATRIX_RW_CHECK(name,min,max,object,field,length,function) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RW, length, length, min, max, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field[0], NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_FLOAT_MATRIX_RO(name,object,field,length) \
	{ name, KX_PYATTRIBUTE_TYPE_FLOAT, KX_PYATTRIBUTE_RO, length, length, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, ((object *)0)->field[0], NULL, NULL, NULL} }

// only for STR_String member
#define KX_PYATTRIBUTE_STRING_RW(name,min,max,clamp,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_STRING_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RW, min, max, 0.f, 0.f, clamp, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }
#define KX_PYATTRIBUTE_STRING_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_STRING, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, 1 , NULL, NULL, NULL, {NULL, NULL, NULL, NULL, &((object *)0)->field, NULL, NULL} }

// only for char [] array 
#define KX_PYATTRIBUTE_CHAR_RW(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_CHAR, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0.f, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, ((object *)0)->field} }
#define KX_PYATTRIBUTE_CHAR_RW_CHECK(name,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_CHAR, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0.f, true, false, offsetof(object,field), sizeof(((object *)0)->field), 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, ((object *)0)->field} }
#define KX_PYATTRIBUTE_CHAR_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_CHAR, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), sizeof(((object *)0)->field), 1 , NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, NULL, ((object *)0)->field} }

// for MT_Vector3 member
#define KX_PYATTRIBUTE_VECTOR_RW(name,min,max,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, 1, NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_VECTOR_RW_CHECK(name,min,max,clamp,object,field,function) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RW, 0, 0, min, max, true, false, offsetof(object,field), 0, 1, &object::function, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }
#define KX_PYATTRIBUTE_VECTOR_RO(name,object,field) \
	{ name, KX_PYATTRIBUTE_TYPE_VECTOR, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, offsetof(object,field), 0, 1 , NULL, NULL, NULL, {NULL, NULL, NULL, NULL, NULL, &((object *)0)->field, NULL} }

#define KX_PYATTRIBUTE_RW_FUNCTION(name,object,getfunction,setfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0.f, false, false, 0, 0, 1, NULL, &object::setfunction, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_RO_FUNCTION(name,object,getfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0.f, false, false, 0, 0, 1, NULL, NULL, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ARRAY_RW_FUNCTION(name,object,length,getfunction,setfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RW, 0, 0, 0.f, 0,f, false, false, 0, 0, length, NULL, &object::setfunction, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }
#define KX_PYATTRIBUTE_ARRAY_RO_FUNCTION(name,object,length,getfunction) \
	{ name, KX_PYATTRIBUTE_TYPE_FUNCTION, KX_PYATTRIBUTE_RO, 0, 0, 0.f, 0,f, false, false, 0, 0, length, NULL, NULL, &object::getfunction, {NULL, NULL, NULL, NULL, NULL, NULL, NULL} }


/*------------------------------
 * PyObjectPlus
------------------------------*/
typedef PyTypeObject * PyParentObject;				// Define the PyParent Object

#else // WITH_PYTHON

#ifdef WITH_CXX_GUARDEDALLOC
#define Py_Header                                                             \
public:                                                                       \
	void *operator new(size_t num_bytes) {                                    \
		return MEM_mallocN(num_bytes, "GE:PyObjectPlus");                     \
	}                                                                         \
	void operator delete( void *mem ) {                                       \
		MEM_freeN(mem);                                                       \
	}                                                                         \

#define Py_HeaderPtr                                                          \
public:                                                                       \
	void *operator new(size_t num_bytes) {                                    \
		return MEM_mallocN(num_bytes, "GE:PyObjectPlusPtr");                  \
	}                                                                         \
	void operator delete( void *mem ) {                                       \
	MEM_freeN(mem);                                                           \
	}                                                                         \

#else // WITH_CXX_GUARDEDALLOC

#define Py_Header \
public: \

#define Py_HeaderPtr \
public: \

#endif // WITH_CXX_GUARDEDALLOC

#endif


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
	Py_Header							// Always start with Py_Header
	
public:
	PyObjectPlus();
	
	virtual ~PyObjectPlus();					// destructor
	
#ifdef WITH_PYTHON
	PyObject *m_proxy; /* actually a PyObjectPlus_Proxy */

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
	/* subclass may overwrite this function to implement more sophisticated method of validating a proxy */
	virtual bool			py_is_valid(void) { return true; }

	static PyObject*		py_get_attrdef(PyObject *self_py, const PyAttributeDef *attrdef);
	static int				py_set_attrdef(PyObject *self_py, PyObject *value, const PyAttributeDef *attrdef);
	
	/* Kindof dumb, always returns True, the false case is checked for, before this function gets accessed */
	static PyObject*	pyattr_get_invalid(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);

	static PyObject *GetProxyPlus_Ext(PyObjectPlus *self, PyTypeObject *tp, void *ptr);
	/* self=NULL => proxy to generic pointer detached from GE object
	                if py_owns is true, the memory pointed by ptr will be deleted automatially with MEM_freeN 
	   self!=NULL=> proxy attached to GE object, ptr is optional and point to a struct from which attributes can be defined
	                if py_owns is true, the object will be deleted automatically, ptr will NOT be deleted 
					(assume object destructor takes care of it) */
	static PyObject *NewProxyPlus_Ext(PyObjectPlus *self, PyTypeObject *tp, void *ptr, bool py_owns);

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
	
#endif

	void	InvalidateProxy();

	/**
	 * Makes sure any internal data owned by this class is deep copied.
	 */
	virtual void ProcessReplica();

	static bool			m_ignore_deprecation_warnings;
};

#ifdef WITH_PYTHON
PyObject *py_getattr_dict(PyObject *pydict, PyObject *tp_dict);

PyObject *PyUnicode_From_STR_String(const STR_String& str);
#endif

#endif //  _PY_OBJECT_PLUS_H
