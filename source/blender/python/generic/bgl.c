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
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/bgl.c
 *  \ingroup pygen
 *
 * This file is the 'bgl' module which wraps OpenGL functions and constants,
 * allowing script writers to make OpenGL calls in their Python scripts.
 *
 * \note
 * This module is very similar to 'PyOpenGL' which could replace 'bgl' one day.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_glew.h"
#include "MEM_guardedalloc.h"

#include "bgl.h"


/* -------------------------------------------------------------------- */

/** \name Local utility defines for wrapping OpenGL
 * \{ */

/*@ By golly George! It looks like fancy pants macro time!!! */


/* TYPE_str is the string to pass to Py_ArgParse (for the format) */
/* TYPE_var is the name to pass to the GL function */
/* TYPE_ref is the pointer to pass to Py_ArgParse (to store in) */
/* TYPE_def is the C initialization of the variable */

#define void_str      ""
#define void_var(num)
#define void_ref(num)   &bgl_var##num
#define void_def(num)   char bgl_var##num

#if 0
#define buffer_str "O!"
#define buffer_var(number)  (bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define buffer_def(number)  Buffer *bgl_buffer##number
#endif

/* GL Pointer fields, handled by buffer type */
/* GLdoubleP, GLfloatP, GLintP, GLuintP, GLshortP, GLsizeiP, GLcharP */

#define GLbooleanP_str      "O!"
#define GLbooleanP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLbooleanP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLbooleanP_def(number)  Buffer *bgl_buffer##number

#define GLbyteP_str     "O!"
#define GLbyteP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLbyteP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLbyteP_def(number) Buffer *bgl_buffer##number

#define GLubyteP_str      "O!"
#define GLubyteP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLubyteP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLubyteP_def(number)  Buffer *bgl_buffer##number

#define GLintP_str      "O!"
#define GLintP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLintP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLintP_def(number)  Buffer *bgl_buffer##number

#define GLint64P_str      "O!"
#define GLint64P_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLint64P_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLint64P_def(number)  Buffer *bgl_buffer##number

#define GLenumP_str      "O!"
#define GLenumP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLenumP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLenumP_def(number)  Buffer *bgl_buffer##number

#define GLuintP_str     "O!"
#define GLuintP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLuintP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLuintP_def(number) Buffer *bgl_buffer##number

#if 0
#define GLuint64P_str     "O!"
#define GLuint64P_var(number) (bgl_buffer##number)->buf.asvoid
#define GLuint64P_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLuint64P_def(number) Buffer *bgl_buffer##number
#endif

#define GLshortP_str      "O!"
#define GLshortP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLshortP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLshortP_def(number)  Buffer *bgl_buffer##number

#define GLushortP_str     "O!"
#define GLushortP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLushortP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLushortP_def(number) Buffer *bgl_buffer##number

#define GLfloatP_str      "O!"
#define GLfloatP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLfloatP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLfloatP_def(number)  Buffer *bgl_buffer##number

#define GLdoubleP_str     "O!"
#define GLdoubleP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLdoubleP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLdoubleP_def(number) Buffer *bgl_buffer##number

#if 0
#define GLclampfP_str     "O!"
#define GLclampfP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLclampfP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLclampfP_def(number) Buffer *bgl_buffer##number
#endif

#define GLvoidP_str     "O!"
#define GLvoidP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLvoidP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLvoidP_def(number) Buffer *bgl_buffer##number

#define GLsizeiP_str     "O!"
#define GLsizeiP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLsizeiP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLsizeiP_def(number) Buffer *bgl_buffer##number

#define GLcharP_str     "O!"
#define GLcharP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLcharP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLcharP_def(number) Buffer *bgl_buffer##number

#if 0
#define buffer_str      "O!"
#define buffer_var(number)  (bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define buffer_def(number)  Buffer *bgl_buffer##number
#endif

/*@The standard GL typedefs are used as prototypes, we can't
 * use the GL type directly because Py_ArgParse expects normal
 * C types.
 *
 * Py_ArgParse doesn't grok writing into unsigned variables,
 * so we use signed everything (even stuff that should be unsigned.
 */

/* typedef unsigned int GLenum; */
#define GLenum_str      "i"
#define GLenum_var(num)   bgl_var##num
#define GLenum_ref(num)   &bgl_var##num
#define GLenum_def(num)   /* unsigned */ int GLenum_var(num)

/* typedef unsigned int GLboolean; */
#define GLboolean_str     "b"
#define GLboolean_var(num)    bgl_var##num
#define GLboolean_ref(num)    &bgl_var##num
#define GLboolean_def(num)    /* unsigned */ char GLboolean_var(num)

/* typedef unsigned int GLbitfield; */
#define GLbitfield_str      "i"
#define GLbitfield_var(num)   bgl_var##num
#define GLbitfield_ref(num)   &bgl_var##num
#define GLbitfield_def(num)   /* unsigned */ int GLbitfield_var(num)

/* typedef signed char GLbyte; */
#define GLbyte_str        "b"
#define GLbyte_var(num)     bgl_var##num
#define GLbyte_ref(num)     &bgl_var##num
#define GLbyte_def(num)     signed char GLbyte_var(num)

/* typedef short GLshort; */
#define GLshort_str       "h"
#define GLshort_var(num)    bgl_var##num
#define GLshort_ref(num)    &bgl_var##num
#define GLshort_def(num)    short GLshort_var(num)

/* typedef int GLint; */
#define GLint_str       "i"
#define GLint_var(num)      bgl_var##num
#define GLint_ref(num)      &bgl_var##num
#define GLint_def(num)      int GLint_var(num)

/* typedef int GLsizei; */
#define GLsizei_str       "n"
#define GLsizei_var(num)    bgl_var##num
#define GLsizei_ref(num)    &bgl_var##num
#define GLsizei_def(num)    size_t GLsizei_var(num)

/* typedef int GLsizeiptr; */
#define GLsizeiptr_str       "n"
#define GLsizeiptr_var(num)    bgl_var##num
#define GLsizeiptr_ref(num)    &bgl_var##num
#define GLsizeiptr_def(num)    size_t GLsizeiptr_var(num)

/* typedef int GLintptr; */
#define GLintptr_str       "n"
#define GLintptr_var(num)    bgl_var##num
#define GLintptr_ref(num)    &bgl_var##num
#define GLintptr_def(num)    size_t GLintptr_var(num)

/* typedef unsigned char GLubyte; */
#define GLubyte_str       "B"
#define GLubyte_var(num)    bgl_var##num
#define GLubyte_ref(num)    &bgl_var##num
#define GLubyte_def(num)    /* unsigned */ char GLubyte_var(num)

/* typedef unsigned short GLushort; */
#define GLushort_str      "H"
#define GLushort_var(num)   bgl_var##num
#define GLushort_ref(num)   &bgl_var##num
#define GLushort_def(num)   /* unsigned */ short GLushort_var(num)

/* typedef unsigned int GLuint; */
#define GLuint_str        "I"
#define GLuint_var(num)     bgl_var##num
#define GLuint_ref(num)     &bgl_var##num
#define GLuint_def(num)     /* unsigned */ int GLuint_var(num)

/* typedef unsigned int GLuint64; */
#if 0
#define GLuint64_str        "Q"
#define GLuint64_var(num)     bgl_var##num
#define GLuint64_ref(num)     &bgl_var##num
#define GLuint64_def(num)     /* unsigned */ int GLuint64_var(num)
#endif

/* typedef unsigned int GLsync; */
#if 0
#define GLsync_str        "I"
#define GLsync_var(num)     bgl_var##num
#define GLsync_ref(num)     &bgl_var##num
#define GLsync_def(num)     /* unsigned */ int GLsync_var(num)
#endif

/* typedef float GLfloat; */
#define GLfloat_str       "f"
#define GLfloat_var(num)    bgl_var##num
#define GLfloat_ref(num)    &bgl_var##num
#define GLfloat_def(num)    float GLfloat_var(num)

/* typedef char *GLstring; */
#define GLstring_str     "s"
#define GLstring_var(number) bgl_var##number
#define GLstring_ref(number) &bgl_var##number
#define GLstring_def(number) char *GLstring_var(number)

/* typedef float GLclampf; */
#if 0
#define GLclampf_str      "f"
#define GLclampf_var(num)   bgl_var##num
#define GLclampf_ref(num)   &bgl_var##num
#define GLclampf_def(num)   float GLclampf_var(num)
#endif

/* typedef double GLdouble; */
#define GLdouble_str      "d"
#define GLdouble_var(num)   bgl_var##num
#define GLdouble_ref(num)   &bgl_var##num
#define GLdouble_def(num)   double GLdouble_var(num)

/* typedef double GLclampd; */
#if 0
#define GLclampd_str      "d"
#define GLclampd_var(num)   bgl_var##num
#define GLclampd_ref(num)   &bgl_var##num
#define GLclampd_def(num)   double GLclampd_var(num)
#endif

#define _arg_def1(a1) \
                   a1##_def(1)
#define _arg_def2(a1, a2) \
        _arg_def1(a1); a2##_def(2)
#define _arg_def3(a1, a2, a3) \
        _arg_def2(a1, a2); a3##_def(3)
#define _arg_def4(a1, a2, a3, a4) \
        _arg_def3(a1, a2, a3); a4##_def(4)
#define _arg_def5(a1, a2, a3, a4, a5) \
        _arg_def4(a1, a2, a3, a4); a5##_def(5)
#define _arg_def6(a1, a2, a3, a4, a5, a6) \
        _arg_def5(a1, a2, a3, a4, a5); a6##_def(6)
#define _arg_def7(a1, a2, a3, a4, a5, a6, a7) \
        _arg_def6(a1, a2, a3, a4, a5, a6); a7##_def(7)
#define _arg_def8(a1, a2, a3, a4, a5, a6, a7, a8) \
        _arg_def7(a1, a2, a3, a4, a5, a6, a7); a8##_def(8)
#define _arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        _arg_def8(a1, a2, a3, a4, a5, a6, a7, a8); a9##_def(9)
#define _arg_def10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        _arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9); a10##_def(10)
#define _arg_def11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_def10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10); a11##_def(11)
#define  arg_def(...) VA_NARGS_CALL_OVERLOAD(_arg_def, __VA_ARGS__)

#define _arg_var1(a1) \
                a1##_var(1)
#define _arg_var2(a1, a2) \
        _arg_var1(a1), a2##_var(2)
#define _arg_var3(a1, a2, a3) \
        _arg_var2(a1, a2), a3##_var(3)
#define _arg_var4(a1, a2, a3, a4) \
        _arg_var3(a1, a2, a3), a4##_var(4)
#define _arg_var5(a1, a2, a3, a4, a5) \
        _arg_var4(a1, a2, a3, a4), a5##_var(5)
#define _arg_var6(a1, a2, a3, a4, a5, a6) \
        _arg_var5(a1, a2, a3, a4, a5), a6##_var(6)
#define _arg_var7(a1, a2, a3, a4, a5, a6, a7) \
        _arg_var6(a1, a2, a3, a4, a5, a6), a7##_var(7)
#define _arg_var8(a1, a2, a3, a4, a5, a6, a7, a8) \
        _arg_var7(a1, a2, a3, a4, a5, a6, a7), a8##_var(8)
#define _arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        _arg_var8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_var(9)
#define _arg_var10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        _arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_var(10)
#define _arg_var11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_var10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10), a11##_var(11)
#define  arg_var(...) VA_NARGS_CALL_OVERLOAD(_arg_var, __VA_ARGS__)

#define _arg_ref1(a1) \
                   a1##_ref(1)
#define _arg_ref2(a1, a2) \
        _arg_ref1(a1), a2##_ref(2)
#define _arg_ref3(a1, a2, a3) \
        _arg_ref2(a1, a2), a3##_ref(3)
#define _arg_ref4(a1, a2, a3, a4) \
        _arg_ref3(a1, a2, a3), a4##_ref(4)
#define _arg_ref5(a1, a2, a3, a4, a5) \
        _arg_ref4(a1, a2, a3, a4), a5##_ref(5)
#define _arg_ref6(a1, a2, a3, a4, a5, a6) \
        _arg_ref5(a1, a2, a3, a4, a5), a6##_ref(6)
#define _arg_ref7(a1, a2, a3, a4, a5, a6, a7) \
        _arg_ref6(a1, a2, a3, a4, a5, a6), a7##_ref(7)
#define _arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8) \
        _arg_ref7(a1, a2, a3, a4, a5, a6, a7), a8##_ref(8)
#define _arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        _arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_ref(9)
#define _arg_ref10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        _arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_ref(10)
#define _arg_ref11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_ref10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10), a11##_ref(11)
#define  arg_ref(...) VA_NARGS_CALL_OVERLOAD(_arg_ref, __VA_ARGS__)

#define _arg_str1(a1) \
                  a1##_str
#define _arg_str2(a1, a2) \
        _arg_str1(a1) a2##_str
#define _arg_str3(a1, a2, a3) \
        _arg_str2(a1, a2) a3##_str
#define _arg_str4(a1, a2, a3, a4) \
        _arg_str3(a1, a2, a3) a4##_str
#define _arg_str5(a1, a2, a3, a4, a5) \
        _arg_str4(a1, a2, a3, a4) a5##_str
#define _arg_str6(a1, a2, a3, a4, a5, a6) \
        _arg_str5(a1, a2, a3, a4, a5) a6##_str
#define _arg_str7(a1, a2, a3, a4, a5, a6, a7) \
        _arg_str6(a1, a2, a3, a4, a5, a6) a7##_str
#define _arg_str8(a1, a2, a3, a4, a5, a6, a7, a8) \
        _arg_str7(a1, a2, a3, a4, a5, a6, a7) a8##_str
#define _arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        _arg_str8(a1, a2, a3, a4, a5, a6, a7, a8) a9##_str
#define _arg_str10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        _arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9) a10##_str
#define _arg_str11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_str10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) a11##_str
#define  arg_str(...) VA_NARGS_CALL_OVERLOAD(_arg_str, __VA_ARGS__)

#define ret_def_void
#define ret_set_void
#define ret_ret_void    return Py_INCREF(Py_None), Py_None

#define ret_def_GLint   int ret_int
#define ret_set_GLint   ret_int =
#define ret_ret_GLint   return PyLong_FromLong(ret_int)

#define ret_def_GLuint    unsigned int ret_uint
#define ret_set_GLuint    ret_uint =
#define ret_ret_GLuint    return PyLong_FromLong((long) ret_uint)

#if 0
#define ret_def_GLsizei   size_t ret_size_t
#define ret_set_GLsizei   ret_size_t =
#define ret_ret_GLsizei   return PyLong_FromSsize_t(ret_size_t)
#endif

#if 0
#define ret_def_GLsync    unsigned int ret_sync
#define ret_set_GLsync    ret_sync =
#define ret_ret_GLsync    return PyLong_FromLong((long) ret_sync)
#endif

#define ret_def_GLenum    unsigned int ret_uint
#define ret_set_GLenum    ret_uint =
#define ret_ret_GLenum    return PyLong_FromLong((long) ret_uint)

#define ret_def_GLboolean unsigned char ret_bool
#define ret_set_GLboolean ret_bool =
#define ret_ret_GLboolean return PyLong_FromLong((long) ret_bool)

#define ret_def_GLstring  const unsigned char *ret_str
#define ret_set_GLstring  ret_str =

#define ret_ret_GLstring                                                      \
	if (ret_str) {                                                            \
		return PyUnicode_FromString((const char *)ret_str);                   \
	}                                                                         \
	else {                                                                    \
		PyErr_SetString(PyExc_AttributeError, "could not get opengl string"); \
		return NULL;                                                          \
	}                                                                         \

/** \} */


/* -------------------------------------------------------------------- */
/* Forward Declarations */

static PyObject *Buffer_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static PyObject *Method_ShaderSource(PyObject *self, PyObject *args);

/* Buffer sequence methods */

static int Buffer_len(Buffer *self);
static PyObject *Buffer_item(Buffer *self, int i);
static PyObject *Buffer_slice(Buffer *self, int begin, int end);
static int Buffer_ass_item(Buffer *self, int i, PyObject *v);
static int Buffer_ass_slice(Buffer *self, int begin, int end, PyObject *seq);
static PyObject *Buffer_subscript(Buffer *self, PyObject *item);
static int Buffer_ass_subscript(Buffer *self, PyObject *item, PyObject *value);


/* -------------------------------------------------------------------- */

/** \name Utility Functions
 * \{ */


int BGL_typeSize(int type)
{
	switch (type) {
		case GL_BYTE:
			return sizeof(char);
		case GL_SHORT:
			return sizeof(short);
		case GL_INT:
			return sizeof(int);
		case GL_FLOAT:
			return sizeof(float);
		case GL_DOUBLE:
			return sizeof(double);
	}
	return -1;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Buffer API
 * \{ */

static PySequenceMethods Buffer_SeqMethods = {
	(lenfunc) Buffer_len,                       /*sq_length */
	(binaryfunc) NULL,                          /*sq_concat */
	(ssizeargfunc) NULL,                        /*sq_repeat */
	(ssizeargfunc) Buffer_item,                 /*sq_item */
	(ssizessizeargfunc) NULL,                   /*sq_slice, deprecated, handled in Buffer_item */
	(ssizeobjargproc) Buffer_ass_item,          /*sq_ass_item */
	(ssizessizeobjargproc) NULL,                /*sq_ass_slice, deprecated handled in Buffer_ass_item */
	(objobjproc) NULL,                          /* sq_contains */
	(binaryfunc) NULL,                          /* sq_inplace_concat */
	(ssizeargfunc) NULL,                        /* sq_inplace_repeat */
};


static PyMappingMethods Buffer_AsMapping = {
	(lenfunc)Buffer_len,
	(binaryfunc)Buffer_subscript,
	(objobjargproc)Buffer_ass_subscript
};

static void Buffer_dealloc(Buffer *self);
static PyObject *Buffer_repr(Buffer *self);

static PyObject *Buffer_to_list(Buffer *self)
{
	int i, len = self->dimensions[0];
	PyObject *list = PyList_New(len);

	for (i = 0; i < len; i++) {
		PyList_SET_ITEM(list, i, Buffer_item(self, i));
	}

	return list;
}

static PyObject *Buffer_to_list_recursive(Buffer *self)
{
	PyObject *list;

	if (self->ndimensions > 1) {
		int i, len = self->dimensions[0];
		list = PyList_New(len);

		for (i = 0; i < len; i++) {
			Buffer *sub = (Buffer *)Buffer_item(self, i);
			PyList_SET_ITEM(list, i, Buffer_to_list_recursive(sub));
			Py_DECREF(sub);
		}
	}
	else {
		list = Buffer_to_list(self);
	}

	return list;
}

static PyObject *Buffer_dimensions(Buffer *self, void *UNUSED(arg))
{
	PyObject *list = PyList_New(self->ndimensions);
	int i;

	for (i = 0; i < self->ndimensions; i++) {
		PyList_SET_ITEM(list, i, PyLong_FromLong(self->dimensions[i]));
	}

	return list;
}

static PyMethodDef Buffer_methods[] = {
	{"to_list", (PyCFunction)Buffer_to_list_recursive, METH_NOARGS,
	 "return the buffer as a list"},
	{NULL, NULL, 0, NULL}
};

static PyGetSetDef Buffer_getseters[] = {
	{(char *)"dimensions", (getter)Buffer_dimensions, NULL, NULL, NULL},
	{NULL, NULL, NULL, NULL, NULL}
};


PyTypeObject BGL_bufferType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bgl.Buffer",               /*tp_name */
	sizeof(Buffer),             /*tp_basicsize */
	0,                          /*tp_itemsize */
	(destructor)Buffer_dealloc, /*tp_dealloc */
	(printfunc)NULL,            /*tp_print */
	NULL,                       /*tp_getattr */
	NULL,                       /*tp_setattr */
	NULL,                       /*tp_compare */
	(reprfunc) Buffer_repr,     /*tp_repr */
	NULL,                       /*tp_as_number */
	&Buffer_SeqMethods,         /*tp_as_sequence */
	&Buffer_AsMapping,          /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL, /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

	/*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
	/*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

	/***  Assigned meaning in release 2.1 ***/
	/*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

	/***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

	/*** Added in release 2.2 ***/
	/*   Iterators */
	NULL, /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */
	/*** Attribute descriptor and subclassing stuff ***/
	Buffer_methods,             /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	Buffer_getseters,           /* struct PyGetSetDef *tp_getset; */
	NULL,                       /*tp_base*/
	NULL,                       /*tp_dict*/
	NULL,                       /*tp_descr_get*/
	NULL,                       /*tp_descr_set*/
	0,                          /*tp_dictoffset*/
	NULL,                       /*tp_init*/
	NULL,                       /*tp_alloc*/
	Buffer_new,                 /*tp_new*/
	NULL,                       /*tp_free*/
	NULL,                       /*tp_is_gc*/
	NULL,                       /*tp_bases*/
	NULL,                       /*tp_mro*/
	NULL,                       /*tp_cache*/
	NULL,                       /*tp_subclasses*/
	NULL,                       /*tp_weaklist*/
	NULL                        /*tp_del*/
};

/**
 * Create a buffer object
 *
 * \param dimensions: An array of ndimensions integers representing the size of each dimension.
 * \param initbuffer: When not NULL holds a contiguous buffer
 * with the correct format from which the buffer will be initialized
 */
Buffer *BGL_MakeBuffer(int type, int ndimensions, int *dimensions, void *initbuffer)
{
	Buffer *buffer;
	void *buf = NULL;
	int i, size, length;

	length = 1;
	for (i = 0; i < ndimensions; i++) {
		length *= dimensions[i];
	}

	size = BGL_typeSize(type);

	buf = MEM_mallocN(length * size, "Buffer buffer");

	buffer = (Buffer *)PyObject_NEW(Buffer, &BGL_bufferType);
	buffer->parent = NULL;
	buffer->ndimensions = ndimensions;
	buffer->dimensions = MEM_mallocN(ndimensions * sizeof(int), "Buffer dimensions");
	memcpy(buffer->dimensions, dimensions, ndimensions * sizeof(int));
	buffer->type = type;
	buffer->buf.asvoid = buf;

	if (initbuffer) {
		memcpy(buffer->buf.asvoid, initbuffer, length * size);
	}
	else {
		memset(buffer->buf.asvoid, 0, length * size);
	}
	return buffer;
}


#define MAX_DIMENSIONS  256
static PyObject *Buffer_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	PyObject *length_ob = NULL, *init = NULL;
	Buffer *buffer;
	int dimensions[MAX_DIMENSIONS];

	int type;
	Py_ssize_t i, ndimensions = 0;

	if (kwds && PyDict_Size(kwds)) {
		PyErr_SetString(PyExc_TypeError,
		                "bgl.Buffer(): takes no keyword args");
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "iO|O: bgl.Buffer", &type, &length_ob, &init)) {
		return NULL;
	}
	if (!ELEM(type, GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT, GL_DOUBLE)) {
		PyErr_SetString(PyExc_AttributeError,
		                "invalid first argument type, should be one of "
		                "GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT or GL_DOUBLE");
		return NULL;
	}

	if (PyLong_Check(length_ob)) {
		ndimensions = 1;
		if (((dimensions[0] = PyLong_AsLong(length_ob)) < 1)) {
			PyErr_SetString(PyExc_AttributeError,
			                "dimensions must be between 1 and "STRINGIFY(MAX_DIMENSIONS));
			return NULL;
		}
	}
	else if (PySequence_Check(length_ob)) {
		ndimensions = PySequence_Size(length_ob);
		if (ndimensions > MAX_DIMENSIONS) {
			PyErr_SetString(PyExc_AttributeError,
			                "too many dimensions, max is "STRINGIFY(MAX_DIMENSIONS));
			return NULL;
		}
		else if (ndimensions < 1) {
			PyErr_SetString(PyExc_AttributeError,
			                "sequence must have at least one dimension");
			return NULL;
		}
		for (i = 0; i < ndimensions; i++) {
			PyObject *ob = PySequence_GetItem(length_ob, i);

			if (!PyLong_Check(ob))
				dimensions[i] = 1;
			else
				dimensions[i] = PyLong_AsLong(ob);
			Py_DECREF(ob);

			if (dimensions[i] < 1) {
				PyErr_SetString(PyExc_AttributeError,
				                "dimensions must be between 1 and "STRINGIFY(MAX_DIMENSIONS));
				return NULL;
			}
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "invalid second argument argument expected a sequence "
		             "or an int, not a %.200s", Py_TYPE(length_ob)->tp_name);
		return NULL;
	}

	buffer = BGL_MakeBuffer(type, ndimensions, dimensions, NULL);
	if (init && ndimensions) {
		if (Buffer_ass_slice(buffer, 0, dimensions[0], init)) {
			Py_DECREF(buffer);
			return NULL;
		}
	}

	return (PyObject *)buffer;
}

/* Buffer sequence methods */

static int Buffer_len(Buffer *self)
{
	return self->dimensions[0];
}

static PyObject *Buffer_item(Buffer *self, int i)
{
	if (i >= self->dimensions[0] || i < 0) {
		PyErr_SetString(PyExc_IndexError, "array index out of range");
		return NULL;
	}

	if (self->ndimensions == 1) {
		switch (self->type) {
			case GL_BYTE:   return Py_BuildValue("b", self->buf.asbyte[i]);
			case GL_SHORT:  return Py_BuildValue("h", self->buf.asshort[i]);
			case GL_INT:    return Py_BuildValue("i", self->buf.asint[i]);
			case GL_FLOAT:  return PyFloat_FromDouble(self->buf.asfloat[i]);
			case GL_DOUBLE: return Py_BuildValue("d", self->buf.asdouble[i]);
		}
	}
	else {
		Buffer *newbuf;
		int j, length, size;

		length = 1;
		for (j = 1; j < self->ndimensions; j++) {
			length *= self->dimensions[j];
		}
		size = BGL_typeSize(self->type);

		newbuf = (Buffer *)PyObject_NEW(Buffer, &BGL_bufferType);

		Py_INCREF(self);
		newbuf->parent = (PyObject *)self;

		newbuf->ndimensions = self->ndimensions - 1;
		newbuf->type = self->type;
		newbuf->buf.asvoid = self->buf.asbyte + i * length * size;
		newbuf->dimensions = MEM_mallocN(newbuf->ndimensions * sizeof(int), "Buffer dimensions");
		memcpy(newbuf->dimensions, self->dimensions + 1, newbuf->ndimensions * sizeof(int));

		return (PyObject *)newbuf;
	}

	return NULL;
}

static PyObject *Buffer_slice(Buffer *self, int begin, int end)
{
	PyObject *list;
	int count;
	
	if (begin < 0) begin = 0;
	if (end > self->dimensions[0]) end = self->dimensions[0];
	if (begin > end) begin = end;

	list = PyList_New(end - begin);

	for (count = begin; count < end; count++) {
		PyList_SET_ITEM(list, count - begin, Buffer_item(self, count));
	}
	return list;
}

static int Buffer_ass_item(Buffer *self, int i, PyObject *v)
{
	if (i >= self->dimensions[0] || i < 0) {
		PyErr_SetString(PyExc_IndexError,
		                "array assignment index out of range");
		return -1;
	}

	if (self->ndimensions != 1) {
		Buffer *row = (Buffer *)Buffer_item(self, i);

		if (row) {
			int ret = Buffer_ass_slice(row, 0, self->dimensions[1], v);
			Py_DECREF(row);
			return ret;
		}
		else {
			return -1;
		}
	}

	switch (self->type) {
		case GL_BYTE:   return PyArg_Parse(v, "b:Expected ints",   &self->buf.asbyte[i])   ? 0 : -1;
		case GL_SHORT:  return PyArg_Parse(v, "h:Expected ints",   &self->buf.asshort[i])  ? 0 : -1;
		case GL_INT:    return PyArg_Parse(v, "i:Expected ints",   &self->buf.asint[i])    ? 0 : -1;
		case GL_FLOAT:  return PyArg_Parse(v, "f:Expected floats", &self->buf.asfloat[i])  ? 0 : -1;
		case GL_DOUBLE: return PyArg_Parse(v, "d:Expected floats", &self->buf.asdouble[i]) ? 0 : -1;
		default:        return 0; /* should never happen */
	}
}

static int Buffer_ass_slice(Buffer *self, int begin, int end, PyObject *seq)
{
	PyObject *item;
	int count, err = 0;
	
	if (begin < 0) begin = 0;
	if (end > self->dimensions[0]) end = self->dimensions[0];
	if (begin > end) begin = end;
	
	if (!PySequence_Check(seq)) {
		PyErr_Format(PyExc_TypeError,
		             "buffer[:] = value, invalid assignment. "
		             "Expected a sequence, not an %.200s type",
		             Py_TYPE(seq)->tp_name);
		return -1;
	}

	/* re-use count var */
	if ((count = PySequence_Size(seq)) != (end - begin)) {
		PyErr_Format(PyExc_TypeError,
		             "buffer[:] = value, size mismatch in assignment. "
		             "Expected: %d (given: %d)", count, end - begin);
		return -1;
	}
	
	for (count = begin; count < end; count++) {
		item = PySequence_GetItem(seq, count - begin);
		if (item) {
			err = Buffer_ass_item(self, count, item);
			Py_DECREF(item);
		}
		else {
			err = -1;
		}
		if (err) {
			break;
		}
	}
	return err;
}

static PyObject *Buffer_subscript(Buffer *self, PyObject *item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += self->dimensions[0];
		return Buffer_item(self, i);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx(item, self->dimensions[0], &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyTuple_New(0);
		}
		else if (step == 1) {
			return Buffer_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with vectors");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "buffer indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return NULL;
	}
}

static int Buffer_ass_subscript(Buffer *self, PyObject *item, PyObject *value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += self->dimensions[0];
		return Buffer_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx(item, self->dimensions[0], &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Buffer_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with vectors");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "buffer indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return -1;
	}
}


static void Buffer_dealloc(Buffer *self)
{
	if (self->parent) {
		Py_DECREF(self->parent);
	}
	else {
		MEM_freeN(self->buf.asvoid);
	}

	MEM_freeN(self->dimensions);

	PyObject_DEL(self);
}


static PyObject *Buffer_repr(Buffer *self)
{
	PyObject *list = Buffer_to_list_recursive(self);
	PyObject *repr;
	const char *typestr;

	switch (self->type) {
		case GL_BYTE:   typestr = "GL_BYTE"; break;
		case GL_SHORT:  typestr = "GL_SHORT"; break;
		case GL_INT:    typestr = "GL_INT"; break;
		case GL_FLOAT:  typestr = "GL_FLOAT"; break;
		case GL_DOUBLE: typestr = "GL_DOUBLE"; break;
		default:        typestr = "UNKNOWN"; break;
	}

	repr = PyUnicode_FromFormat("Buffer(%s, %R)", typestr, list);
	Py_DECREF(list);

	return repr;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name OpenGL API Wrapping
 * \{ */

#define BGL_Wrap(funcname, ret, arg_list)                                     \
static PyObject *Method_##funcname (PyObject *UNUSED(self), PyObject *args)   \
{                                                                             \
	arg_def arg_list;                                                         \
	ret_def_##ret;                                                            \
	if (!PyArg_ParseTuple(args, arg_str arg_list, arg_ref arg_list)) {        \
		return NULL;                                                          \
	}                                                                         \
	ret_set_##ret gl##funcname (arg_var arg_list);                            \
	ret_ret_##ret;                                                            \
}

#define BGLU_Wrap(funcname, ret, arg_list)                                    \
static PyObject *Method_##funcname (PyObject *UNUSED(self), PyObject *args)   \
{                                                                             \
	arg_def arg_list;                                                         \
	ret_def_##ret;                                                            \
	if (!PyArg_ParseTuple(args, arg_str arg_list, arg_ref arg_list)) {        \
		return NULL;                                                          \
	}                                                                         \
	ret_set_##ret glu##funcname (arg_var arg_list);                           \
	ret_ret_##ret;                                                            \
}

/* GL_VERSION_1_0 */
BGL_Wrap(Accum,                     void,      (GLenum, GLfloat))
BGL_Wrap(AlphaFunc,                 void,      (GLenum, GLfloat))
BGL_Wrap(Begin,                     void,      (GLenum))
BGL_Wrap(Bitmap,                    void,      (GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, GLubyteP))
BGL_Wrap(BlendFunc,                 void,      (GLenum, GLenum))
BGL_Wrap(CallList,                  void,      (GLuint))
BGL_Wrap(CallLists,                 void,      (GLsizei, GLenum, GLvoidP))
BGL_Wrap(Clear,                     void,      (GLbitfield))
BGL_Wrap(ClearAccum,                void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(ClearColor,                void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(ClearDepth,                void,      (GLdouble))
BGL_Wrap(ClearIndex,                void,      (GLfloat))
BGL_Wrap(ClearStencil,              void,      (GLint))
BGL_Wrap(ClipPlane,                 void,      (GLenum, GLdoubleP))
BGL_Wrap(Color3b,                   void,      (GLbyte, GLbyte, GLbyte))
BGL_Wrap(Color3bv,                  void,      (GLbyteP))
BGL_Wrap(Color3d,                   void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Color3dv,                  void,      (GLdoubleP))
BGL_Wrap(Color3f,                   void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Color3fv,                  void,      (GLfloatP))
BGL_Wrap(Color3i,                   void,      (GLint, GLint, GLint))
BGL_Wrap(Color3iv,                  void,      (GLintP))
BGL_Wrap(Color3s,                   void,      (GLshort, GLshort, GLshort))
BGL_Wrap(Color3sv,                  void,      (GLshortP))
BGL_Wrap(Color3ub,                  void,      (GLubyte, GLubyte, GLubyte))
BGL_Wrap(Color3ubv,                 void,      (GLubyteP))
BGL_Wrap(Color3ui,                  void,      (GLuint, GLuint, GLuint))
BGL_Wrap(Color3uiv,                 void,      (GLuintP))
BGL_Wrap(Color3us,                  void,      (GLushort, GLushort, GLushort))
BGL_Wrap(Color3usv,                 void,      (GLushortP))
BGL_Wrap(Color4b,                   void,      (GLbyte, GLbyte, GLbyte, GLbyte))
BGL_Wrap(Color4bv,                  void,      (GLbyteP))
BGL_Wrap(Color4d,                   void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Color4dv,                  void,      (GLdoubleP))
BGL_Wrap(Color4f,                   void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Color4fv,                  void,      (GLfloatP))
BGL_Wrap(Color4i,                   void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(Color4iv,                  void,      (GLintP))
BGL_Wrap(Color4s,                   void,      (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Color4sv,                  void,      (GLshortP))
BGL_Wrap(Color4ub,                  void,      (GLubyte, GLubyte, GLubyte, GLubyte))
BGL_Wrap(Color4ubv,                 void,      (GLubyteP))
BGL_Wrap(Color4ui,                  void,      (GLuint, GLuint, GLuint, GLuint))
BGL_Wrap(Color4uiv,                 void,      (GLuintP))
BGL_Wrap(Color4us,                  void,      (GLushort, GLushort, GLushort, GLushort))
BGL_Wrap(Color4usv,                 void,      (GLushortP))
BGL_Wrap(ColorMask,                 void,      (GLboolean, GLboolean, GLboolean, GLboolean))
BGL_Wrap(ColorMaterial,             void,      (GLenum, GLenum))
BGL_Wrap(CopyPixels,                void,      (GLint, GLint, GLsizei, GLsizei, GLenum))
BGL_Wrap(CullFace,                  void,      (GLenum))
BGL_Wrap(DeleteLists,               void,      (GLuint, GLsizei))
BGL_Wrap(DepthFunc,                 void,      (GLenum))
BGL_Wrap(DepthMask,                 void,      (GLboolean))
BGL_Wrap(DepthRange,                void,      (GLdouble, GLdouble))
BGL_Wrap(Disable,                   void,      (GLenum))
BGL_Wrap(DrawBuffer,                void,      (GLenum))
BGL_Wrap(DrawPixels,                void,      (GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(EdgeFlag,                  void,      (GLboolean))
BGL_Wrap(EdgeFlagv,                 void,      (GLbooleanP))
BGL_Wrap(Enable,                    void,      (GLenum))
BGL_Wrap(End,                       void,      (void))
BGL_Wrap(EndList,                   void,      (void))
BGL_Wrap(EvalCoord1d,               void,      (GLdouble))
BGL_Wrap(EvalCoord1dv,              void,      (GLdoubleP))
BGL_Wrap(EvalCoord1f,               void,      (GLfloat))
BGL_Wrap(EvalCoord1fv,              void,      (GLfloatP))
BGL_Wrap(EvalCoord2d,               void,      (GLdouble, GLdouble))
BGL_Wrap(EvalCoord2dv,              void,      (GLdoubleP))
BGL_Wrap(EvalCoord2f,               void,      (GLfloat, GLfloat))
BGL_Wrap(EvalCoord2fv,              void,      (GLfloatP))
BGL_Wrap(EvalMesh1,                 void,      (GLenum, GLint, GLint))
BGL_Wrap(EvalMesh2,                 void,      (GLenum, GLint, GLint, GLint, GLint))
BGL_Wrap(EvalPoint1,                void,      (GLint))
BGL_Wrap(EvalPoint2,                void,      (GLint, GLint))
BGL_Wrap(FeedbackBuffer,            void,      (GLsizei, GLenum, GLfloatP))
BGL_Wrap(Finish,                    void,      (void))
BGL_Wrap(Flush,                     void,      (void))
BGL_Wrap(Fogf,                      void,      (GLenum, GLfloat))
BGL_Wrap(Fogfv,                     void,      (GLenum, GLfloatP))
BGL_Wrap(Fogi,                      void,      (GLenum, GLint))
BGL_Wrap(Fogiv,                     void,      (GLenum, GLintP))
BGL_Wrap(FrontFace,                 void,      (GLenum))
BGL_Wrap(Frustum,                   void,      (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(GenLists,                  GLuint,    (GLsizei))
BGL_Wrap(GetBooleanv,               void,      (GLenum, GLbooleanP))
BGL_Wrap(GetClipPlane,              void,      (GLenum, GLdoubleP))
BGL_Wrap(GetDoublev,                void,      (GLenum, GLdoubleP))
BGL_Wrap(GetError,                  GLenum,    (void))
BGL_Wrap(GetFloatv,                 void,      (GLenum, GLfloatP))
BGL_Wrap(GetIntegerv,               void,      (GLenum, GLintP))
BGL_Wrap(GetLightfv,                void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetLightiv,                void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetMapdv,                  void,      (GLenum, GLenum, GLdoubleP))
BGL_Wrap(GetMapfv,                  void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetMapiv,                  void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetMaterialfv,             void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetMaterialiv,             void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetPixelMapfv,             void,      (GLenum, GLfloatP))
BGL_Wrap(GetPixelMapuiv,            void,      (GLenum, GLuintP))
BGL_Wrap(GetPixelMapusv,            void,      (GLenum, GLushortP))
BGL_Wrap(GetPolygonStipple,         void,      (GLubyteP))
BGL_Wrap(GetString,                 GLstring,  (GLenum))
BGL_Wrap(GetTexEnvfv,               void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexEnviv,               void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetTexGendv,               void,      (GLenum, GLenum, GLdoubleP))
BGL_Wrap(GetTexGenfv,               void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexGeniv,               void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetTexImage,               void,      (GLenum, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(GetTexLevelParameterfv,    void,      (GLenum, GLint, GLenum, GLfloatP))
BGL_Wrap(GetTexLevelParameteriv,    void,      (GLenum, GLint, GLenum, GLintP))
BGL_Wrap(GetTexParameterfv,         void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexParameteriv,         void,      (GLenum, GLenum, GLintP))
BGL_Wrap(Hint,                      void,      (GLenum, GLenum))
BGL_Wrap(IndexMask,                 void,      (GLuint))
BGL_Wrap(Indexd,                    void,      (GLdouble))
BGL_Wrap(Indexdv,                   void,      (GLdoubleP))
BGL_Wrap(Indexf,                    void,      (GLfloat))
BGL_Wrap(Indexfv,                   void,      (GLfloatP))
BGL_Wrap(Indexi,                    void,      (GLint))
BGL_Wrap(Indexiv,                   void,      (GLintP))
BGL_Wrap(Indexs,                    void,      (GLshort))
BGL_Wrap(Indexsv,                   void,      (GLshortP))
BGL_Wrap(InitNames,                 void,      (void))
BGL_Wrap(IsEnabled,                 GLboolean, (GLenum))
BGL_Wrap(IsList,                    GLboolean, (GLuint))
BGL_Wrap(LightModelf,               void,      (GLenum, GLfloat))
BGL_Wrap(LightModelfv,              void,      (GLenum, GLfloatP))
BGL_Wrap(LightModeli,               void,      (GLenum, GLint))
BGL_Wrap(LightModeliv,              void,      (GLenum, GLintP))
BGL_Wrap(Lightf,                    void,      (GLenum, GLenum, GLfloat))
BGL_Wrap(Lightfv,                   void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(Lighti,                    void,      (GLenum, GLenum, GLint))
BGL_Wrap(Lightiv,                   void,      (GLenum, GLenum, GLintP))
BGL_Wrap(LineStipple,               void,      (GLint, GLushort))
BGL_Wrap(LineWidth,                 void,      (GLfloat))
BGL_Wrap(ListBase,                  void,      (GLuint))
BGL_Wrap(LoadIdentity,              void,      (void))
BGL_Wrap(LoadMatrixd,               void,      (GLdoubleP))
BGL_Wrap(LoadMatrixf,               void,      (GLfloatP))
BGL_Wrap(LoadName,                  void,      (GLuint))
BGL_Wrap(LogicOp,                   void,      (GLenum))
BGL_Wrap(Map1d,                     void,      (GLenum, GLdouble, GLdouble, GLint, GLint, GLdoubleP))
BGL_Wrap(Map1f,                     void,      (GLenum, GLfloat, GLfloat, GLint, GLint, GLfloatP))
BGL_Wrap(Map2d,                     void,      (GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, GLdoubleP))
BGL_Wrap(Map2f,                     void,      (GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, GLfloatP))
BGL_Wrap(MapGrid1d,                 void,      (GLint, GLdouble, GLdouble))
BGL_Wrap(MapGrid1f,                 void,      (GLint, GLfloat, GLfloat))
BGL_Wrap(MapGrid2d,                 void,      (GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble))
BGL_Wrap(MapGrid2f,                 void,      (GLint, GLfloat, GLfloat, GLint, GLfloat, GLfloat))
BGL_Wrap(Materialf,                 void,      (GLenum, GLenum, GLfloat))
BGL_Wrap(Materialfv,                void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(Materiali,                 void,      (GLenum, GLenum, GLint))
BGL_Wrap(Materialiv,                void,      (GLenum, GLenum, GLintP))
BGL_Wrap(MatrixMode,                void,      (GLenum))
BGL_Wrap(MultMatrixd,               void,      (GLdoubleP))
BGL_Wrap(MultMatrixf,               void,      (GLfloatP))
BGL_Wrap(NewList,                   void,      (GLuint, GLenum))
BGL_Wrap(Normal3b,                  void,      (GLbyte, GLbyte, GLbyte))
BGL_Wrap(Normal3bv,                 void,      (GLbyteP))
BGL_Wrap(Normal3d,                  void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Normal3dv,                 void,      (GLdoubleP))
BGL_Wrap(Normal3f,                  void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Normal3fv,                 void,      (GLfloatP))
BGL_Wrap(Normal3i,                  void,      (GLint, GLint, GLint))
BGL_Wrap(Normal3iv,                 void,      (GLintP))
BGL_Wrap(Normal3s,                  void,      (GLshort, GLshort, GLshort))
BGL_Wrap(Normal3sv,                 void,      (GLshortP))
BGL_Wrap(Ortho,                     void,      (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(PassThrough,               void,      (GLfloat))
BGL_Wrap(PixelMapfv,                void,      (GLenum, GLsizei, GLfloatP))
BGL_Wrap(PixelMapuiv,               void,      (GLenum, GLsizei, GLuintP))
BGL_Wrap(PixelMapusv,               void,      (GLenum, GLsizei, GLushortP))
BGL_Wrap(PixelStoref,               void,      (GLenum, GLfloat))
BGL_Wrap(PixelStorei,               void,      (GLenum, GLint))
BGL_Wrap(PixelTransferf,            void,      (GLenum, GLfloat))
BGL_Wrap(PixelTransferi,            void,      (GLenum, GLint))
BGL_Wrap(PixelZoom,                 void,      (GLfloat, GLfloat))
BGL_Wrap(PointSize,                 void,      (GLfloat))
BGL_Wrap(PolygonMode,               void,      (GLenum, GLenum))
BGL_Wrap(PolygonStipple,            void,      (GLubyteP))
BGL_Wrap(PopAttrib,                 void,      (void))
BGL_Wrap(PopMatrix,                 void,      (void))
BGL_Wrap(PopName,                   void,      (void))
BGL_Wrap(PushAttrib,                void,      (GLbitfield))
BGL_Wrap(PushMatrix,                void,      (void))
BGL_Wrap(PushName,                  void,      (GLuint))
BGL_Wrap(RasterPos2d,               void,      (GLdouble, GLdouble))
BGL_Wrap(RasterPos2dv,              void,      (GLdoubleP))
BGL_Wrap(RasterPos2f,               void,      (GLfloat, GLfloat))
BGL_Wrap(RasterPos2fv,              void,      (GLfloatP))
BGL_Wrap(RasterPos2i,               void,      (GLint, GLint))
BGL_Wrap(RasterPos2iv,              void,      (GLintP))
BGL_Wrap(RasterPos2s,               void,      (GLshort, GLshort))
BGL_Wrap(RasterPos2sv,              void,      (GLshortP))
BGL_Wrap(RasterPos3d,               void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(RasterPos3dv,              void,      (GLdoubleP))
BGL_Wrap(RasterPos3f,               void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(RasterPos3fv,              void,      (GLfloatP))
BGL_Wrap(RasterPos3i,               void,      (GLint, GLint, GLint))
BGL_Wrap(RasterPos3iv,              void,      (GLintP))
BGL_Wrap(RasterPos3s,               void,      (GLshort, GLshort, GLshort))
BGL_Wrap(RasterPos3sv,              void,      (GLshortP))
BGL_Wrap(RasterPos4d,               void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(RasterPos4dv,              void,      (GLdoubleP))
BGL_Wrap(RasterPos4f,               void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(RasterPos4fv,              void,      (GLfloatP))
BGL_Wrap(RasterPos4i,               void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(RasterPos4iv,              void,      (GLintP))
BGL_Wrap(RasterPos4s,               void,      (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(RasterPos4sv,              void,      (GLshortP))
BGL_Wrap(ReadBuffer,                void,      (GLenum))
BGL_Wrap(ReadPixels,                void,      (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(Rectd,                     void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Rectdv,                    void,      (GLdoubleP, GLdoubleP))
BGL_Wrap(Rectf,                     void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Rectfv,                    void,      (GLfloatP, GLfloatP))
BGL_Wrap(Recti,                     void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(Rectiv,                    void,      (GLintP, GLintP))
BGL_Wrap(Rects,                     void,      (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Rectsv,                    void,      (GLshortP, GLshortP))
BGL_Wrap(RenderMode,                GLint,     (GLenum))
BGL_Wrap(Rotated,                   void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Rotatef,                   void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Scaled,                    void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Scalef,                    void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Scissor,                   void,      (GLint, GLint, GLsizei, GLsizei))
BGL_Wrap(SelectBuffer,              void,      (GLsizei, GLuintP))
BGL_Wrap(ShadeModel,                void,      (GLenum))
BGL_Wrap(StencilFunc,               void,      (GLenum, GLint, GLuint))
BGL_Wrap(StencilMask,               void,      (GLuint))
BGL_Wrap(StencilOp,                 void,      (GLenum, GLenum, GLenum))
BGL_Wrap(TexCoord1d,                void,      (GLdouble))
BGL_Wrap(TexCoord1dv,               void,      (GLdoubleP))
BGL_Wrap(TexCoord1f,                void,      (GLfloat))
BGL_Wrap(TexCoord1fv,               void,      (GLfloatP))
BGL_Wrap(TexCoord1i,                void,      (GLint))
BGL_Wrap(TexCoord1iv,               void,      (GLintP))
BGL_Wrap(TexCoord1s,                void,      (GLshort))
BGL_Wrap(TexCoord1sv,               void,      (GLshortP))
BGL_Wrap(TexCoord2d,                void,      (GLdouble, GLdouble))
BGL_Wrap(TexCoord2dv,               void,      (GLdoubleP))
BGL_Wrap(TexCoord2f,                void,      (GLfloat, GLfloat))
BGL_Wrap(TexCoord2fv,               void,      (GLfloatP))
BGL_Wrap(TexCoord2i,                void,      (GLint, GLint))
BGL_Wrap(TexCoord2iv,               void,      (GLintP))
BGL_Wrap(TexCoord2s,                void,      (GLshort, GLshort))
BGL_Wrap(TexCoord2sv,               void,      (GLshortP))
BGL_Wrap(TexCoord3d,                void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(TexCoord3dv,               void,      (GLdoubleP))
BGL_Wrap(TexCoord3f,                void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(TexCoord3fv,               void,      (GLfloatP))
BGL_Wrap(TexCoord3i,                void,      (GLint, GLint, GLint))
BGL_Wrap(TexCoord3iv,               void,      (GLintP))
BGL_Wrap(TexCoord3s,                void,      (GLshort, GLshort, GLshort))
BGL_Wrap(TexCoord3sv,               void,      (GLshortP))
BGL_Wrap(TexCoord4d,                void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(TexCoord4dv,               void,      (GLdoubleP))
BGL_Wrap(TexCoord4f,                void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(TexCoord4fv,               void,      (GLfloatP))
BGL_Wrap(TexCoord4i,                void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(TexCoord4iv,               void,      (GLintP))
BGL_Wrap(TexCoord4s,                void,      (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(TexCoord4sv,               void,      (GLshortP))
BGL_Wrap(TexEnvf,                   void,      (GLenum, GLenum, GLfloat))
BGL_Wrap(TexEnvfv,                  void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexEnvi,                   void,      (GLenum, GLenum, GLint))
BGL_Wrap(TexEnviv,                  void,      (GLenum, GLenum, GLintP))
BGL_Wrap(TexGend,                   void,      (GLenum, GLenum, GLdouble))
BGL_Wrap(TexGendv,                  void,      (GLenum, GLenum, GLdoubleP))
BGL_Wrap(TexGenf,                   void,      (GLenum, GLenum, GLfloat))
BGL_Wrap(TexGenfv,                  void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexGeni,                   void,      (GLenum, GLenum, GLint))
BGL_Wrap(TexGeniv,                  void,      (GLenum, GLenum, GLintP))
BGL_Wrap(TexImage1D,                void,      (GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexImage2D,                void,      (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexParameterf,             void,      (GLenum, GLenum, GLfloat))
BGL_Wrap(TexParameterfv,            void,      (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexParameteri,             void,      (GLenum, GLenum, GLint))
BGL_Wrap(TexParameteriv,            void,      (GLenum, GLenum, GLintP))
BGL_Wrap(Translated,                void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Translatef,                void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Vertex2d,                  void,      (GLdouble, GLdouble))
BGL_Wrap(Vertex2dv,                 void,      (GLdoubleP))
BGL_Wrap(Vertex2f,                  void,      (GLfloat, GLfloat))
BGL_Wrap(Vertex2fv,                 void,      (GLfloatP))
BGL_Wrap(Vertex2i,                  void,      (GLint, GLint))
BGL_Wrap(Vertex2iv,                 void,      (GLintP))
BGL_Wrap(Vertex2s,                  void,      (GLshort, GLshort))
BGL_Wrap(Vertex2sv,                 void,      (GLshortP))
BGL_Wrap(Vertex3d,                  void,      (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Vertex3dv,                 void,      (GLdoubleP))
BGL_Wrap(Vertex3f,                  void,      (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Vertex3fv,                 void,      (GLfloatP))
BGL_Wrap(Vertex3i,                  void,      (GLint, GLint, GLint))
BGL_Wrap(Vertex3iv,                 void,      (GLintP))
BGL_Wrap(Vertex3s,                  void,      (GLshort, GLshort, GLshort))
BGL_Wrap(Vertex3sv,                 void,      (GLshortP))
BGL_Wrap(Vertex4d,                  void,      (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Vertex4dv,                 void,      (GLdoubleP))
BGL_Wrap(Vertex4f,                  void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Vertex4fv,                 void,      (GLfloatP))
BGL_Wrap(Vertex4i,                  void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(Vertex4iv,                 void,      (GLintP))
BGL_Wrap(Vertex4s,                  void,      (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Vertex4sv,                 void,      (GLshortP))
BGL_Wrap(Viewport,                  void,      (GLint, GLint, GLsizei, GLsizei))


/* GL_VERSION_1_1 */
BGL_Wrap(AreTexturesResident,       GLboolean, (GLsizei, GLuintP, GLbooleanP))
BGL_Wrap(ArrayElement,              void,      (GLint))
BGL_Wrap(BindTexture,               void,      (GLenum, GLuint))
BGL_Wrap(ColorPointer,              void,      (GLint, GLenum, GLsizei, GLvoidP))
BGL_Wrap(CopyTexImage1D,            void,      (GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint))
BGL_Wrap(CopyTexImage2D,            void,      (GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint))
BGL_Wrap(CopyTexSubImage1D,         void,      (GLenum, GLint, GLint, GLint, GLint, GLsizei))
BGL_Wrap(CopyTexSubImage2D,         void,      (GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei))
BGL_Wrap(DeleteTextures,            void,      (GLsizei, GLuintP))
BGL_Wrap(DisableClientState,        void,      (GLenum))
BGL_Wrap(DrawArrays,                void,      (GLenum, GLint, GLsizei))
BGL_Wrap(DrawElements,              void,      (GLenum, GLsizei, GLenum, GLvoidP))
BGL_Wrap(EdgeFlagPointer,           void,      (GLsizei, GLvoidP))
BGL_Wrap(EnableClientState,         void,      (GLenum))
BGL_Wrap(GenTextures,               void,      (GLsizei, GLuintP))
BGL_Wrap(GetPointerv,               void,      (GLenum, GLvoidP))
BGL_Wrap(IndexPointer,              void,      (GLenum, GLsizei, GLvoidP))
BGL_Wrap(Indexub,                   void,      (GLubyte))
BGL_Wrap(Indexubv,                  void,      (GLubyteP))
BGL_Wrap(InterleavedArrays,         void,      (GLenum, GLsizei, GLvoidP))
BGL_Wrap(IsTexture,                 GLboolean, (GLuint))
BGL_Wrap(NormalPointer,             void,      (GLenum, GLsizei, GLvoidP))
BGL_Wrap(PolygonOffset,             void,      (GLfloat, GLfloat))
BGL_Wrap(PopClientAttrib,           void,      (void))
BGL_Wrap(PrioritizeTextures,        void,      (GLsizei, GLuintP, GLfloatP))
BGL_Wrap(PushClientAttrib,          void,      (GLbitfield))
BGL_Wrap(TexCoordPointer,           void,      (GLint, GLenum, GLsizei, GLvoidP))
BGL_Wrap(TexSubImage1D,             void,      (GLenum, GLint, GLint, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexSubImage2D,             void,      (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(VertexPointer,             void,      (GLint, GLenum, GLsizei, GLvoidP))


/* GL_VERSION_1_2 */
BGL_Wrap(CopyTexSubImage3D,         void,      (GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei))
BGL_Wrap(DrawRangeElements,         void,      (GLenum, GLuint, GLuint, GLsizei, GLenum, GLvoidP))
BGL_Wrap(TexImage3D,                void,      (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexSubImage3D,             void,      (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, GLvoidP))


/* GL_VERSION_1_3 */
BGL_Wrap(ActiveTexture,             void,      (GLenum))
BGL_Wrap(ClientActiveTexture,       void,      (GLenum))
BGL_Wrap(CompressedTexImage1D,      void,      (GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, GLvoidP))
BGL_Wrap(CompressedTexImage2D,      void,      (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, GLvoidP))
BGL_Wrap(CompressedTexImage3D,      void,      (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, GLvoidP))
BGL_Wrap(CompressedTexSubImage1D,   void,      (GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, GLvoidP))
BGL_Wrap(CompressedTexSubImage2D,   void,      (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, GLvoidP))
BGL_Wrap(CompressedTexSubImage3D,   void,      (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, GLvoidP))
BGL_Wrap(GetCompressedTexImage,     void,      (GLenum, GLint, GLvoidP))
BGL_Wrap(LoadTransposeMatrixd,      void,      (GLdoubleP))
BGL_Wrap(LoadTransposeMatrixf,      void,      (GLfloatP))
BGL_Wrap(MultTransposeMatrixd,      void,      (GLdoubleP))
BGL_Wrap(MultTransposeMatrixf,      void,      (GLfloatP))
BGL_Wrap(MultiTexCoord1d,           void,      (GLenum, GLdouble))
BGL_Wrap(MultiTexCoord1dv,          void,      (GLenum, GLdoubleP))
BGL_Wrap(MultiTexCoord1f,           void,      (GLenum, GLfloat))
BGL_Wrap(MultiTexCoord1fv,          void,      (GLenum, GLfloatP))
BGL_Wrap(MultiTexCoord1i,           void,      (GLenum, GLint))
BGL_Wrap(MultiTexCoord1iv,          void,      (GLenum, GLintP))
BGL_Wrap(MultiTexCoord1s,           void,      (GLenum, GLshort))
BGL_Wrap(MultiTexCoord1sv,          void,      (GLenum, GLshortP))
BGL_Wrap(MultiTexCoord2d,           void,      (GLenum, GLdouble, GLdouble))
BGL_Wrap(MultiTexCoord2dv,          void,      (GLenum, GLdoubleP))
BGL_Wrap(MultiTexCoord2f,           void,      (GLenum, GLfloat, GLfloat))
BGL_Wrap(MultiTexCoord2fv,          void,      (GLenum, GLfloatP))
BGL_Wrap(MultiTexCoord2i,           void,      (GLenum, GLint, GLint))
BGL_Wrap(MultiTexCoord2iv,          void,      (GLenum, GLintP))
BGL_Wrap(MultiTexCoord2s,           void,      (GLenum, GLshort, GLshort))
BGL_Wrap(MultiTexCoord2sv,          void,      (GLenum, GLshortP))
BGL_Wrap(MultiTexCoord3d,           void,      (GLenum, GLdouble, GLdouble, GLdouble))
BGL_Wrap(MultiTexCoord3dv,          void,      (GLenum, GLdoubleP))
BGL_Wrap(MultiTexCoord3f,           void,      (GLenum, GLfloat, GLfloat, GLfloat))
BGL_Wrap(MultiTexCoord3fv,          void,      (GLenum, GLfloatP))
BGL_Wrap(MultiTexCoord3i,           void,      (GLenum, GLint, GLint, GLint))
BGL_Wrap(MultiTexCoord3iv,          void,      (GLenum, GLintP))
BGL_Wrap(MultiTexCoord3s,           void,      (GLenum, GLshort, GLshort, GLshort))
BGL_Wrap(MultiTexCoord3sv,          void,      (GLenum, GLshortP))
BGL_Wrap(MultiTexCoord4d,           void,      (GLenum, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(MultiTexCoord4dv,          void,      (GLenum, GLdoubleP))
BGL_Wrap(MultiTexCoord4f,           void,      (GLenum, GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(MultiTexCoord4fv,          void,      (GLenum, GLfloatP))
BGL_Wrap(MultiTexCoord4i,           void,      (GLenum, GLint, GLint, GLint, GLint))
BGL_Wrap(MultiTexCoord4iv,          void,      (GLenum, GLintP))
BGL_Wrap(MultiTexCoord4s,           void,      (GLenum, GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(MultiTexCoord4sv,          void,      (GLenum, GLshortP))
BGL_Wrap(SampleCoverage,            void,      (GLfloat, GLboolean))


/* GL_VERSION_1_4 */
BGL_Wrap(BlendColor,                void,      (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(BlendEquation,             void,      (GLenum))


/* GL_VERSION_1_5 */
BGL_Wrap(BeginQuery,                void,      (GLenum, GLuint))
BGL_Wrap(BindBuffer,                void,      (GLenum, GLuint))
BGL_Wrap(BufferData,                void,      (GLenum, GLsizeiptr, GLvoidP, GLenum))
BGL_Wrap(BufferSubData,             void,      (GLenum, GLintptr, GLsizeiptr, GLvoidP))
BGL_Wrap(DeleteBuffers,             void,      (GLsizei, GLuintP))
BGL_Wrap(DeleteQueries,             void,      (GLsizei, GLuintP))
BGL_Wrap(EndQuery,                  void,      (GLenum))
BGL_Wrap(GenBuffers,                void,      (GLsizei, GLuintP))
BGL_Wrap(GenQueries,                void,      (GLsizei, GLuintP))
BGL_Wrap(GetBufferParameteriv,      void,      (GLenum, GLenum, GLintP))
BGL_Wrap(GetBufferPointerv,         void,      (GLenum, GLenum, GLvoidP))
BGL_Wrap(GetBufferSubData,          void,      (GLenum, GLintptr, GLsizeiptr, GLvoidP))
BGL_Wrap(GetQueryObjectiv,          void,      (GLuint, GLenum, GLintP))
BGL_Wrap(GetQueryObjectuiv,         void,      (GLuint, GLenum, GLuintP))
BGL_Wrap(GetQueryiv,                void,      (GLenum, GLenum, GLintP))
BGL_Wrap(IsBuffer,                  GLboolean, (GLuint))
BGL_Wrap(IsQuery,                   GLboolean, (GLuint))
BGL_Wrap(MapBuffer,                 void,      (GLenum, GLenum))
BGL_Wrap(UnmapBuffer,               GLboolean, (GLenum))


/* GL_VERSION_2_0 */
BGL_Wrap(AttachShader,              void,      (GLuint, GLuint))
BGL_Wrap(BindAttribLocation,        void,      (GLuint, GLuint, GLstring))
BGL_Wrap(BlendEquationSeparate,     void,      (GLenum, GLenum))
BGL_Wrap(CompileShader,             void,      (GLuint))
BGL_Wrap(CreateProgram,             GLuint,    (void))
BGL_Wrap(CreateShader,              GLuint,    (GLenum))
BGL_Wrap(DeleteProgram,             void,      (GLuint))
BGL_Wrap(DeleteShader,              void,      (GLuint))
BGL_Wrap(DetachShader,              void,      (GLuint, GLuint))
BGL_Wrap(DisableVertexAttribArray,  void,      (GLuint))
BGL_Wrap(DrawBuffers,               void,      (GLsizei, GLenumP))
BGL_Wrap(EnableVertexAttribArray,   void,      (GLuint))
BGL_Wrap(GetActiveAttrib,           void,      (GLuint, GLuint, GLsizei, GLsizeiP, GLintP, GLenumP, GLcharP))
BGL_Wrap(GetActiveUniform,          void,      (GLuint, GLuint, GLsizei, GLsizeiP, GLintP, GLenumP, GLcharP))
BGL_Wrap(GetAttachedShaders,        void,      (GLuint, GLsizei, GLsizeiP, GLuintP))
BGL_Wrap(GetAttribLocation,         GLint,     (GLuint, GLstring))
BGL_Wrap(GetProgramInfoLog,         void,      (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetProgramiv,              void,      (GLuint, GLenum, GLintP))
BGL_Wrap(GetShaderInfoLog,          void,      (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetShaderSource,           void,      (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetShaderiv,               void,      (GLuint, GLenum, GLintP))
BGL_Wrap(GetUniformLocation,        GLint,     (GLuint, GLstring))
BGL_Wrap(GetUniformfv,              void,      (GLuint, GLint, GLfloatP))
BGL_Wrap(GetUniformiv,              void,      (GLuint, GLint, GLintP))
BGL_Wrap(GetVertexAttribPointerv,   void,      (GLuint, GLenum, GLvoidP))
BGL_Wrap(GetVertexAttribdv,         void,      (GLuint, GLenum, GLdoubleP))
BGL_Wrap(GetVertexAttribfv,         void,      (GLuint, GLenum, GLfloatP))
BGL_Wrap(GetVertexAttribiv,         void,      (GLuint, GLenum, GLintP))
BGL_Wrap(IsProgram,                 GLboolean, (GLuint))
BGL_Wrap(IsShader,                  GLboolean, (GLuint))
BGL_Wrap(LinkProgram,               void,      (GLuint))
BGL_Wrap(StencilFuncSeparate,       void,      (GLenum, GLenum, GLint, GLuint))
BGL_Wrap(StencilMaskSeparate,       void,      (GLenum, GLuint))
BGL_Wrap(StencilOpSeparate,         void,      (GLenum, GLenum, GLenum, GLenum))
BGL_Wrap(Uniform1f,                 void,      (GLint, GLfloat))
BGL_Wrap(Uniform1fv,                void,      (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform1i,                 void,      (GLint, GLint))
BGL_Wrap(Uniform1iv,                void,      (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform2f,                 void,      (GLint, GLfloat, GLfloat))
BGL_Wrap(Uniform2fv,                void,      (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform2i,                 void,      (GLint, GLint, GLint))
BGL_Wrap(Uniform2iv,                void,      (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform3f,                 void,      (GLint, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Uniform3fv,                void,      (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform3i,                 void,      (GLint, GLint, GLint, GLint))
BGL_Wrap(Uniform3iv,                void,      (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform4f,                 void,      (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Uniform4fv,                void,      (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform4i,                 void,      (GLint, GLint, GLint, GLint, GLint))
BGL_Wrap(Uniform4iv,                void,      (GLint, GLsizei, GLintP))
BGL_Wrap(UniformMatrix2fv,          void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3fv,          void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4fv,          void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UseProgram,                void,      (GLuint))
BGL_Wrap(ValidateProgram,           void,      (GLuint))
BGL_Wrap(VertexAttrib1d,            void,      (GLuint, GLdouble))
BGL_Wrap(VertexAttrib1dv,           void,      (GLuint, GLdoubleP))
BGL_Wrap(VertexAttrib1f,            void,      (GLuint, GLfloat))
BGL_Wrap(VertexAttrib1fv,           void,      (GLuint, GLfloatP))
BGL_Wrap(VertexAttrib1s,            void,      (GLuint, GLshort))
BGL_Wrap(VertexAttrib1sv,           void,      (GLuint, GLshortP))
BGL_Wrap(VertexAttrib2d,            void,      (GLuint, GLdouble, GLdouble))
BGL_Wrap(VertexAttrib2dv,           void,      (GLuint, GLdoubleP))
BGL_Wrap(VertexAttrib2f,            void,      (GLuint, GLfloat, GLfloat))
BGL_Wrap(VertexAttrib2fv,           void,      (GLuint, GLfloatP))
BGL_Wrap(VertexAttrib2s,            void,      (GLuint, GLshort, GLshort))
BGL_Wrap(VertexAttrib2sv,           void,      (GLuint, GLshortP))
BGL_Wrap(VertexAttrib3d,            void,      (GLuint, GLdouble, GLdouble, GLdouble))
BGL_Wrap(VertexAttrib3dv,           void,      (GLuint, GLdoubleP))
BGL_Wrap(VertexAttrib3f,            void,      (GLuint, GLfloat, GLfloat, GLfloat))
BGL_Wrap(VertexAttrib3fv,           void,      (GLuint, GLfloatP))
BGL_Wrap(VertexAttrib3s,            void,      (GLuint, GLshort, GLshort, GLshort))
BGL_Wrap(VertexAttrib3sv,           void,      (GLuint, GLshortP))
BGL_Wrap(VertexAttrib4Nbv,          void,      (GLuint, GLbyteP))
BGL_Wrap(VertexAttrib4Niv,          void,      (GLuint, GLintP))
BGL_Wrap(VertexAttrib4Nsv,          void,      (GLuint, GLshortP))
BGL_Wrap(VertexAttrib4Nub,          void,      (GLuint, GLubyte, GLubyte, GLubyte, GLubyte))
BGL_Wrap(VertexAttrib4Nubv,         void,      (GLuint, GLubyteP))
BGL_Wrap(VertexAttrib4Nuiv,         void,      (GLuint, GLuintP))
BGL_Wrap(VertexAttrib4Nusv,         void,      (GLuint, GLushortP))
BGL_Wrap(VertexAttrib4bv,           void,      (GLuint, GLbyteP))
BGL_Wrap(VertexAttrib4d,            void,      (GLuint, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(VertexAttrib4dv,           void,      (GLuint, GLdoubleP))
BGL_Wrap(VertexAttrib4f,            void,      (GLuint, GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(VertexAttrib4fv,           void,      (GLuint, GLfloatP))
BGL_Wrap(VertexAttrib4iv,           void,      (GLuint, GLintP))
BGL_Wrap(VertexAttrib4s,            void,      (GLuint, GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(VertexAttrib4sv,           void,      (GLuint, GLshortP))
BGL_Wrap(VertexAttrib4ubv,          void,      (GLuint, GLubyteP))
BGL_Wrap(VertexAttrib4uiv,          void,      (GLuint, GLuintP))
BGL_Wrap(VertexAttrib4usv,          void,      (GLuint, GLushortP))
BGL_Wrap(VertexAttribPointer,       void,      (GLuint, GLint, GLenum, GLboolean, GLsizei, GLvoidP))


/* GL_VERSION_2_1 */
BGL_Wrap(UniformMatrix2x3fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix2x4fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3x2fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3x4fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4x2fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4x3fv,        void,      (GLint, GLsizei, GLboolean, GLfloatP))


/* GL_VERSION_3_0 */
BGL_Wrap(BindVertexArray,           void,      (GLuint))
BGL_Wrap(DeleteVertexArrays,        void,      (GLsizei, GLuintP))
BGL_Wrap(GenVertexArrays,           void,      (GLsizei, GLuintP))
BGL_Wrap(IsVertexArray,             GLboolean, (GLuint))


/* GL_VERSION_3_1 */
BGL_Wrap(BindBufferBase,            void,      (GLenum, GLuint, GLuint))
BGL_Wrap(BindBufferRange,           void,      (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr))
BGL_Wrap(GetActiveUniformBlockName, void,      (GLuint, GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetActiveUniformBlockiv,   void,      (GLuint, GLuint, GLenum, GLintP))
BGL_Wrap(GetActiveUniformName,      void,      (GLuint, GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetActiveUniformsiv,       void,      (GLuint, GLsizei, GLuintP, GLenum, GLintP))
BGL_Wrap(GetIntegeri_v,             void,      (GLenum, GLuint, GLintP))
BGL_Wrap(GetUniformBlockIndex,      GLuint,    (GLuint, GLstring))
BGL_Wrap(GetUniformIndices,         void,      (GLuint, GLsizei, GLcharP, GLuintP))
BGL_Wrap(UniformBlockBinding,       void,      (GLuint, GLuint, GLuint))


/* GL_VERSION_3_2 */
BGL_Wrap(FramebufferTexture,        void,      (GLenum, GLenum, GLuint, GLint))
BGL_Wrap(GetBufferParameteri64v,    void,      (GLenum, GLenum, GLint64P))
BGL_Wrap(GetInteger64i_v,           void,      (GLenum, GLuint, GLint64P))
BGL_Wrap(GetMultisamplefv,          void,      (GLenum, GLuint, GLfloatP))
BGL_Wrap(SampleMaski,               void,      (GLuint, GLbitfield))
BGL_Wrap(TexImage2DMultisample,     void,      (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean))
BGL_Wrap(TexImage3DMultisample,     void,      (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean))


/* GL_VERSION_3_3 */
BGL_Wrap(ColorP3ui,                 void,      (GLenum, GLuint))
BGL_Wrap(ColorP3uiv,                void,      (GLenum, GLuintP))
BGL_Wrap(ColorP4ui,                 void,      (GLenum, GLuint))
BGL_Wrap(ColorP4uiv,                void,      (GLenum, GLuintP))
BGL_Wrap(MultiTexCoordP1ui,         void,      (GLenum, GLenum, GLuint))
BGL_Wrap(MultiTexCoordP1uiv,        void,      (GLenum, GLenum, GLuintP))
BGL_Wrap(MultiTexCoordP2ui,         void,      (GLenum, GLenum, GLuint))
BGL_Wrap(MultiTexCoordP2uiv,        void,      (GLenum, GLenum, GLuintP))
BGL_Wrap(MultiTexCoordP3ui,         void,      (GLenum, GLenum, GLuint))
BGL_Wrap(MultiTexCoordP3uiv,        void,      (GLenum, GLenum, GLuintP))
BGL_Wrap(MultiTexCoordP4ui,         void,      (GLenum, GLenum, GLuint))
BGL_Wrap(MultiTexCoordP4uiv,        void,      (GLenum, GLenum, GLuintP))
BGL_Wrap(NormalP3ui,                void,      (GLenum, GLuint))
BGL_Wrap(NormalP3uiv,               void,      (GLenum, GLuintP))
BGL_Wrap(SecondaryColorP3ui,        void,      (GLenum, GLuint))
BGL_Wrap(SecondaryColorP3uiv,       void,      (GLenum, GLuintP))
BGL_Wrap(TexCoordP1ui,              void,      (GLenum, GLuint))
BGL_Wrap(TexCoordP1uiv,             void,      (GLenum, GLuintP))
BGL_Wrap(TexCoordP2ui,              void,      (GLenum, GLuint))
BGL_Wrap(TexCoordP2uiv,             void,      (GLenum, GLuintP))
BGL_Wrap(TexCoordP3ui,              void,      (GLenum, GLuint))
BGL_Wrap(TexCoordP3uiv,             void,      (GLenum, GLuintP))
BGL_Wrap(TexCoordP4ui,              void,      (GLenum, GLuint))
BGL_Wrap(TexCoordP4uiv,             void,      (GLenum, GLuintP))
BGL_Wrap(VertexP2ui,                void,      (GLenum, GLuint))
BGL_Wrap(VertexP2uiv,               void,      (GLenum, GLuintP))
BGL_Wrap(VertexP3ui,                void,      (GLenum, GLuint))
BGL_Wrap(VertexP3uiv,               void,      (GLenum, GLuintP))
BGL_Wrap(VertexP4ui,                void,      (GLenum, GLuint))
BGL_Wrap(VertexP4uiv,               void,      (GLenum, GLuintP))


BGLU_Wrap(Perspective,       void,       (GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(LookAt,            void,       (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(Ortho2D,           void,       (GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(PickMatrix,        void,       (GLdouble, GLdouble, GLdouble, GLdouble, GLintP))
BGLU_Wrap(Project,           GLint,      (GLdouble, GLdouble, GLdouble, GLdoubleP, GLdoubleP, GLintP, GLdoubleP, GLdoubleP, GLdoubleP))
BGLU_Wrap(UnProject,         GLint,      (GLdouble, GLdouble, GLdouble, GLdoubleP, GLdoubleP, GLintP, GLdoubleP, GLdoubleP, GLdoubleP))

/** \} */


/* -------------------------------------------------------------------- */

/** \name Module Definition
 * \{ */

#define MethodDefu(func) {"glu"#func, Method_##func, METH_VARARGS, NULL}

static struct PyMethodDef BGL_methods[] = {
	MethodDefu(Perspective),
	MethodDefu(LookAt),
	MethodDefu(Ortho2D),
	MethodDefu(PickMatrix),
	MethodDefu(Project),
	MethodDefu(UnProject),
	{NULL, NULL, 0, NULL}
};

#undef MethodDefu

static struct PyModuleDef BGL_module_def = {
	PyModuleDef_HEAD_INIT,
	"bgl",  /* m_name */
	NULL,  /* m_doc */
	0,  /* m_size */
	BGL_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

static void py_module_dict_add_int(PyObject *dict, const char *name, int value)
{
	PyObject *item;
	PyDict_SetItemString(dict, name, item = PyLong_FromLong(value));
	Py_DECREF(item);
}

static void py_module_dict_add_int64(PyObject *dict, const char *name, int64_t value)
{
	PyObject *item;
	PyDict_SetItemString(dict, name, item = PyLong_FromLongLong(value));
	Py_DECREF(item);
}

static void py_module_dict_add_method(PyObject *submodule, PyObject *dict, PyMethodDef *method_def, bool is_valid)
{
	if (is_valid) {
		PyObject *m;
		m = PyCFunction_NewEx(method_def, NULL, submodule);
		PyDict_SetItemString(dict, method_def->ml_name, m);
		Py_DECREF(m);
	}
	else {
		PyDict_SetItemString(dict, method_def->ml_name, Py_None);
	}
}

/* TODO, expose to users */
static bool use_deprecated = true;

PyObject *BPyInit_bgl(void)
{
	PyObject *submodule, *dict;
	submodule = PyModule_Create(&BGL_module_def);
	dict = PyModule_GetDict(submodule);

	if (PyType_Ready(&BGL_bufferType) < 0)
		return NULL;  /* should never happen */

	PyModule_AddObject(submodule, "Buffer", (PyObject *)&BGL_bufferType);
	Py_INCREF((PyObject *)&BGL_bufferType);

/* needed since some function pointers won't be NULL */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Waddress"
#endif

#define PY_MOD_ADD_METHOD(func) \
	{ \
		static PyMethodDef method_def = {"gl"#func, Method_##func, METH_VARARGS}; \
		py_module_dict_add_method(submodule, dict, &method_def, (gl##func != NULL)); \
	} ((void)0)

	/* GL_VERSION_1_0 */
	{
		PY_MOD_ADD_METHOD(BlendFunc);
		PY_MOD_ADD_METHOD(Clear);
		PY_MOD_ADD_METHOD(ClearColor);
		PY_MOD_ADD_METHOD(ClearDepth);
		PY_MOD_ADD_METHOD(ClearStencil);
		PY_MOD_ADD_METHOD(ColorMask);
		PY_MOD_ADD_METHOD(CullFace);
		PY_MOD_ADD_METHOD(DepthFunc);
		PY_MOD_ADD_METHOD(DepthMask);
		PY_MOD_ADD_METHOD(DepthRange);
		PY_MOD_ADD_METHOD(Disable);
		PY_MOD_ADD_METHOD(DrawBuffer);
		PY_MOD_ADD_METHOD(Enable);
		PY_MOD_ADD_METHOD(Finish);
		PY_MOD_ADD_METHOD(Flush);
		PY_MOD_ADD_METHOD(FrontFace);
		PY_MOD_ADD_METHOD(GetBooleanv);
		PY_MOD_ADD_METHOD(GetDoublev);
		PY_MOD_ADD_METHOD(GetError);
		PY_MOD_ADD_METHOD(GetFloatv);
		PY_MOD_ADD_METHOD(GetIntegerv);
		PY_MOD_ADD_METHOD(GetString);
		PY_MOD_ADD_METHOD(GetTexImage);
		PY_MOD_ADD_METHOD(GetTexLevelParameterfv);
		PY_MOD_ADD_METHOD(GetTexLevelParameteriv);
		PY_MOD_ADD_METHOD(GetTexParameterfv);
		PY_MOD_ADD_METHOD(GetTexParameteriv);
		PY_MOD_ADD_METHOD(Hint);
		PY_MOD_ADD_METHOD(IsEnabled);
		PY_MOD_ADD_METHOD(LineWidth);
		PY_MOD_ADD_METHOD(LogicOp);
		PY_MOD_ADD_METHOD(PixelStoref);
		PY_MOD_ADD_METHOD(PixelStorei);
		PY_MOD_ADD_METHOD(PointSize);
		PY_MOD_ADD_METHOD(PolygonMode);
		PY_MOD_ADD_METHOD(ReadBuffer);
		PY_MOD_ADD_METHOD(ReadPixels);
		PY_MOD_ADD_METHOD(Scissor);
		PY_MOD_ADD_METHOD(StencilFunc);
		PY_MOD_ADD_METHOD(StencilMask);
		PY_MOD_ADD_METHOD(StencilOp);
		PY_MOD_ADD_METHOD(TexImage1D);
		PY_MOD_ADD_METHOD(TexImage2D);
		PY_MOD_ADD_METHOD(TexParameterf);
		PY_MOD_ADD_METHOD(TexParameterfv);
		PY_MOD_ADD_METHOD(TexParameteri);
		PY_MOD_ADD_METHOD(TexParameteriv);
		PY_MOD_ADD_METHOD(Viewport);
	}
	/* adding in GL_VERSION_1_0 removed from core profile */
	if (use_deprecated == true) {
		PY_MOD_ADD_METHOD(Accum);
		PY_MOD_ADD_METHOD(AlphaFunc);
		PY_MOD_ADD_METHOD(Begin);
		PY_MOD_ADD_METHOD(Bitmap);
		PY_MOD_ADD_METHOD(CallList);
		PY_MOD_ADD_METHOD(CallLists);
		PY_MOD_ADD_METHOD(ClearAccum);
		PY_MOD_ADD_METHOD(ClearIndex);
		PY_MOD_ADD_METHOD(ClipPlane);
		PY_MOD_ADD_METHOD(Color3b);
		PY_MOD_ADD_METHOD(Color3bv);
		PY_MOD_ADD_METHOD(Color3d);
		PY_MOD_ADD_METHOD(Color3dv);
		PY_MOD_ADD_METHOD(Color3f);
		PY_MOD_ADD_METHOD(Color3fv);
		PY_MOD_ADD_METHOD(Color3i);
		PY_MOD_ADD_METHOD(Color3iv);
		PY_MOD_ADD_METHOD(Color3s);
		PY_MOD_ADD_METHOD(Color3sv);
		PY_MOD_ADD_METHOD(Color3ub);
		PY_MOD_ADD_METHOD(Color3ubv);
		PY_MOD_ADD_METHOD(Color3ui);
		PY_MOD_ADD_METHOD(Color3uiv);
		PY_MOD_ADD_METHOD(Color3us);
		PY_MOD_ADD_METHOD(Color3usv);
		PY_MOD_ADD_METHOD(Color4b);
		PY_MOD_ADD_METHOD(Color4bv);
		PY_MOD_ADD_METHOD(Color4d);
		PY_MOD_ADD_METHOD(Color4dv);
		PY_MOD_ADD_METHOD(Color4f);
		PY_MOD_ADD_METHOD(Color4fv);
		PY_MOD_ADD_METHOD(Color4i);
		PY_MOD_ADD_METHOD(Color4iv);
		PY_MOD_ADD_METHOD(Color4s);
		PY_MOD_ADD_METHOD(Color4sv);
		PY_MOD_ADD_METHOD(Color4ub);
		PY_MOD_ADD_METHOD(Color4ubv);
		PY_MOD_ADD_METHOD(Color4ui);
		PY_MOD_ADD_METHOD(Color4uiv);
		PY_MOD_ADD_METHOD(Color4us);
		PY_MOD_ADD_METHOD(Color4usv);
		PY_MOD_ADD_METHOD(ColorMaterial);
		PY_MOD_ADD_METHOD(CopyPixels);
		PY_MOD_ADD_METHOD(DeleteLists);
		PY_MOD_ADD_METHOD(DrawPixels);
		PY_MOD_ADD_METHOD(EdgeFlag);
		PY_MOD_ADD_METHOD(EdgeFlagv);
		PY_MOD_ADD_METHOD(End);
		PY_MOD_ADD_METHOD(EndList);
		PY_MOD_ADD_METHOD(EvalCoord1d);
		PY_MOD_ADD_METHOD(EvalCoord1dv);
		PY_MOD_ADD_METHOD(EvalCoord1f);
		PY_MOD_ADD_METHOD(EvalCoord1fv);
		PY_MOD_ADD_METHOD(EvalCoord2d);
		PY_MOD_ADD_METHOD(EvalCoord2dv);
		PY_MOD_ADD_METHOD(EvalCoord2f);
		PY_MOD_ADD_METHOD(EvalCoord2fv);
		PY_MOD_ADD_METHOD(EvalMesh1);
		PY_MOD_ADD_METHOD(EvalMesh2);
		PY_MOD_ADD_METHOD(EvalPoint1);
		PY_MOD_ADD_METHOD(EvalPoint2);
		PY_MOD_ADD_METHOD(FeedbackBuffer);
		PY_MOD_ADD_METHOD(Fogf);
		PY_MOD_ADD_METHOD(Fogfv);
		PY_MOD_ADD_METHOD(Fogi);
		PY_MOD_ADD_METHOD(Fogiv);
		PY_MOD_ADD_METHOD(Frustum);
		PY_MOD_ADD_METHOD(GenLists);
		PY_MOD_ADD_METHOD(GetClipPlane);
		PY_MOD_ADD_METHOD(GetLightfv);
		PY_MOD_ADD_METHOD(GetLightiv);
		PY_MOD_ADD_METHOD(GetMapdv);
		PY_MOD_ADD_METHOD(GetMapfv);
		PY_MOD_ADD_METHOD(GetMapiv);
		PY_MOD_ADD_METHOD(GetMaterialfv);
		PY_MOD_ADD_METHOD(GetMaterialiv);
		PY_MOD_ADD_METHOD(GetPixelMapfv);
		PY_MOD_ADD_METHOD(GetPixelMapuiv);
		PY_MOD_ADD_METHOD(GetPixelMapusv);
		PY_MOD_ADD_METHOD(GetPolygonStipple);
		PY_MOD_ADD_METHOD(GetTexEnvfv);
		PY_MOD_ADD_METHOD(GetTexEnviv);
		PY_MOD_ADD_METHOD(GetTexGendv);
		PY_MOD_ADD_METHOD(GetTexGenfv);
		PY_MOD_ADD_METHOD(GetTexGeniv);
		PY_MOD_ADD_METHOD(IndexMask);
		PY_MOD_ADD_METHOD(Indexd);
		PY_MOD_ADD_METHOD(Indexdv);
		PY_MOD_ADD_METHOD(Indexf);
		PY_MOD_ADD_METHOD(Indexfv);
		PY_MOD_ADD_METHOD(Indexi);
		PY_MOD_ADD_METHOD(Indexiv);
		PY_MOD_ADD_METHOD(Indexs);
		PY_MOD_ADD_METHOD(Indexsv);
		PY_MOD_ADD_METHOD(InitNames);
		PY_MOD_ADD_METHOD(IsList);
		PY_MOD_ADD_METHOD(LightModelf);
		PY_MOD_ADD_METHOD(LightModelfv);
		PY_MOD_ADD_METHOD(LightModeli);
		PY_MOD_ADD_METHOD(LightModeliv);
		PY_MOD_ADD_METHOD(Lightf);
		PY_MOD_ADD_METHOD(Lightfv);
		PY_MOD_ADD_METHOD(Lighti);
		PY_MOD_ADD_METHOD(Lightiv);
		PY_MOD_ADD_METHOD(LineStipple);
		PY_MOD_ADD_METHOD(ListBase);
		PY_MOD_ADD_METHOD(LoadIdentity);
		PY_MOD_ADD_METHOD(LoadMatrixd);
		PY_MOD_ADD_METHOD(LoadMatrixf);
		PY_MOD_ADD_METHOD(LoadName);
		PY_MOD_ADD_METHOD(Map1d);
		PY_MOD_ADD_METHOD(Map1f);
		PY_MOD_ADD_METHOD(Map2d);
		PY_MOD_ADD_METHOD(Map2f);
		PY_MOD_ADD_METHOD(MapGrid1d);
		PY_MOD_ADD_METHOD(MapGrid1f);
		PY_MOD_ADD_METHOD(MapGrid2d);
		PY_MOD_ADD_METHOD(MapGrid2f);
		PY_MOD_ADD_METHOD(Materialf);
		PY_MOD_ADD_METHOD(Materialfv);
		PY_MOD_ADD_METHOD(Materiali);
		PY_MOD_ADD_METHOD(Materialiv);
		PY_MOD_ADD_METHOD(MatrixMode);
		PY_MOD_ADD_METHOD(MultMatrixd);
		PY_MOD_ADD_METHOD(MultMatrixf);
		PY_MOD_ADD_METHOD(NewList);
		PY_MOD_ADD_METHOD(Normal3b);
		PY_MOD_ADD_METHOD(Normal3bv);
		PY_MOD_ADD_METHOD(Normal3d);
		PY_MOD_ADD_METHOD(Normal3dv);
		PY_MOD_ADD_METHOD(Normal3f);
		PY_MOD_ADD_METHOD(Normal3fv);
		PY_MOD_ADD_METHOD(Normal3i);
		PY_MOD_ADD_METHOD(Normal3iv);
		PY_MOD_ADD_METHOD(Normal3s);
		PY_MOD_ADD_METHOD(Normal3sv);
		PY_MOD_ADD_METHOD(Ortho);
		PY_MOD_ADD_METHOD(PassThrough);
		PY_MOD_ADD_METHOD(PixelMapfv);
		PY_MOD_ADD_METHOD(PixelMapuiv);
		PY_MOD_ADD_METHOD(PixelMapusv);
		PY_MOD_ADD_METHOD(PixelTransferf);
		PY_MOD_ADD_METHOD(PixelTransferi);
		PY_MOD_ADD_METHOD(PixelZoom);
		PY_MOD_ADD_METHOD(PolygonStipple);
		PY_MOD_ADD_METHOD(PopAttrib);
		PY_MOD_ADD_METHOD(PopMatrix);
		PY_MOD_ADD_METHOD(PopName);
		PY_MOD_ADD_METHOD(PushAttrib);
		PY_MOD_ADD_METHOD(PushMatrix);
		PY_MOD_ADD_METHOD(PushName);
		PY_MOD_ADD_METHOD(RasterPos2d);
		PY_MOD_ADD_METHOD(RasterPos2dv);
		PY_MOD_ADD_METHOD(RasterPos2f);
		PY_MOD_ADD_METHOD(RasterPos2fv);
		PY_MOD_ADD_METHOD(RasterPos2i);
		PY_MOD_ADD_METHOD(RasterPos2iv);
		PY_MOD_ADD_METHOD(RasterPos2s);
		PY_MOD_ADD_METHOD(RasterPos2sv);
		PY_MOD_ADD_METHOD(RasterPos3d);
		PY_MOD_ADD_METHOD(RasterPos3dv);
		PY_MOD_ADD_METHOD(RasterPos3f);
		PY_MOD_ADD_METHOD(RasterPos3fv);
		PY_MOD_ADD_METHOD(RasterPos3i);
		PY_MOD_ADD_METHOD(RasterPos3iv);
		PY_MOD_ADD_METHOD(RasterPos3s);
		PY_MOD_ADD_METHOD(RasterPos3sv);
		PY_MOD_ADD_METHOD(RasterPos4d);
		PY_MOD_ADD_METHOD(RasterPos4dv);
		PY_MOD_ADD_METHOD(RasterPos4f);
		PY_MOD_ADD_METHOD(RasterPos4fv);
		PY_MOD_ADD_METHOD(RasterPos4i);
		PY_MOD_ADD_METHOD(RasterPos4iv);
		PY_MOD_ADD_METHOD(RasterPos4s);
		PY_MOD_ADD_METHOD(RasterPos4sv);
		PY_MOD_ADD_METHOD(Rectd);
		PY_MOD_ADD_METHOD(Rectdv);
		PY_MOD_ADD_METHOD(Rectf);
		PY_MOD_ADD_METHOD(Rectfv);
		PY_MOD_ADD_METHOD(Recti);
		PY_MOD_ADD_METHOD(Rectiv);
		PY_MOD_ADD_METHOD(Rects);
		PY_MOD_ADD_METHOD(Rectsv);
		PY_MOD_ADD_METHOD(RenderMode);
		PY_MOD_ADD_METHOD(Rotated);
		PY_MOD_ADD_METHOD(Rotatef);
		PY_MOD_ADD_METHOD(Scaled);
		PY_MOD_ADD_METHOD(Scalef);
		PY_MOD_ADD_METHOD(SelectBuffer);
		PY_MOD_ADD_METHOD(ShadeModel);
		PY_MOD_ADD_METHOD(TexCoord1d);
		PY_MOD_ADD_METHOD(TexCoord1dv);
		PY_MOD_ADD_METHOD(TexCoord1f);
		PY_MOD_ADD_METHOD(TexCoord1fv);
		PY_MOD_ADD_METHOD(TexCoord1i);
		PY_MOD_ADD_METHOD(TexCoord1iv);
		PY_MOD_ADD_METHOD(TexCoord1s);
		PY_MOD_ADD_METHOD(TexCoord1sv);
		PY_MOD_ADD_METHOD(TexCoord2d);
		PY_MOD_ADD_METHOD(TexCoord2dv);
		PY_MOD_ADD_METHOD(TexCoord2f);
		PY_MOD_ADD_METHOD(TexCoord2fv);
		PY_MOD_ADD_METHOD(TexCoord2i);
		PY_MOD_ADD_METHOD(TexCoord2iv);
		PY_MOD_ADD_METHOD(TexCoord2s);
		PY_MOD_ADD_METHOD(TexCoord2sv);
		PY_MOD_ADD_METHOD(TexCoord3d);
		PY_MOD_ADD_METHOD(TexCoord3dv);
		PY_MOD_ADD_METHOD(TexCoord3f);
		PY_MOD_ADD_METHOD(TexCoord3fv);
		PY_MOD_ADD_METHOD(TexCoord3i);
		PY_MOD_ADD_METHOD(TexCoord3iv);
		PY_MOD_ADD_METHOD(TexCoord3s);
		PY_MOD_ADD_METHOD(TexCoord3sv);
		PY_MOD_ADD_METHOD(TexCoord4d);
		PY_MOD_ADD_METHOD(TexCoord4dv);
		PY_MOD_ADD_METHOD(TexCoord4f);
		PY_MOD_ADD_METHOD(TexCoord4fv);
		PY_MOD_ADD_METHOD(TexCoord4i);
		PY_MOD_ADD_METHOD(TexCoord4iv);
		PY_MOD_ADD_METHOD(TexCoord4s);
		PY_MOD_ADD_METHOD(TexCoord4sv);
		PY_MOD_ADD_METHOD(TexEnvf);
		PY_MOD_ADD_METHOD(TexEnvfv);
		PY_MOD_ADD_METHOD(TexEnvi);
		PY_MOD_ADD_METHOD(TexEnviv);
		PY_MOD_ADD_METHOD(TexGend);
		PY_MOD_ADD_METHOD(TexGendv);
		PY_MOD_ADD_METHOD(TexGenf);
		PY_MOD_ADD_METHOD(TexGenfv);
		PY_MOD_ADD_METHOD(TexGeni);
		PY_MOD_ADD_METHOD(TexGeniv);
		PY_MOD_ADD_METHOD(Translated);
		PY_MOD_ADD_METHOD(Translatef);
		PY_MOD_ADD_METHOD(Vertex2d);
		PY_MOD_ADD_METHOD(Vertex2dv);
		PY_MOD_ADD_METHOD(Vertex2f);
		PY_MOD_ADD_METHOD(Vertex2fv);
		PY_MOD_ADD_METHOD(Vertex2i);
		PY_MOD_ADD_METHOD(Vertex2iv);
		PY_MOD_ADD_METHOD(Vertex2s);
		PY_MOD_ADD_METHOD(Vertex2sv);
		PY_MOD_ADD_METHOD(Vertex3d);
		PY_MOD_ADD_METHOD(Vertex3dv);
		PY_MOD_ADD_METHOD(Vertex3f);
		PY_MOD_ADD_METHOD(Vertex3fv);
		PY_MOD_ADD_METHOD(Vertex3i);
		PY_MOD_ADD_METHOD(Vertex3iv);
		PY_MOD_ADD_METHOD(Vertex3s);
		PY_MOD_ADD_METHOD(Vertex3sv);
		PY_MOD_ADD_METHOD(Vertex4d);
		PY_MOD_ADD_METHOD(Vertex4dv);
		PY_MOD_ADD_METHOD(Vertex4f);
		PY_MOD_ADD_METHOD(Vertex4fv);
		PY_MOD_ADD_METHOD(Vertex4i);
		PY_MOD_ADD_METHOD(Vertex4iv);
		PY_MOD_ADD_METHOD(Vertex4s);
		PY_MOD_ADD_METHOD(Vertex4sv);
	}


	/* GL_VERSION_1_1 */
	{
		PY_MOD_ADD_METHOD(BindTexture);
		PY_MOD_ADD_METHOD(CopyTexImage1D);
		PY_MOD_ADD_METHOD(CopyTexImage2D);
		PY_MOD_ADD_METHOD(CopyTexSubImage1D);
		PY_MOD_ADD_METHOD(CopyTexSubImage2D);
		PY_MOD_ADD_METHOD(DeleteTextures);
		PY_MOD_ADD_METHOD(DrawArrays);
		PY_MOD_ADD_METHOD(DrawElements);
		PY_MOD_ADD_METHOD(GenTextures);
		PY_MOD_ADD_METHOD(IsTexture);
		PY_MOD_ADD_METHOD(PolygonOffset);
		PY_MOD_ADD_METHOD(TexSubImage1D);
		PY_MOD_ADD_METHOD(TexSubImage2D);
	}
	/* adding in GL_VERSION_1_1 removed from core profile */
	if (use_deprecated == true) {
		PY_MOD_ADD_METHOD(AreTexturesResident);
		PY_MOD_ADD_METHOD(ArrayElement);
		PY_MOD_ADD_METHOD(ColorPointer);
		PY_MOD_ADD_METHOD(DisableClientState);
		PY_MOD_ADD_METHOD(EdgeFlagPointer);
		PY_MOD_ADD_METHOD(EnableClientState);
		PY_MOD_ADD_METHOD(GetPointerv);
		PY_MOD_ADD_METHOD(IndexPointer);
		PY_MOD_ADD_METHOD(Indexub);
		PY_MOD_ADD_METHOD(Indexubv);
		PY_MOD_ADD_METHOD(InterleavedArrays);
		PY_MOD_ADD_METHOD(NormalPointer);
		PY_MOD_ADD_METHOD(PopClientAttrib);
		PY_MOD_ADD_METHOD(PrioritizeTextures);
		PY_MOD_ADD_METHOD(PushClientAttrib);
		PY_MOD_ADD_METHOD(TexCoordPointer);
		PY_MOD_ADD_METHOD(VertexPointer);
	}


	/* GL_VERSION_1_2 */
	{
		PY_MOD_ADD_METHOD(CopyTexSubImage3D);
		PY_MOD_ADD_METHOD(DrawRangeElements);
		PY_MOD_ADD_METHOD(TexImage3D);
		PY_MOD_ADD_METHOD(TexSubImage3D);
	}


	/* GL_VERSION_1_3 */
	{
		PY_MOD_ADD_METHOD(ActiveTexture);
		PY_MOD_ADD_METHOD(CompressedTexImage1D);
		PY_MOD_ADD_METHOD(CompressedTexImage2D);
		PY_MOD_ADD_METHOD(CompressedTexImage3D);
		PY_MOD_ADD_METHOD(CompressedTexSubImage1D);
		PY_MOD_ADD_METHOD(CompressedTexSubImage2D);
		PY_MOD_ADD_METHOD(CompressedTexSubImage3D);
		PY_MOD_ADD_METHOD(GetCompressedTexImage);
		PY_MOD_ADD_METHOD(SampleCoverage);
	}
	/* adding in GL_VERSION_1_3 removed from core profile */
	if (use_deprecated == true) {
		PY_MOD_ADD_METHOD(ClientActiveTexture);
		PY_MOD_ADD_METHOD(LoadTransposeMatrixd);
		PY_MOD_ADD_METHOD(LoadTransposeMatrixf);
		PY_MOD_ADD_METHOD(MultTransposeMatrixd);
		PY_MOD_ADD_METHOD(MultTransposeMatrixf);
		PY_MOD_ADD_METHOD(MultiTexCoord1d);
		PY_MOD_ADD_METHOD(MultiTexCoord1dv);
		PY_MOD_ADD_METHOD(MultiTexCoord1f);
		PY_MOD_ADD_METHOD(MultiTexCoord1fv);
		PY_MOD_ADD_METHOD(MultiTexCoord1i);
		PY_MOD_ADD_METHOD(MultiTexCoord1iv);
		PY_MOD_ADD_METHOD(MultiTexCoord1s);
		PY_MOD_ADD_METHOD(MultiTexCoord1sv);
		PY_MOD_ADD_METHOD(MultiTexCoord2d);
		PY_MOD_ADD_METHOD(MultiTexCoord2dv);
		PY_MOD_ADD_METHOD(MultiTexCoord2f);
		PY_MOD_ADD_METHOD(MultiTexCoord2fv);
		PY_MOD_ADD_METHOD(MultiTexCoord2i);
		PY_MOD_ADD_METHOD(MultiTexCoord2iv);
		PY_MOD_ADD_METHOD(MultiTexCoord2s);
		PY_MOD_ADD_METHOD(MultiTexCoord2sv);
		PY_MOD_ADD_METHOD(MultiTexCoord3d);
		PY_MOD_ADD_METHOD(MultiTexCoord3dv);
		PY_MOD_ADD_METHOD(MultiTexCoord3f);
		PY_MOD_ADD_METHOD(MultiTexCoord3fv);
		PY_MOD_ADD_METHOD(MultiTexCoord3i);
		PY_MOD_ADD_METHOD(MultiTexCoord3iv);
		PY_MOD_ADD_METHOD(MultiTexCoord3s);
		PY_MOD_ADD_METHOD(MultiTexCoord3sv);
		PY_MOD_ADD_METHOD(MultiTexCoord4d);
		PY_MOD_ADD_METHOD(MultiTexCoord4dv);
		PY_MOD_ADD_METHOD(MultiTexCoord4f);
		PY_MOD_ADD_METHOD(MultiTexCoord4fv);
		PY_MOD_ADD_METHOD(MultiTexCoord4i);
		PY_MOD_ADD_METHOD(MultiTexCoord4iv);
		PY_MOD_ADD_METHOD(MultiTexCoord4s);
		PY_MOD_ADD_METHOD(MultiTexCoord4sv);
	}


	/* GL_VERSION_1_4 */
	{
		PY_MOD_ADD_METHOD(BlendColor);
		PY_MOD_ADD_METHOD(BlendEquation);
	}


	/* GL_VERSION_1_5 */
	{
		PY_MOD_ADD_METHOD(BeginQuery);
		PY_MOD_ADD_METHOD(BindBuffer);
		PY_MOD_ADD_METHOD(BufferData);
		PY_MOD_ADD_METHOD(BufferSubData);
		PY_MOD_ADD_METHOD(DeleteBuffers);
		PY_MOD_ADD_METHOD(DeleteQueries);
		PY_MOD_ADD_METHOD(EndQuery);
		PY_MOD_ADD_METHOD(GenBuffers);
		PY_MOD_ADD_METHOD(GenQueries);
		PY_MOD_ADD_METHOD(GetBufferParameteriv);
		PY_MOD_ADD_METHOD(GetBufferPointerv);
		PY_MOD_ADD_METHOD(GetBufferSubData);
		PY_MOD_ADD_METHOD(GetQueryObjectiv);
		PY_MOD_ADD_METHOD(GetQueryObjectuiv);
		PY_MOD_ADD_METHOD(GetQueryiv);
		PY_MOD_ADD_METHOD(IsBuffer);
		PY_MOD_ADD_METHOD(IsQuery);
		PY_MOD_ADD_METHOD(MapBuffer);
		PY_MOD_ADD_METHOD(UnmapBuffer);
	}


	/* GL_VERSION_2_0 */
	{
		PY_MOD_ADD_METHOD(AttachShader);
		PY_MOD_ADD_METHOD(BindAttribLocation);
		PY_MOD_ADD_METHOD(BlendEquationSeparate);
		PY_MOD_ADD_METHOD(CompileShader);
		PY_MOD_ADD_METHOD(CreateProgram);
		PY_MOD_ADD_METHOD(CreateShader);
		PY_MOD_ADD_METHOD(DeleteProgram);
		PY_MOD_ADD_METHOD(DeleteShader);
		PY_MOD_ADD_METHOD(DetachShader);
		PY_MOD_ADD_METHOD(DisableVertexAttribArray);
		PY_MOD_ADD_METHOD(DrawBuffers);
		PY_MOD_ADD_METHOD(EnableVertexAttribArray);
		PY_MOD_ADD_METHOD(GetActiveAttrib);
		PY_MOD_ADD_METHOD(GetActiveUniform);
		PY_MOD_ADD_METHOD(GetAttachedShaders);
		PY_MOD_ADD_METHOD(GetAttribLocation);
		PY_MOD_ADD_METHOD(GetProgramInfoLog);
		PY_MOD_ADD_METHOD(GetProgramiv);
		PY_MOD_ADD_METHOD(GetShaderInfoLog);
		PY_MOD_ADD_METHOD(GetShaderSource);
		PY_MOD_ADD_METHOD(GetShaderiv);
		PY_MOD_ADD_METHOD(GetUniformLocation);
		PY_MOD_ADD_METHOD(GetUniformfv);
		PY_MOD_ADD_METHOD(GetUniformiv);
		PY_MOD_ADD_METHOD(GetVertexAttribPointerv);
		PY_MOD_ADD_METHOD(GetVertexAttribdv);
		PY_MOD_ADD_METHOD(GetVertexAttribfv);
		PY_MOD_ADD_METHOD(GetVertexAttribiv);
		PY_MOD_ADD_METHOD(IsProgram);
		PY_MOD_ADD_METHOD(IsShader);
		PY_MOD_ADD_METHOD(LinkProgram);
		PY_MOD_ADD_METHOD(ShaderSource);
		PY_MOD_ADD_METHOD(StencilFuncSeparate);
		PY_MOD_ADD_METHOD(StencilMaskSeparate);
		PY_MOD_ADD_METHOD(StencilOpSeparate);
		PY_MOD_ADD_METHOD(Uniform1f);
		PY_MOD_ADD_METHOD(Uniform1fv);
		PY_MOD_ADD_METHOD(Uniform1i);
		PY_MOD_ADD_METHOD(Uniform1iv);
		PY_MOD_ADD_METHOD(Uniform2f);
		PY_MOD_ADD_METHOD(Uniform2fv);
		PY_MOD_ADD_METHOD(Uniform2i);
		PY_MOD_ADD_METHOD(Uniform2iv);
		PY_MOD_ADD_METHOD(Uniform3f);
		PY_MOD_ADD_METHOD(Uniform3fv);
		PY_MOD_ADD_METHOD(Uniform3i);
		PY_MOD_ADD_METHOD(Uniform3iv);
		PY_MOD_ADD_METHOD(Uniform4f);
		PY_MOD_ADD_METHOD(Uniform4fv);
		PY_MOD_ADD_METHOD(Uniform4i);
		PY_MOD_ADD_METHOD(Uniform4iv);
		PY_MOD_ADD_METHOD(UniformMatrix2fv);
		PY_MOD_ADD_METHOD(UniformMatrix3fv);
		PY_MOD_ADD_METHOD(UniformMatrix4fv);
		PY_MOD_ADD_METHOD(UseProgram);
		PY_MOD_ADD_METHOD(ValidateProgram);
		PY_MOD_ADD_METHOD(VertexAttrib1d);
		PY_MOD_ADD_METHOD(VertexAttrib1dv);
		PY_MOD_ADD_METHOD(VertexAttrib1f);
		PY_MOD_ADD_METHOD(VertexAttrib1fv);
		PY_MOD_ADD_METHOD(VertexAttrib1s);
		PY_MOD_ADD_METHOD(VertexAttrib1sv);
		PY_MOD_ADD_METHOD(VertexAttrib2d);
		PY_MOD_ADD_METHOD(VertexAttrib2dv);
		PY_MOD_ADD_METHOD(VertexAttrib2f);
		PY_MOD_ADD_METHOD(VertexAttrib2fv);
		PY_MOD_ADD_METHOD(VertexAttrib2s);
		PY_MOD_ADD_METHOD(VertexAttrib2sv);
		PY_MOD_ADD_METHOD(VertexAttrib3d);
		PY_MOD_ADD_METHOD(VertexAttrib3dv);
		PY_MOD_ADD_METHOD(VertexAttrib3f);
		PY_MOD_ADD_METHOD(VertexAttrib3fv);
		PY_MOD_ADD_METHOD(VertexAttrib3s);
		PY_MOD_ADD_METHOD(VertexAttrib3sv);
		PY_MOD_ADD_METHOD(VertexAttrib4Nbv);
		PY_MOD_ADD_METHOD(VertexAttrib4Niv);
		PY_MOD_ADD_METHOD(VertexAttrib4Nsv);
		PY_MOD_ADD_METHOD(VertexAttrib4Nub);
		PY_MOD_ADD_METHOD(VertexAttrib4Nubv);
		PY_MOD_ADD_METHOD(VertexAttrib4Nuiv);
		PY_MOD_ADD_METHOD(VertexAttrib4Nusv);
		PY_MOD_ADD_METHOD(VertexAttrib4bv);
		PY_MOD_ADD_METHOD(VertexAttrib4d);
		PY_MOD_ADD_METHOD(VertexAttrib4dv);
		PY_MOD_ADD_METHOD(VertexAttrib4f);
		PY_MOD_ADD_METHOD(VertexAttrib4fv);
		PY_MOD_ADD_METHOD(VertexAttrib4iv);
		PY_MOD_ADD_METHOD(VertexAttrib4s);
		PY_MOD_ADD_METHOD(VertexAttrib4sv);
		PY_MOD_ADD_METHOD(VertexAttrib4ubv);
		PY_MOD_ADD_METHOD(VertexAttrib4uiv);
		PY_MOD_ADD_METHOD(VertexAttrib4usv);
		PY_MOD_ADD_METHOD(VertexAttribPointer);
	}


	/* GL_VERSION_2_1 */
	{
		PY_MOD_ADD_METHOD(UniformMatrix2x3fv);
		PY_MOD_ADD_METHOD(UniformMatrix2x4fv);
		PY_MOD_ADD_METHOD(UniformMatrix3x2fv);
		PY_MOD_ADD_METHOD(UniformMatrix3x4fv);
		PY_MOD_ADD_METHOD(UniformMatrix4x2fv);
		PY_MOD_ADD_METHOD(UniformMatrix4x3fv);
	}


	/* GL_VERSION_3_0 */
	{
		PY_MOD_ADD_METHOD(BindVertexArray);
		PY_MOD_ADD_METHOD(DeleteVertexArrays);
		PY_MOD_ADD_METHOD(GenVertexArrays);
		PY_MOD_ADD_METHOD(IsVertexArray);
	}


	/* GL_VERSION_3_1 */
	{
		PY_MOD_ADD_METHOD(BindBufferBase);
		PY_MOD_ADD_METHOD(BindBufferRange);
		PY_MOD_ADD_METHOD(GetActiveUniformBlockName);
		PY_MOD_ADD_METHOD(GetActiveUniformBlockiv);
		PY_MOD_ADD_METHOD(GetActiveUniformName);
		PY_MOD_ADD_METHOD(GetActiveUniformsiv);
		PY_MOD_ADD_METHOD(GetIntegeri_v);
		PY_MOD_ADD_METHOD(GetUniformBlockIndex);
		PY_MOD_ADD_METHOD(GetUniformIndices);
		PY_MOD_ADD_METHOD(UniformBlockBinding);
	}


	/* GL_VERSION_3_2 */
	{
		PY_MOD_ADD_METHOD(FramebufferTexture);
		PY_MOD_ADD_METHOD(GetBufferParameteri64v);
		PY_MOD_ADD_METHOD(GetInteger64i_v);
		PY_MOD_ADD_METHOD(GetMultisamplefv);
		PY_MOD_ADD_METHOD(SampleMaski);
		PY_MOD_ADD_METHOD(TexImage2DMultisample);
		PY_MOD_ADD_METHOD(TexImage3DMultisample);
	}


	/* GL_VERSION_3_3 */
	{
		PY_MOD_ADD_METHOD(ColorP3ui);
		PY_MOD_ADD_METHOD(ColorP3uiv);
		PY_MOD_ADD_METHOD(ColorP4ui);
		PY_MOD_ADD_METHOD(ColorP4uiv);
		PY_MOD_ADD_METHOD(MultiTexCoordP1ui);
		PY_MOD_ADD_METHOD(MultiTexCoordP1uiv);
		PY_MOD_ADD_METHOD(MultiTexCoordP2ui);
		PY_MOD_ADD_METHOD(MultiTexCoordP2uiv);
		PY_MOD_ADD_METHOD(MultiTexCoordP3ui);
		PY_MOD_ADD_METHOD(MultiTexCoordP3uiv);
		PY_MOD_ADD_METHOD(MultiTexCoordP4ui);
		PY_MOD_ADD_METHOD(MultiTexCoordP4uiv);
		PY_MOD_ADD_METHOD(NormalP3ui);
		PY_MOD_ADD_METHOD(NormalP3uiv);
		PY_MOD_ADD_METHOD(SecondaryColorP3ui);
		PY_MOD_ADD_METHOD(SecondaryColorP3uiv);
		PY_MOD_ADD_METHOD(TexCoordP1ui);
		PY_MOD_ADD_METHOD(TexCoordP1uiv);
		PY_MOD_ADD_METHOD(TexCoordP2ui);
		PY_MOD_ADD_METHOD(TexCoordP2uiv);
		PY_MOD_ADD_METHOD(TexCoordP3ui);
		PY_MOD_ADD_METHOD(TexCoordP3uiv);
		PY_MOD_ADD_METHOD(TexCoordP4ui);
		PY_MOD_ADD_METHOD(TexCoordP4uiv);
		PY_MOD_ADD_METHOD(VertexP2ui);
		PY_MOD_ADD_METHOD(VertexP2uiv);
		PY_MOD_ADD_METHOD(VertexP3ui);
		PY_MOD_ADD_METHOD(VertexP3uiv);
		PY_MOD_ADD_METHOD(VertexP4ui);
		PY_MOD_ADD_METHOD(VertexP4uiv);
	}

#define PY_DICT_ADD_INT(x) py_module_dict_add_int(dict, #x, x)
#define PY_DICT_ADD_INT64(x) py_module_dict_add_int64(dict, #x, x)

	/* GL_VERSION_1_1 */
	{
		PY_DICT_ADD_INT(GL_ALPHA);
		PY_DICT_ADD_INT(GL_ALWAYS);
		PY_DICT_ADD_INT(GL_AND);
		PY_DICT_ADD_INT(GL_AND_INVERTED);
		PY_DICT_ADD_INT(GL_AND_REVERSE);
		PY_DICT_ADD_INT(GL_BACK);
		PY_DICT_ADD_INT(GL_BACK_LEFT);
		PY_DICT_ADD_INT(GL_BACK_RIGHT);
		PY_DICT_ADD_INT(GL_BLEND);
		PY_DICT_ADD_INT(GL_BLEND_DST);
		PY_DICT_ADD_INT(GL_BLEND_SRC);
		PY_DICT_ADD_INT(GL_BLUE);
		PY_DICT_ADD_INT(GL_BYTE);
		PY_DICT_ADD_INT(GL_CCW);
		PY_DICT_ADD_INT(GL_CLEAR);
		PY_DICT_ADD_INT(GL_COLOR);
		PY_DICT_ADD_INT(GL_COLOR_BUFFER_BIT);
		PY_DICT_ADD_INT(GL_COLOR_CLEAR_VALUE);
		PY_DICT_ADD_INT(GL_COLOR_LOGIC_OP);
		PY_DICT_ADD_INT(GL_COLOR_WRITEMASK);
		PY_DICT_ADD_INT(GL_COPY);
		PY_DICT_ADD_INT(GL_COPY_INVERTED);
		PY_DICT_ADD_INT(GL_CULL_FACE);
		PY_DICT_ADD_INT(GL_CULL_FACE_MODE);
		PY_DICT_ADD_INT(GL_CW);
		PY_DICT_ADD_INT(GL_DECR);
		PY_DICT_ADD_INT(GL_DEPTH);
		PY_DICT_ADD_INT(GL_DEPTH_BUFFER_BIT);
		PY_DICT_ADD_INT(GL_DEPTH_CLEAR_VALUE);
		PY_DICT_ADD_INT(GL_DEPTH_COMPONENT);
		PY_DICT_ADD_INT(GL_DEPTH_FUNC);
		PY_DICT_ADD_INT(GL_DEPTH_RANGE);
		PY_DICT_ADD_INT(GL_DEPTH_TEST);
		PY_DICT_ADD_INT(GL_DEPTH_WRITEMASK);
		PY_DICT_ADD_INT(GL_DITHER);
		PY_DICT_ADD_INT(GL_DONT_CARE);
		PY_DICT_ADD_INT(GL_DOUBLE);
		PY_DICT_ADD_INT(GL_DOUBLEBUFFER);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER);
		PY_DICT_ADD_INT(GL_DST_ALPHA);
		PY_DICT_ADD_INT(GL_DST_COLOR);
		PY_DICT_ADD_INT(GL_EQUAL);
		PY_DICT_ADD_INT(GL_EQUIV);
		PY_DICT_ADD_INT(GL_EXTENSIONS);
		PY_DICT_ADD_INT(GL_FALSE);
		PY_DICT_ADD_INT(GL_FASTEST);
		PY_DICT_ADD_INT(GL_FILL);
		PY_DICT_ADD_INT(GL_FLOAT);
		PY_DICT_ADD_INT(GL_FRONT);
		PY_DICT_ADD_INT(GL_FRONT_AND_BACK);
		PY_DICT_ADD_INT(GL_FRONT_FACE);
		PY_DICT_ADD_INT(GL_FRONT_LEFT);
		PY_DICT_ADD_INT(GL_FRONT_RIGHT);
		PY_DICT_ADD_INT(GL_GEQUAL);
		PY_DICT_ADD_INT(GL_GREATER);
		PY_DICT_ADD_INT(GL_GREEN);
		PY_DICT_ADD_INT(GL_INCR);
		PY_DICT_ADD_INT(GL_INT);
		PY_DICT_ADD_INT(GL_INVALID_ENUM);
		PY_DICT_ADD_INT(GL_INVALID_OPERATION);
		PY_DICT_ADD_INT(GL_INVALID_VALUE);
		PY_DICT_ADD_INT(GL_INVERT);
		PY_DICT_ADD_INT(GL_KEEP);
		PY_DICT_ADD_INT(GL_LEFT);
		PY_DICT_ADD_INT(GL_LEQUAL);
		PY_DICT_ADD_INT(GL_LESS);
		PY_DICT_ADD_INT(GL_LINE);
		PY_DICT_ADD_INT(GL_LINEAR);
		PY_DICT_ADD_INT(GL_LINEAR_MIPMAP_LINEAR);
		PY_DICT_ADD_INT(GL_LINEAR_MIPMAP_NEAREST);
		PY_DICT_ADD_INT(GL_LINES);
		PY_DICT_ADD_INT(GL_LINE_LOOP);
		PY_DICT_ADD_INT(GL_LINE_SMOOTH);
		PY_DICT_ADD_INT(GL_LINE_SMOOTH_HINT);
		PY_DICT_ADD_INT(GL_LINE_STRIP);
		PY_DICT_ADD_INT(GL_LINE_WIDTH);
		PY_DICT_ADD_INT(GL_LINE_WIDTH_GRANULARITY);
		PY_DICT_ADD_INT(GL_LINE_WIDTH_RANGE);
		PY_DICT_ADD_INT(GL_LOGIC_OP_MODE);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_SIZE);
		PY_DICT_ADD_INT(GL_MAX_VIEWPORT_DIMS);
		PY_DICT_ADD_INT(GL_NAND);
		PY_DICT_ADD_INT(GL_NEAREST);
		PY_DICT_ADD_INT(GL_NEAREST_MIPMAP_LINEAR);
		PY_DICT_ADD_INT(GL_NEAREST_MIPMAP_NEAREST);
		PY_DICT_ADD_INT(GL_NEVER);
		PY_DICT_ADD_INT(GL_NICEST);
		PY_DICT_ADD_INT(GL_NONE);
		PY_DICT_ADD_INT(GL_NOOP);
		PY_DICT_ADD_INT(GL_NOR);
		PY_DICT_ADD_INT(GL_NOTEQUAL);
		PY_DICT_ADD_INT(GL_NO_ERROR);
		PY_DICT_ADD_INT(GL_ONE);
		PY_DICT_ADD_INT(GL_ONE_MINUS_DST_ALPHA);
		PY_DICT_ADD_INT(GL_ONE_MINUS_DST_COLOR);
		PY_DICT_ADD_INT(GL_ONE_MINUS_SRC_ALPHA);
		PY_DICT_ADD_INT(GL_ONE_MINUS_SRC_COLOR);
		PY_DICT_ADD_INT(GL_OR);
		PY_DICT_ADD_INT(GL_OR_INVERTED);
		PY_DICT_ADD_INT(GL_OR_REVERSE);
		PY_DICT_ADD_INT(GL_OUT_OF_MEMORY);
		PY_DICT_ADD_INT(GL_PACK_ALIGNMENT);
		PY_DICT_ADD_INT(GL_PACK_LSB_FIRST);
		PY_DICT_ADD_INT(GL_PACK_ROW_LENGTH);
		PY_DICT_ADD_INT(GL_PACK_SKIP_PIXELS);
		PY_DICT_ADD_INT(GL_PACK_SKIP_ROWS);
		PY_DICT_ADD_INT(GL_PACK_SWAP_BYTES);
		PY_DICT_ADD_INT(GL_POINT);
		PY_DICT_ADD_INT(GL_POINTS);
		PY_DICT_ADD_INT(GL_POINT_SIZE);
		PY_DICT_ADD_INT(GL_POINT_SIZE_GRANULARITY);
		PY_DICT_ADD_INT(GL_POINT_SIZE_RANGE);
		PY_DICT_ADD_INT(GL_POLYGON_MODE);
		PY_DICT_ADD_INT(GL_POLYGON_OFFSET_FACTOR);
		PY_DICT_ADD_INT(GL_POLYGON_OFFSET_FILL);
		PY_DICT_ADD_INT(GL_POLYGON_OFFSET_LINE);
		PY_DICT_ADD_INT(GL_POLYGON_OFFSET_POINT);
		PY_DICT_ADD_INT(GL_POLYGON_OFFSET_UNITS);
		PY_DICT_ADD_INT(GL_POLYGON_SMOOTH);
		PY_DICT_ADD_INT(GL_POLYGON_SMOOTH_HINT);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_1D);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_2D);
		PY_DICT_ADD_INT(GL_R3_G3_B2);
		PY_DICT_ADD_INT(GL_READ_BUFFER);
		PY_DICT_ADD_INT(GL_RED);
		PY_DICT_ADD_INT(GL_RENDERER);
		PY_DICT_ADD_INT(GL_REPEAT);
		PY_DICT_ADD_INT(GL_REPLACE);
		PY_DICT_ADD_INT(GL_RGB);
		PY_DICT_ADD_INT(GL_RGB10);
		PY_DICT_ADD_INT(GL_RGB10_A2);
		PY_DICT_ADD_INT(GL_RGB12);
		PY_DICT_ADD_INT(GL_RGB16);
		PY_DICT_ADD_INT(GL_RGB4);
		PY_DICT_ADD_INT(GL_RGB5);
		PY_DICT_ADD_INT(GL_RGB5_A1);
		PY_DICT_ADD_INT(GL_RGB8);
		PY_DICT_ADD_INT(GL_RGBA);
		PY_DICT_ADD_INT(GL_RGBA12);
		PY_DICT_ADD_INT(GL_RGBA16);
		PY_DICT_ADD_INT(GL_RGBA2);
		PY_DICT_ADD_INT(GL_RGBA4);
		PY_DICT_ADD_INT(GL_RGBA8);
		PY_DICT_ADD_INT(GL_RIGHT);
		PY_DICT_ADD_INT(GL_SCISSOR_BOX);
		PY_DICT_ADD_INT(GL_SCISSOR_TEST);
		PY_DICT_ADD_INT(GL_SET);
		PY_DICT_ADD_INT(GL_SHORT);
		PY_DICT_ADD_INT(GL_SRC_ALPHA);
		PY_DICT_ADD_INT(GL_SRC_ALPHA_SATURATE);
		PY_DICT_ADD_INT(GL_SRC_COLOR);
		PY_DICT_ADD_INT(GL_STENCIL);
		PY_DICT_ADD_INT(GL_STENCIL_BUFFER_BIT);
		PY_DICT_ADD_INT(GL_STENCIL_CLEAR_VALUE);
		PY_DICT_ADD_INT(GL_STENCIL_FAIL);
		PY_DICT_ADD_INT(GL_STENCIL_FUNC);
		PY_DICT_ADD_INT(GL_STENCIL_INDEX);
		PY_DICT_ADD_INT(GL_STENCIL_PASS_DEPTH_FAIL);
		PY_DICT_ADD_INT(GL_STENCIL_PASS_DEPTH_PASS);
		PY_DICT_ADD_INT(GL_STENCIL_REF);
		PY_DICT_ADD_INT(GL_STENCIL_TEST);
		PY_DICT_ADD_INT(GL_STENCIL_VALUE_MASK);
		PY_DICT_ADD_INT(GL_STENCIL_WRITEMASK);
		PY_DICT_ADD_INT(GL_STEREO);
		PY_DICT_ADD_INT(GL_SUBPIXEL_BITS);
		PY_DICT_ADD_INT(GL_TEXTURE);
		PY_DICT_ADD_INT(GL_TEXTURE_1D);
		PY_DICT_ADD_INT(GL_TEXTURE_2D);
		PY_DICT_ADD_INT(GL_TEXTURE_ALPHA_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_1D);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_2D);
		PY_DICT_ADD_INT(GL_TEXTURE_BLUE_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_BORDER_COLOR);
		PY_DICT_ADD_INT(GL_TEXTURE_GREEN_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_HEIGHT);
		PY_DICT_ADD_INT(GL_TEXTURE_INTERNAL_FORMAT);
		PY_DICT_ADD_INT(GL_TEXTURE_MAG_FILTER);
		PY_DICT_ADD_INT(GL_TEXTURE_MIN_FILTER);
		PY_DICT_ADD_INT(GL_TEXTURE_RED_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_WIDTH);
		PY_DICT_ADD_INT(GL_TEXTURE_WRAP_S);
		PY_DICT_ADD_INT(GL_TEXTURE_WRAP_T);
		PY_DICT_ADD_INT(GL_TRIANGLES);
		PY_DICT_ADD_INT(GL_TRIANGLE_FAN);
		PY_DICT_ADD_INT(GL_TRIANGLE_STRIP);
		PY_DICT_ADD_INT(GL_TRUE);
		PY_DICT_ADD_INT(GL_UNPACK_ALIGNMENT);
		PY_DICT_ADD_INT(GL_UNPACK_LSB_FIRST);
		PY_DICT_ADD_INT(GL_UNPACK_ROW_LENGTH);
		PY_DICT_ADD_INT(GL_UNPACK_SKIP_PIXELS);
		PY_DICT_ADD_INT(GL_UNPACK_SKIP_ROWS);
		PY_DICT_ADD_INT(GL_UNPACK_SWAP_BYTES);
		PY_DICT_ADD_INT(GL_UNSIGNED_BYTE);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT);
		PY_DICT_ADD_INT(GL_VENDOR);
		PY_DICT_ADD_INT(GL_VERSION);
		PY_DICT_ADD_INT(GL_VIEWPORT);
		PY_DICT_ADD_INT(GL_XOR);
		PY_DICT_ADD_INT(GL_ZERO);
	}
	/* adding in GL_VERSION_1_1 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_2D);
		PY_DICT_ADD_INT(GL_2_BYTES);
		PY_DICT_ADD_INT(GL_3D);
		PY_DICT_ADD_INT(GL_3D_COLOR);
		PY_DICT_ADD_INT(GL_3D_COLOR_TEXTURE);
		PY_DICT_ADD_INT(GL_3_BYTES);
		PY_DICT_ADD_INT(GL_4D_COLOR_TEXTURE);
		PY_DICT_ADD_INT(GL_4_BYTES);
		PY_DICT_ADD_INT(GL_ACCUM);
		PY_DICT_ADD_INT(GL_ACCUM_ALPHA_BITS);
		PY_DICT_ADD_INT(GL_ACCUM_BLUE_BITS);
		PY_DICT_ADD_INT(GL_ACCUM_BUFFER_BIT);
		PY_DICT_ADD_INT(GL_ACCUM_CLEAR_VALUE);
		PY_DICT_ADD_INT(GL_ACCUM_GREEN_BITS);
		PY_DICT_ADD_INT(GL_ACCUM_RED_BITS);
		PY_DICT_ADD_INT(GL_ADD);
		PY_DICT_ADD_INT(GL_ALL_ATTRIB_BITS);
		PY_DICT_ADD_INT(GL_ALPHA12);
		PY_DICT_ADD_INT(GL_ALPHA16);
		PY_DICT_ADD_INT(GL_ALPHA4);
		PY_DICT_ADD_INT(GL_ALPHA8);
		PY_DICT_ADD_INT(GL_ALPHA_BIAS);
		PY_DICT_ADD_INT(GL_ALPHA_BITS);
		PY_DICT_ADD_INT(GL_ALPHA_SCALE);
		PY_DICT_ADD_INT(GL_ALPHA_TEST);
		PY_DICT_ADD_INT(GL_ALPHA_TEST_FUNC);
		PY_DICT_ADD_INT(GL_ALPHA_TEST_REF);
		PY_DICT_ADD_INT(GL_AMBIENT);
		PY_DICT_ADD_INT(GL_AMBIENT_AND_DIFFUSE);
		PY_DICT_ADD_INT(GL_ATTRIB_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_AUTO_NORMAL);
		PY_DICT_ADD_INT(GL_AUX0);
		PY_DICT_ADD_INT(GL_AUX1);
		PY_DICT_ADD_INT(GL_AUX2);
		PY_DICT_ADD_INT(GL_AUX3);
		PY_DICT_ADD_INT(GL_AUX_BUFFERS);
		PY_DICT_ADD_INT(GL_BITMAP);
		PY_DICT_ADD_INT(GL_BITMAP_TOKEN);
		PY_DICT_ADD_INT(GL_BLUE_BIAS);
		PY_DICT_ADD_INT(GL_BLUE_BITS);
		PY_DICT_ADD_INT(GL_BLUE_SCALE);
		PY_DICT_ADD_INT(GL_C3F_V3F);
		PY_DICT_ADD_INT(GL_C4F_N3F_V3F);
		PY_DICT_ADD_INT(GL_C4UB_V2F);
		PY_DICT_ADD_INT(GL_C4UB_V3F);
		PY_DICT_ADD_INT(GL_CLAMP);
		PY_DICT_ADD_INT(GL_CLIENT_ALL_ATTRIB_BITS);
		PY_DICT_ADD_INT(GL_CLIENT_ATTRIB_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_CLIENT_PIXEL_STORE_BIT);
		PY_DICT_ADD_INT(GL_CLIENT_VERTEX_ARRAY_BIT);
		PY_DICT_ADD_INT(GL_CLIP_PLANE0);
		PY_DICT_ADD_INT(GL_CLIP_PLANE1);
		PY_DICT_ADD_INT(GL_CLIP_PLANE2);
		PY_DICT_ADD_INT(GL_CLIP_PLANE3);
		PY_DICT_ADD_INT(GL_CLIP_PLANE4);
		PY_DICT_ADD_INT(GL_CLIP_PLANE5);
		PY_DICT_ADD_INT(GL_COEFF);
		PY_DICT_ADD_INT(GL_COLOR_ARRAY);
		PY_DICT_ADD_INT(GL_COLOR_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_COLOR_ARRAY_SIZE);
		PY_DICT_ADD_INT(GL_COLOR_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_COLOR_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_COLOR_INDEX);
		PY_DICT_ADD_INT(GL_COLOR_INDEXES);
		PY_DICT_ADD_INT(GL_COLOR_MATERIAL);
		PY_DICT_ADD_INT(GL_COLOR_MATERIAL_FACE);
		PY_DICT_ADD_INT(GL_COLOR_MATERIAL_PARAMETER);
		PY_DICT_ADD_INT(GL_COMPILE);
		PY_DICT_ADD_INT(GL_COMPILE_AND_EXECUTE);
		PY_DICT_ADD_INT(GL_CONSTANT_ATTENUATION);
		PY_DICT_ADD_INT(GL_COPY_PIXEL_TOKEN);
		PY_DICT_ADD_INT(GL_CURRENT_BIT);
		PY_DICT_ADD_INT(GL_CURRENT_COLOR);
		PY_DICT_ADD_INT(GL_CURRENT_INDEX);
		PY_DICT_ADD_INT(GL_CURRENT_NORMAL);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_COLOR);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_DISTANCE);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_INDEX);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_POSITION);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_POSITION_VALID);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_TEXTURE_COORDS);
		PY_DICT_ADD_INT(GL_CURRENT_TEXTURE_COORDS);
		PY_DICT_ADD_INT(GL_DECAL);
		PY_DICT_ADD_INT(GL_DEPTH_BIAS);
		PY_DICT_ADD_INT(GL_DEPTH_BITS);
		PY_DICT_ADD_INT(GL_DEPTH_SCALE);
		PY_DICT_ADD_INT(GL_DIFFUSE);
		PY_DICT_ADD_INT(GL_DOMAIN);
		PY_DICT_ADD_INT(GL_DRAW_PIXEL_TOKEN);
		PY_DICT_ADD_INT(GL_EDGE_FLAG);
		PY_DICT_ADD_INT(GL_EDGE_FLAG_ARRAY);
		PY_DICT_ADD_INT(GL_EDGE_FLAG_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_EDGE_FLAG_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_EMISSION);
		PY_DICT_ADD_INT(GL_ENABLE_BIT);
		PY_DICT_ADD_INT(GL_EVAL_BIT);
		PY_DICT_ADD_INT(GL_EXP);
		PY_DICT_ADD_INT(GL_EXP2);
		PY_DICT_ADD_INT(GL_EYE_LINEAR);
		PY_DICT_ADD_INT(GL_EYE_PLANE);
		PY_DICT_ADD_INT(GL_FEEDBACK);
		PY_DICT_ADD_INT(GL_FEEDBACK_BUFFER_POINTER);
		PY_DICT_ADD_INT(GL_FEEDBACK_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_FEEDBACK_BUFFER_TYPE);
		PY_DICT_ADD_INT(GL_FLAT);
		PY_DICT_ADD_INT(GL_FOG);
		PY_DICT_ADD_INT(GL_FOG_BIT);
		PY_DICT_ADD_INT(GL_FOG_COLOR);
		PY_DICT_ADD_INT(GL_FOG_DENSITY);
		PY_DICT_ADD_INT(GL_FOG_END);
		PY_DICT_ADD_INT(GL_FOG_HINT);
		PY_DICT_ADD_INT(GL_FOG_INDEX);
		PY_DICT_ADD_INT(GL_FOG_MODE);
		PY_DICT_ADD_INT(GL_FOG_START);
		PY_DICT_ADD_INT(GL_GREEN_BIAS);
		PY_DICT_ADD_INT(GL_GREEN_BITS);
		PY_DICT_ADD_INT(GL_GREEN_SCALE);
		PY_DICT_ADD_INT(GL_HINT_BIT);
		PY_DICT_ADD_INT(GL_INDEX_ARRAY);
		PY_DICT_ADD_INT(GL_INDEX_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_INDEX_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_INDEX_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_INDEX_BITS);
		PY_DICT_ADD_INT(GL_INDEX_CLEAR_VALUE);
		PY_DICT_ADD_INT(GL_INDEX_LOGIC_OP);
		PY_DICT_ADD_INT(GL_INDEX_MODE);
		PY_DICT_ADD_INT(GL_INDEX_OFFSET);
		PY_DICT_ADD_INT(GL_INDEX_SHIFT);
		PY_DICT_ADD_INT(GL_INDEX_WRITEMASK);
		PY_DICT_ADD_INT(GL_INTENSITY);
		PY_DICT_ADD_INT(GL_INTENSITY12);
		PY_DICT_ADD_INT(GL_INTENSITY16);
		PY_DICT_ADD_INT(GL_INTENSITY4);
		PY_DICT_ADD_INT(GL_INTENSITY8);
		PY_DICT_ADD_INT(GL_LIGHT0);
		PY_DICT_ADD_INT(GL_LIGHT1);
		PY_DICT_ADD_INT(GL_LIGHT2);
		PY_DICT_ADD_INT(GL_LIGHT3);
		PY_DICT_ADD_INT(GL_LIGHT4);
		PY_DICT_ADD_INT(GL_LIGHT5);
		PY_DICT_ADD_INT(GL_LIGHT6);
		PY_DICT_ADD_INT(GL_LIGHT7);
		PY_DICT_ADD_INT(GL_LIGHTING);
		PY_DICT_ADD_INT(GL_LIGHTING_BIT);
		PY_DICT_ADD_INT(GL_LIGHT_MODEL_AMBIENT);
		PY_DICT_ADD_INT(GL_LIGHT_MODEL_LOCAL_VIEWER);
		PY_DICT_ADD_INT(GL_LIGHT_MODEL_TWO_SIDE);
		PY_DICT_ADD_INT(GL_LINEAR_ATTENUATION);
		PY_DICT_ADD_INT(GL_LINE_BIT);
		PY_DICT_ADD_INT(GL_LINE_RESET_TOKEN);
		PY_DICT_ADD_INT(GL_LINE_STIPPLE);
		PY_DICT_ADD_INT(GL_LINE_STIPPLE_PATTERN);
		PY_DICT_ADD_INT(GL_LINE_STIPPLE_REPEAT);
		PY_DICT_ADD_INT(GL_LINE_TOKEN);
		PY_DICT_ADD_INT(GL_LIST_BASE);
		PY_DICT_ADD_INT(GL_LIST_BIT);
		PY_DICT_ADD_INT(GL_LIST_INDEX);
		PY_DICT_ADD_INT(GL_LIST_MODE);
		PY_DICT_ADD_INT(GL_LOAD);
		PY_DICT_ADD_INT(GL_LOGIC_OP);
		PY_DICT_ADD_INT(GL_LUMINANCE);
		PY_DICT_ADD_INT(GL_LUMINANCE12);
		PY_DICT_ADD_INT(GL_LUMINANCE12_ALPHA12);
		PY_DICT_ADD_INT(GL_LUMINANCE12_ALPHA4);
		PY_DICT_ADD_INT(GL_LUMINANCE16);
		PY_DICT_ADD_INT(GL_LUMINANCE16_ALPHA16);
		PY_DICT_ADD_INT(GL_LUMINANCE4);
		PY_DICT_ADD_INT(GL_LUMINANCE4_ALPHA4);
		PY_DICT_ADD_INT(GL_LUMINANCE6_ALPHA2);
		PY_DICT_ADD_INT(GL_LUMINANCE8);
		PY_DICT_ADD_INT(GL_LUMINANCE8_ALPHA8);
		PY_DICT_ADD_INT(GL_LUMINANCE_ALPHA);
		PY_DICT_ADD_INT(GL_MAP1_COLOR_4);
		PY_DICT_ADD_INT(GL_MAP1_GRID_DOMAIN);
		PY_DICT_ADD_INT(GL_MAP1_GRID_SEGMENTS);
		PY_DICT_ADD_INT(GL_MAP1_INDEX);
		PY_DICT_ADD_INT(GL_MAP1_NORMAL);
		PY_DICT_ADD_INT(GL_MAP1_TEXTURE_COORD_1);
		PY_DICT_ADD_INT(GL_MAP1_TEXTURE_COORD_2);
		PY_DICT_ADD_INT(GL_MAP1_TEXTURE_COORD_3);
		PY_DICT_ADD_INT(GL_MAP1_TEXTURE_COORD_4);
		PY_DICT_ADD_INT(GL_MAP1_VERTEX_3);
		PY_DICT_ADD_INT(GL_MAP1_VERTEX_4);
		PY_DICT_ADD_INT(GL_MAP2_COLOR_4);
		PY_DICT_ADD_INT(GL_MAP2_GRID_DOMAIN);
		PY_DICT_ADD_INT(GL_MAP2_GRID_SEGMENTS);
		PY_DICT_ADD_INT(GL_MAP2_INDEX);
		PY_DICT_ADD_INT(GL_MAP2_NORMAL);
		PY_DICT_ADD_INT(GL_MAP2_TEXTURE_COORD_1);
		PY_DICT_ADD_INT(GL_MAP2_TEXTURE_COORD_2);
		PY_DICT_ADD_INT(GL_MAP2_TEXTURE_COORD_3);
		PY_DICT_ADD_INT(GL_MAP2_TEXTURE_COORD_4);
		PY_DICT_ADD_INT(GL_MAP2_VERTEX_3);
		PY_DICT_ADD_INT(GL_MAP2_VERTEX_4);
		PY_DICT_ADD_INT(GL_MAP_COLOR);
		PY_DICT_ADD_INT(GL_MAP_STENCIL);
		PY_DICT_ADD_INT(GL_MATRIX_MODE);
		PY_DICT_ADD_INT(GL_MAX_ATTRIB_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MAX_CLIP_PLANES);
		PY_DICT_ADD_INT(GL_MAX_EVAL_ORDER);
		PY_DICT_ADD_INT(GL_MAX_LIGHTS);
		PY_DICT_ADD_INT(GL_MAX_LIST_NESTING);
		PY_DICT_ADD_INT(GL_MAX_MODELVIEW_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MAX_NAME_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MAX_PIXEL_MAP_TABLE);
		PY_DICT_ADD_INT(GL_MAX_PROJECTION_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MODELVIEW);
		PY_DICT_ADD_INT(GL_MODELVIEW_MATRIX);
		PY_DICT_ADD_INT(GL_MODELVIEW_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_MODULATE);
		PY_DICT_ADD_INT(GL_MULT);
		PY_DICT_ADD_INT(GL_N3F_V3F);
		PY_DICT_ADD_INT(GL_NAME_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_NORMALIZE);
		PY_DICT_ADD_INT(GL_NORMAL_ARRAY);
		PY_DICT_ADD_INT(GL_NORMAL_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_NORMAL_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_NORMAL_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_OBJECT_LINEAR);
		PY_DICT_ADD_INT(GL_OBJECT_PLANE);
		PY_DICT_ADD_INT(GL_ORDER);
		PY_DICT_ADD_INT(GL_PASS_THROUGH_TOKEN);
		PY_DICT_ADD_INT(GL_PERSPECTIVE_CORRECTION_HINT);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_A_TO_A);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_A_TO_A_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_B_TO_B);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_B_TO_B_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_G_TO_G);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_G_TO_G_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_A);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_A_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_B);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_B_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_G);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_G_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_I);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_I_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_R);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_I_TO_R_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_R_TO_R);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_R_TO_R_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_S_TO_S);
		PY_DICT_ADD_INT(GL_PIXEL_MAP_S_TO_S_SIZE);
		PY_DICT_ADD_INT(GL_PIXEL_MODE_BIT);
		PY_DICT_ADD_INT(GL_POINT_BIT);
		PY_DICT_ADD_INT(GL_POINT_SMOOTH);
		PY_DICT_ADD_INT(GL_POINT_SMOOTH_HINT);
		PY_DICT_ADD_INT(GL_POINT_TOKEN);
		PY_DICT_ADD_INT(GL_POLYGON);
		PY_DICT_ADD_INT(GL_POLYGON_BIT);
		PY_DICT_ADD_INT(GL_POLYGON_STIPPLE);
		PY_DICT_ADD_INT(GL_POLYGON_STIPPLE_BIT);
		PY_DICT_ADD_INT(GL_POLYGON_TOKEN);
		PY_DICT_ADD_INT(GL_POSITION);
		PY_DICT_ADD_INT(GL_PROJECTION);
		PY_DICT_ADD_INT(GL_PROJECTION_MATRIX);
		PY_DICT_ADD_INT(GL_PROJECTION_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_Q);
		PY_DICT_ADD_INT(GL_QUADRATIC_ATTENUATION);
		PY_DICT_ADD_INT(GL_QUADS);
		PY_DICT_ADD_INT(GL_QUAD_STRIP);
		PY_DICT_ADD_INT(GL_R);
		PY_DICT_ADD_INT(GL_RED_BIAS);
		PY_DICT_ADD_INT(GL_RED_BITS);
		PY_DICT_ADD_INT(GL_RED_SCALE);
		PY_DICT_ADD_INT(GL_RENDER);
		PY_DICT_ADD_INT(GL_RENDER_MODE);
		PY_DICT_ADD_INT(GL_RETURN);
		PY_DICT_ADD_INT(GL_RGBA_MODE);
		PY_DICT_ADD_INT(GL_S);
		PY_DICT_ADD_INT(GL_SCISSOR_BIT);
		PY_DICT_ADD_INT(GL_SELECT);
		PY_DICT_ADD_INT(GL_SELECTION_BUFFER_POINTER);
		PY_DICT_ADD_INT(GL_SELECTION_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_SHADE_MODEL);
		PY_DICT_ADD_INT(GL_SHININESS);
		PY_DICT_ADD_INT(GL_SMOOTH);
		PY_DICT_ADD_INT(GL_SPECULAR);
		PY_DICT_ADD_INT(GL_SPHERE_MAP);
		PY_DICT_ADD_INT(GL_SPOT_CUTOFF);
		PY_DICT_ADD_INT(GL_SPOT_DIRECTION);
		PY_DICT_ADD_INT(GL_SPOT_EXPONENT);
		PY_DICT_ADD_INT(GL_STACK_OVERFLOW);
		PY_DICT_ADD_INT(GL_STACK_UNDERFLOW);
		PY_DICT_ADD_INT(GL_STENCIL_BITS);
		PY_DICT_ADD_INT(GL_T);
		PY_DICT_ADD_INT(GL_T2F_C3F_V3F);
		PY_DICT_ADD_INT(GL_T2F_C4F_N3F_V3F);
		PY_DICT_ADD_INT(GL_T2F_C4UB_V3F);
		PY_DICT_ADD_INT(GL_T2F_N3F_V3F);
		PY_DICT_ADD_INT(GL_T2F_V3F);
		PY_DICT_ADD_INT(GL_T4F_C4F_N3F_V4F);
		PY_DICT_ADD_INT(GL_T4F_V4F);
		PY_DICT_ADD_INT(GL_TEXTURE_BIT);
		PY_DICT_ADD_INT(GL_TEXTURE_BORDER);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPONENTS);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_ENV);
		PY_DICT_ADD_INT(GL_TEXTURE_ENV_COLOR);
		PY_DICT_ADD_INT(GL_TEXTURE_ENV_MODE);
		PY_DICT_ADD_INT(GL_TEXTURE_GEN_MODE);
		PY_DICT_ADD_INT(GL_TEXTURE_GEN_Q);
		PY_DICT_ADD_INT(GL_TEXTURE_GEN_R);
		PY_DICT_ADD_INT(GL_TEXTURE_GEN_S);
		PY_DICT_ADD_INT(GL_TEXTURE_GEN_T);
		PY_DICT_ADD_INT(GL_TEXTURE_INTENSITY_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_LUMINANCE_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_MATRIX);
		PY_DICT_ADD_INT(GL_TEXTURE_PRIORITY);
		PY_DICT_ADD_INT(GL_TEXTURE_RESIDENT);
		PY_DICT_ADD_INT(GL_TEXTURE_STACK_DEPTH);
		PY_DICT_ADD_INT(GL_TRANSFORM_BIT);
		PY_DICT_ADD_INT(GL_V2F);
		PY_DICT_ADD_INT(GL_V3F);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_SIZE);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_VIEWPORT_BIT);
		PY_DICT_ADD_INT(GL_ZOOM_X);
		PY_DICT_ADD_INT(GL_ZOOM_Y);
	}


	/* GL_VERSION_1_2 */
	{
		PY_DICT_ADD_INT(GL_ALIASED_LINE_WIDTH_RANGE);
		PY_DICT_ADD_INT(GL_BGR);
		PY_DICT_ADD_INT(GL_BGRA);
		PY_DICT_ADD_INT(GL_CLAMP_TO_EDGE);
		PY_DICT_ADD_INT(GL_MAX_3D_TEXTURE_SIZE);
		PY_DICT_ADD_INT(GL_MAX_ELEMENTS_INDICES);
		PY_DICT_ADD_INT(GL_MAX_ELEMENTS_VERTICES);
		PY_DICT_ADD_INT(GL_PACK_IMAGE_HEIGHT);
		PY_DICT_ADD_INT(GL_PACK_SKIP_IMAGES);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_3D);
		PY_DICT_ADD_INT(GL_SMOOTH_LINE_WIDTH_GRANULARITY);
		PY_DICT_ADD_INT(GL_SMOOTH_LINE_WIDTH_RANGE);
		PY_DICT_ADD_INT(GL_SMOOTH_POINT_SIZE_GRANULARITY);
		PY_DICT_ADD_INT(GL_SMOOTH_POINT_SIZE_RANGE);
		PY_DICT_ADD_INT(GL_TEXTURE_3D);
		PY_DICT_ADD_INT(GL_TEXTURE_BASE_LEVEL);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_3D);
		PY_DICT_ADD_INT(GL_TEXTURE_DEPTH);
		PY_DICT_ADD_INT(GL_TEXTURE_MAX_LEVEL);
		PY_DICT_ADD_INT(GL_TEXTURE_MAX_LOD);
		PY_DICT_ADD_INT(GL_TEXTURE_MIN_LOD);
		PY_DICT_ADD_INT(GL_TEXTURE_WRAP_R);
		PY_DICT_ADD_INT(GL_UNPACK_IMAGE_HEIGHT);
		PY_DICT_ADD_INT(GL_UNPACK_SKIP_IMAGES);
		PY_DICT_ADD_INT(GL_UNSIGNED_BYTE_2_3_3_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_BYTE_3_3_2);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_10_10_10_2);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_2_10_10_10_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_8_8_8_8);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_8_8_8_8_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_1_5_5_5_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_4_4_4_4);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_4_4_4_4_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_5_5_5_1);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_5_6_5);
		PY_DICT_ADD_INT(GL_UNSIGNED_SHORT_5_6_5_REV);
	}
	/* adding in GL_VERSION_1_2 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_ALIASED_POINT_SIZE_RANGE);
		PY_DICT_ADD_INT(GL_LIGHT_MODEL_COLOR_CONTROL);
		PY_DICT_ADD_INT(GL_RESCALE_NORMAL);
		PY_DICT_ADD_INT(GL_SEPARATE_SPECULAR_COLOR);
		PY_DICT_ADD_INT(GL_SINGLE_COLOR);
	}


	/* GL_VERSION_1_3 */
	{
		PY_DICT_ADD_INT(GL_ACTIVE_TEXTURE);
		PY_DICT_ADD_INT(GL_CLAMP_TO_BORDER);
		PY_DICT_ADD_INT(GL_COMPRESSED_RGB);
		PY_DICT_ADD_INT(GL_COMPRESSED_RGBA);
		PY_DICT_ADD_INT(GL_COMPRESSED_TEXTURE_FORMATS);
		PY_DICT_ADD_INT(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
		PY_DICT_ADD_INT(GL_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_CUBE_MAP);
		PY_DICT_ADD_INT(GL_SAMPLES);
		PY_DICT_ADD_INT(GL_SAMPLE_ALPHA_TO_COVERAGE);
		PY_DICT_ADD_INT(GL_SAMPLE_ALPHA_TO_ONE);
		PY_DICT_ADD_INT(GL_SAMPLE_BUFFERS);
		PY_DICT_ADD_INT(GL_SAMPLE_COVERAGE);
		PY_DICT_ADD_INT(GL_SAMPLE_COVERAGE_INVERT);
		PY_DICT_ADD_INT(GL_SAMPLE_COVERAGE_VALUE);
		PY_DICT_ADD_INT(GL_TEXTURE0);
		PY_DICT_ADD_INT(GL_TEXTURE1);
		PY_DICT_ADD_INT(GL_TEXTURE10);
		PY_DICT_ADD_INT(GL_TEXTURE11);
		PY_DICT_ADD_INT(GL_TEXTURE12);
		PY_DICT_ADD_INT(GL_TEXTURE13);
		PY_DICT_ADD_INT(GL_TEXTURE14);
		PY_DICT_ADD_INT(GL_TEXTURE15);
		PY_DICT_ADD_INT(GL_TEXTURE16);
		PY_DICT_ADD_INT(GL_TEXTURE17);
		PY_DICT_ADD_INT(GL_TEXTURE18);
		PY_DICT_ADD_INT(GL_TEXTURE19);
		PY_DICT_ADD_INT(GL_TEXTURE2);
		PY_DICT_ADD_INT(GL_TEXTURE20);
		PY_DICT_ADD_INT(GL_TEXTURE21);
		PY_DICT_ADD_INT(GL_TEXTURE22);
		PY_DICT_ADD_INT(GL_TEXTURE23);
		PY_DICT_ADD_INT(GL_TEXTURE24);
		PY_DICT_ADD_INT(GL_TEXTURE25);
		PY_DICT_ADD_INT(GL_TEXTURE26);
		PY_DICT_ADD_INT(GL_TEXTURE27);
		PY_DICT_ADD_INT(GL_TEXTURE28);
		PY_DICT_ADD_INT(GL_TEXTURE29);
		PY_DICT_ADD_INT(GL_TEXTURE3);
		PY_DICT_ADD_INT(GL_TEXTURE30);
		PY_DICT_ADD_INT(GL_TEXTURE31);
		PY_DICT_ADD_INT(GL_TEXTURE4);
		PY_DICT_ADD_INT(GL_TEXTURE5);
		PY_DICT_ADD_INT(GL_TEXTURE6);
		PY_DICT_ADD_INT(GL_TEXTURE7);
		PY_DICT_ADD_INT(GL_TEXTURE8);
		PY_DICT_ADD_INT(GL_TEXTURE9);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_CUBE_MAP);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPRESSED);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPRESSED_IMAGE_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPRESSION_HINT);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_POSITIVE_X);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
	}
	/* adding in GL_VERSION_1_3 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_ADD_SIGNED);
		PY_DICT_ADD_INT(GL_CLIENT_ACTIVE_TEXTURE);
		PY_DICT_ADD_INT(GL_COMBINE);
		PY_DICT_ADD_INT(GL_COMBINE_ALPHA);
		PY_DICT_ADD_INT(GL_COMBINE_RGB);
		PY_DICT_ADD_INT(GL_COMPRESSED_ALPHA);
		PY_DICT_ADD_INT(GL_COMPRESSED_INTENSITY);
		PY_DICT_ADD_INT(GL_COMPRESSED_LUMINANCE);
		PY_DICT_ADD_INT(GL_COMPRESSED_LUMINANCE_ALPHA);
		PY_DICT_ADD_INT(GL_CONSTANT);
		PY_DICT_ADD_INT(GL_DOT3_RGB);
		PY_DICT_ADD_INT(GL_DOT3_RGBA);
		PY_DICT_ADD_INT(GL_INTERPOLATE);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_UNITS);
		PY_DICT_ADD_INT(GL_MULTISAMPLE_BIT);
		PY_DICT_ADD_INT(GL_NORMAL_MAP);
		PY_DICT_ADD_INT(GL_OPERAND0_ALPHA);
		PY_DICT_ADD_INT(GL_OPERAND0_RGB);
		PY_DICT_ADD_INT(GL_OPERAND1_ALPHA);
		PY_DICT_ADD_INT(GL_OPERAND1_RGB);
		PY_DICT_ADD_INT(GL_OPERAND2_ALPHA);
		PY_DICT_ADD_INT(GL_OPERAND2_RGB);
		PY_DICT_ADD_INT(GL_PREVIOUS);
		PY_DICT_ADD_INT(GL_PRIMARY_COLOR);
		PY_DICT_ADD_INT(GL_REFLECTION_MAP);
		PY_DICT_ADD_INT(GL_RGB_SCALE);
		PY_DICT_ADD_INT(GL_SOURCE0_ALPHA);
		PY_DICT_ADD_INT(GL_SOURCE0_RGB);
		PY_DICT_ADD_INT(GL_SOURCE1_ALPHA);
		PY_DICT_ADD_INT(GL_SOURCE1_RGB);
		PY_DICT_ADD_INT(GL_SOURCE2_ALPHA);
		PY_DICT_ADD_INT(GL_SOURCE2_RGB);
		PY_DICT_ADD_INT(GL_SUBTRACT);
		PY_DICT_ADD_INT(GL_TRANSPOSE_COLOR_MATRIX);
		PY_DICT_ADD_INT(GL_TRANSPOSE_MODELVIEW_MATRIX);
		PY_DICT_ADD_INT(GL_TRANSPOSE_PROJECTION_MATRIX);
		PY_DICT_ADD_INT(GL_TRANSPOSE_TEXTURE_MATRIX);
	}


	/* GL_VERSION_1_4 */
	{
		PY_DICT_ADD_INT(GL_BLEND_DST_ALPHA);
		PY_DICT_ADD_INT(GL_BLEND_DST_RGB);
		PY_DICT_ADD_INT(GL_BLEND_SRC_ALPHA);
		PY_DICT_ADD_INT(GL_BLEND_SRC_RGB);
		PY_DICT_ADD_INT(GL_CONSTANT_ALPHA);
		PY_DICT_ADD_INT(GL_CONSTANT_COLOR);
		PY_DICT_ADD_INT(GL_DECR_WRAP);
		PY_DICT_ADD_INT(GL_DEPTH_COMPONENT16);
		PY_DICT_ADD_INT(GL_DEPTH_COMPONENT24);
		PY_DICT_ADD_INT(GL_DEPTH_COMPONENT32);
		PY_DICT_ADD_INT(GL_FUNC_ADD);
		PY_DICT_ADD_INT(GL_FUNC_REVERSE_SUBTRACT);
		PY_DICT_ADD_INT(GL_FUNC_SUBTRACT);
		PY_DICT_ADD_INT(GL_INCR_WRAP);
		PY_DICT_ADD_INT(GL_MAX);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_LOD_BIAS);
		PY_DICT_ADD_INT(GL_MIN);
		PY_DICT_ADD_INT(GL_MIRRORED_REPEAT);
		PY_DICT_ADD_INT(GL_ONE_MINUS_CONSTANT_ALPHA);
		PY_DICT_ADD_INT(GL_ONE_MINUS_CONSTANT_COLOR);
		PY_DICT_ADD_INT(GL_POINT_FADE_THRESHOLD_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPARE_FUNC);
		PY_DICT_ADD_INT(GL_TEXTURE_COMPARE_MODE);
		PY_DICT_ADD_INT(GL_TEXTURE_DEPTH_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_LOD_BIAS);
	}
	/* adding in GL_VERSION_1_4 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_COLOR_SUM);
		PY_DICT_ADD_INT(GL_COMPARE_R_TO_TEXTURE);
		PY_DICT_ADD_INT(GL_CURRENT_FOG_COORDINATE);
		PY_DICT_ADD_INT(GL_CURRENT_SECONDARY_COLOR);
		PY_DICT_ADD_INT(GL_DEPTH_TEXTURE_MODE);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_ARRAY);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_SOURCE);
		PY_DICT_ADD_INT(GL_FRAGMENT_DEPTH);
		PY_DICT_ADD_INT(GL_GENERATE_MIPMAP);
		PY_DICT_ADD_INT(GL_GENERATE_MIPMAP_HINT);
		PY_DICT_ADD_INT(GL_POINT_DISTANCE_ATTENUATION);
		PY_DICT_ADD_INT(GL_POINT_SIZE_MAX);
		PY_DICT_ADD_INT(GL_POINT_SIZE_MIN);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY_SIZE);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_FILTER_CONTROL);
	}


	/* GL_VERSION_1_5 */
	{
		PY_DICT_ADD_INT(GL_ARRAY_BUFFER);
		PY_DICT_ADD_INT(GL_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_BUFFER_ACCESS);
		PY_DICT_ADD_INT(GL_BUFFER_MAPPED);
		PY_DICT_ADD_INT(GL_BUFFER_MAP_POINTER);
		PY_DICT_ADD_INT(GL_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_BUFFER_USAGE);
		PY_DICT_ADD_INT(GL_CURRENT_QUERY);
		PY_DICT_ADD_INT(GL_DYNAMIC_COPY);
		PY_DICT_ADD_INT(GL_DYNAMIC_DRAW);
		PY_DICT_ADD_INT(GL_DYNAMIC_READ);
		PY_DICT_ADD_INT(GL_ELEMENT_ARRAY_BUFFER);
		PY_DICT_ADD_INT(GL_ELEMENT_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_QUERY_COUNTER_BITS);
		PY_DICT_ADD_INT(GL_QUERY_RESULT);
		PY_DICT_ADD_INT(GL_QUERY_RESULT_AVAILABLE);
		PY_DICT_ADD_INT(GL_READ_ONLY);
		PY_DICT_ADD_INT(GL_READ_WRITE);
		PY_DICT_ADD_INT(GL_SAMPLES_PASSED);
		PY_DICT_ADD_INT(GL_SRC1_ALPHA);
		PY_DICT_ADD_INT(GL_STATIC_COPY);
		PY_DICT_ADD_INT(GL_STATIC_DRAW);
		PY_DICT_ADD_INT(GL_STATIC_READ);
		PY_DICT_ADD_INT(GL_STREAM_COPY);
		PY_DICT_ADD_INT(GL_STREAM_DRAW);
		PY_DICT_ADD_INT(GL_STREAM_READ);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_WRITE_ONLY);
	}
	/* adding in GL_VERSION_1_5 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_COLOR_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_CURRENT_FOG_COORD);
		PY_DICT_ADD_INT(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_FOG_COORD);
		PY_DICT_ADD_INT(GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_FOG_COORD_ARRAY);
		PY_DICT_ADD_INT(GL_FOG_COORD_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_FOG_COORD_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_FOG_COORD_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_FOG_COORD_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_FOG_COORD_SRC);
		PY_DICT_ADD_INT(GL_INDEX_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_NORMAL_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_SRC0_ALPHA);
		PY_DICT_ADD_INT(GL_SRC0_RGB);
		PY_DICT_ADD_INT(GL_SRC1_RGB);
		PY_DICT_ADD_INT(GL_SRC2_ALPHA);
		PY_DICT_ADD_INT(GL_SRC2_RGB);
		PY_DICT_ADD_INT(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_WEIGHT_ARRAY_BUFFER_BINDING);
	}


	/* GL_VERSION_2_0 */
	{
		PY_DICT_ADD_INT(GL_ACTIVE_ATTRIBUTES);
		PY_DICT_ADD_INT(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);
		PY_DICT_ADD_INT(GL_ACTIVE_UNIFORMS);
		PY_DICT_ADD_INT(GL_ACTIVE_UNIFORM_MAX_LENGTH);
		PY_DICT_ADD_INT(GL_ATTACHED_SHADERS);
		PY_DICT_ADD_INT(GL_BLEND_EQUATION_ALPHA);
		PY_DICT_ADD_INT(GL_BLEND_EQUATION_RGB);
		PY_DICT_ADD_INT(GL_BOOL);
		PY_DICT_ADD_INT(GL_BOOL_VEC2);
		PY_DICT_ADD_INT(GL_BOOL_VEC3);
		PY_DICT_ADD_INT(GL_BOOL_VEC4);
		PY_DICT_ADD_INT(GL_COMPILE_STATUS);
		PY_DICT_ADD_INT(GL_CURRENT_PROGRAM);
		PY_DICT_ADD_INT(GL_CURRENT_VERTEX_ATTRIB);
		PY_DICT_ADD_INT(GL_DELETE_STATUS);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER0);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER1);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER10);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER11);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER12);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER13);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER14);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER15);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER2);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER3);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER4);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER5);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER6);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER7);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER8);
		PY_DICT_ADD_INT(GL_DRAW_BUFFER9);
		PY_DICT_ADD_INT(GL_FLOAT_MAT2);
		PY_DICT_ADD_INT(GL_FLOAT_MAT3);
		PY_DICT_ADD_INT(GL_FLOAT_MAT4);
		PY_DICT_ADD_INT(GL_FLOAT_VEC2);
		PY_DICT_ADD_INT(GL_FLOAT_VEC3);
		PY_DICT_ADD_INT(GL_FLOAT_VEC4);
		PY_DICT_ADD_INT(GL_FRAGMENT_SHADER);
		PY_DICT_ADD_INT(GL_FRAGMENT_SHADER_DERIVATIVE_HINT);
		PY_DICT_ADD_INT(GL_INFO_LOG_LENGTH);
		PY_DICT_ADD_INT(GL_INT_VEC2);
		PY_DICT_ADD_INT(GL_INT_VEC3);
		PY_DICT_ADD_INT(GL_INT_VEC4);
		PY_DICT_ADD_INT(GL_LINK_STATUS);
		PY_DICT_ADD_INT(GL_LOWER_LEFT);
		PY_DICT_ADD_INT(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
		PY_DICT_ADD_INT(GL_MAX_DRAW_BUFFERS);
		PY_DICT_ADD_INT(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_IMAGE_UNITS);
		PY_DICT_ADD_INT(GL_MAX_VARYING_FLOATS);
		PY_DICT_ADD_INT(GL_MAX_VERTEX_ATTRIBS);
		PY_DICT_ADD_INT(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
		PY_DICT_ADD_INT(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_POINT_SPRITE_COORD_ORIGIN);
		PY_DICT_ADD_INT(GL_SAMPLER_1D);
		PY_DICT_ADD_INT(GL_SAMPLER_1D_SHADOW);
		PY_DICT_ADD_INT(GL_SAMPLER_2D);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_SHADOW);
		PY_DICT_ADD_INT(GL_SAMPLER_3D);
		PY_DICT_ADD_INT(GL_SAMPLER_CUBE);
		PY_DICT_ADD_INT(GL_SHADER_SOURCE_LENGTH);
		PY_DICT_ADD_INT(GL_SHADER_TYPE);
		PY_DICT_ADD_INT(GL_SHADING_LANGUAGE_VERSION);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_FAIL);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_FUNC);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_PASS_DEPTH_FAIL);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_PASS_DEPTH_PASS);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_REF);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_VALUE_MASK);
		PY_DICT_ADD_INT(GL_STENCIL_BACK_WRITEMASK);
		PY_DICT_ADD_INT(GL_UPPER_LEFT);
		PY_DICT_ADD_INT(GL_VALIDATE_STATUS);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_ENABLED);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_POINTER);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_SIZE);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_TYPE);
		PY_DICT_ADD_INT(GL_VERTEX_PROGRAM_POINT_SIZE);
		PY_DICT_ADD_INT(GL_VERTEX_SHADER);
	}
	/* adding in GL_VERSION_2_0 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_COORD_REPLACE);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_COORDS);
		PY_DICT_ADD_INT(GL_POINT_SPRITE);
		PY_DICT_ADD_INT(GL_VERTEX_PROGRAM_TWO_SIDE);
	}


	/* GL_VERSION_2_1 */
	{
		PY_DICT_ADD_INT(GL_COMPRESSED_SRGB);
		PY_DICT_ADD_INT(GL_COMPRESSED_SRGB_ALPHA);
		PY_DICT_ADD_INT(GL_FLOAT_MAT2x3);
		PY_DICT_ADD_INT(GL_FLOAT_MAT2x4);
		PY_DICT_ADD_INT(GL_FLOAT_MAT3x2);
		PY_DICT_ADD_INT(GL_FLOAT_MAT3x4);
		PY_DICT_ADD_INT(GL_FLOAT_MAT4x2);
		PY_DICT_ADD_INT(GL_FLOAT_MAT4x3);
		PY_DICT_ADD_INT(GL_PIXEL_PACK_BUFFER);
		PY_DICT_ADD_INT(GL_PIXEL_PACK_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_PIXEL_UNPACK_BUFFER);
		PY_DICT_ADD_INT(GL_PIXEL_UNPACK_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_SRGB);
		PY_DICT_ADD_INT(GL_SRGB8);
		PY_DICT_ADD_INT(GL_SRGB8_ALPHA8);
		PY_DICT_ADD_INT(GL_SRGB_ALPHA);
	}
	/* adding in GL_VERSION_2_1 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_COMPRESSED_SLUMINANCE);
		PY_DICT_ADD_INT(GL_COMPRESSED_SLUMINANCE_ALPHA);
		PY_DICT_ADD_INT(GL_CURRENT_RASTER_SECONDARY_COLOR);
		PY_DICT_ADD_INT(GL_SLUMINANCE);
		PY_DICT_ADD_INT(GL_SLUMINANCE8);
		PY_DICT_ADD_INT(GL_SLUMINANCE8_ALPHA8);
		PY_DICT_ADD_INT(GL_SLUMINANCE_ALPHA);
	}

	/* GL_VERSION_3_0 */
	{
		PY_DICT_ADD_INT(GL_BGRA_INTEGER);
		PY_DICT_ADD_INT(GL_BGR_INTEGER);
		PY_DICT_ADD_INT(GL_BLUE_INTEGER);
		PY_DICT_ADD_INT(GL_BUFFER_ACCESS_FLAGS);
		PY_DICT_ADD_INT(GL_BUFFER_MAP_LENGTH);
		PY_DICT_ADD_INT(GL_BUFFER_MAP_OFFSET);
		PY_DICT_ADD_INT(GL_CLAMP_READ_COLOR);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE0);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE1);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE2);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE3);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE4);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE5);
#if 0
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE6);
		PY_DICT_ADD_INT(GL_CLIP_DISTANCE7);
#endif
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT0);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT1);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT2);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT3);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT4);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT5);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT6);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT7);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT8);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT9);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT10);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT11);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT12);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT13);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT14);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT15);
#if 0
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT16);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT17);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT18);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT19);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT20);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT21);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT22);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT23);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT24);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT25);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT26);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT27);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT28);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT29);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT30);
		PY_DICT_ADD_INT(GL_COLOR_ATTACHMENT31);
#endif
		PY_DICT_ADD_INT(GL_COMPARE_REF_TO_TEXTURE);
		PY_DICT_ADD_INT(GL_COMPRESSED_RED);
		PY_DICT_ADD_INT(GL_COMPRESSED_RED_RGTC1);
		PY_DICT_ADD_INT(GL_COMPRESSED_RG);
		PY_DICT_ADD_INT(GL_COMPRESSED_RG_RGTC2);
		PY_DICT_ADD_INT(GL_COMPRESSED_SIGNED_RED_RGTC1);
		PY_DICT_ADD_INT(GL_COMPRESSED_SIGNED_RG_RGTC2);
		PY_DICT_ADD_INT(GL_CONTEXT_FLAGS);
		PY_DICT_ADD_INT(GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT);
		PY_DICT_ADD_INT(GL_DEPTH24_STENCIL8);
		PY_DICT_ADD_INT(GL_DEPTH32F_STENCIL8);
		PY_DICT_ADD_INT(GL_DEPTH_ATTACHMENT);
		PY_DICT_ADD_INT(GL_DEPTH_COMPONENT32F);
		PY_DICT_ADD_INT(GL_DEPTH_STENCIL);
		PY_DICT_ADD_INT(GL_DEPTH_STENCIL_ATTACHMENT);
		PY_DICT_ADD_INT(GL_DRAW_FRAMEBUFFER);
		PY_DICT_ADD_INT(GL_DRAW_FRAMEBUFFER_BINDING);
		PY_DICT_ADD_INT(GL_FIXED_ONLY);
		PY_DICT_ADD_INT(GL_FLOAT_32_UNSIGNED_INT_24_8_REV);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_BINDING);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_COMPLETE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_DEFAULT);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_SRGB);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_UNDEFINED);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_UNSUPPORTED);
		PY_DICT_ADD_INT(GL_GREEN_INTEGER);
		PY_DICT_ADD_INT(GL_HALF_FLOAT);
		PY_DICT_ADD_INT(GL_INDEX);
		PY_DICT_ADD_INT(GL_INTERLEAVED_ATTRIBS);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_1D);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_1D_ARRAY);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_2D);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_2D_ARRAY);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_3D);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_CUBE);
		PY_DICT_ADD_INT(GL_INVALID_FRAMEBUFFER_OPERATION);
		PY_DICT_ADD_INT(GL_MAJOR_VERSION);
		PY_DICT_ADD_INT(GL_MAP_FLUSH_EXPLICIT_BIT);
		PY_DICT_ADD_INT(GL_MAP_INVALIDATE_BUFFER_BIT);
		PY_DICT_ADD_INT(GL_MAP_INVALIDATE_RANGE_BIT);
		PY_DICT_ADD_INT(GL_MAP_READ_BIT);
		PY_DICT_ADD_INT(GL_MAP_UNSYNCHRONIZED_BIT);
		PY_DICT_ADD_INT(GL_MAP_WRITE_BIT);
		PY_DICT_ADD_INT(GL_MAX_ARRAY_TEXTURE_LAYERS);
		PY_DICT_ADD_INT(GL_MAX_CLIP_DISTANCES);
		PY_DICT_ADD_INT(GL_MAX_COLOR_ATTACHMENTS);
		PY_DICT_ADD_INT(GL_MAX_PROGRAM_TEXEL_OFFSET);
		PY_DICT_ADD_INT(GL_MAX_RENDERBUFFER_SIZE);
		PY_DICT_ADD_INT(GL_MAX_SAMPLES);
		PY_DICT_ADD_INT(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
		PY_DICT_ADD_INT(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_VARYING_COMPONENTS);
		PY_DICT_ADD_INT(GL_MINOR_VERSION);
		PY_DICT_ADD_INT(GL_MIN_PROGRAM_TEXEL_OFFSET);
		PY_DICT_ADD_INT(GL_NUM_EXTENSIONS);
		PY_DICT_ADD_INT(GL_PRIMITIVES_GENERATED);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_1D_ARRAY);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_2D_ARRAY);
		PY_DICT_ADD_INT(GL_QUERY_BY_REGION_NO_WAIT);
		PY_DICT_ADD_INT(GL_QUERY_BY_REGION_WAIT);
		PY_DICT_ADD_INT(GL_QUERY_NO_WAIT);
		PY_DICT_ADD_INT(GL_QUERY_WAIT);
		PY_DICT_ADD_INT(GL_R11F_G11F_B10F);
		PY_DICT_ADD_INT(GL_R16);
		PY_DICT_ADD_INT(GL_R16F);
		PY_DICT_ADD_INT(GL_R16I);
		PY_DICT_ADD_INT(GL_R16UI);
		PY_DICT_ADD_INT(GL_R32F);
		PY_DICT_ADD_INT(GL_R32I);
		PY_DICT_ADD_INT(GL_R32UI);
		PY_DICT_ADD_INT(GL_R8);
		PY_DICT_ADD_INT(GL_R8I);
		PY_DICT_ADD_INT(GL_R8UI);
		PY_DICT_ADD_INT(GL_RASTERIZER_DISCARD);
		PY_DICT_ADD_INT(GL_READ_FRAMEBUFFER);
		PY_DICT_ADD_INT(GL_READ_FRAMEBUFFER_BINDING);
		PY_DICT_ADD_INT(GL_RED_INTEGER);
		PY_DICT_ADD_INT(GL_RENDERBUFFER);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_ALPHA_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_BINDING);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_BLUE_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_DEPTH_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_GREEN_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_HEIGHT);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_INTERNAL_FORMAT);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_RED_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_SAMPLES);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_STENCIL_SIZE);
		PY_DICT_ADD_INT(GL_RENDERBUFFER_WIDTH);
		PY_DICT_ADD_INT(GL_RG);
		PY_DICT_ADD_INT(GL_RG16);
		PY_DICT_ADD_INT(GL_RG16F);
		PY_DICT_ADD_INT(GL_RG16I);
		PY_DICT_ADD_INT(GL_RG16UI);
		PY_DICT_ADD_INT(GL_RG32F);
		PY_DICT_ADD_INT(GL_RG32I);
		PY_DICT_ADD_INT(GL_RG32UI);
		PY_DICT_ADD_INT(GL_RG8);
		PY_DICT_ADD_INT(GL_RG8I);
		PY_DICT_ADD_INT(GL_RG8UI);
		PY_DICT_ADD_INT(GL_RGB16F);
		PY_DICT_ADD_INT(GL_RGB16I);
		PY_DICT_ADD_INT(GL_RGB16UI);
		PY_DICT_ADD_INT(GL_RGB32F);
		PY_DICT_ADD_INT(GL_RGB32I);
		PY_DICT_ADD_INT(GL_RGB32UI);
		PY_DICT_ADD_INT(GL_RGB8I);
		PY_DICT_ADD_INT(GL_RGB8UI);
		PY_DICT_ADD_INT(GL_RGB9_E5);
		PY_DICT_ADD_INT(GL_RGBA16F);
		PY_DICT_ADD_INT(GL_RGBA16I);
		PY_DICT_ADD_INT(GL_RGBA16UI);
		PY_DICT_ADD_INT(GL_RGBA32F);
		PY_DICT_ADD_INT(GL_RGBA32I);
		PY_DICT_ADD_INT(GL_RGBA32UI);
		PY_DICT_ADD_INT(GL_RGBA8I);
		PY_DICT_ADD_INT(GL_RGBA8UI);
		PY_DICT_ADD_INT(GL_RGBA_INTEGER);
		PY_DICT_ADD_INT(GL_RGB_INTEGER);
		PY_DICT_ADD_INT(GL_RG_INTEGER);
		PY_DICT_ADD_INT(GL_SAMPLER_1D_ARRAY);
		PY_DICT_ADD_INT(GL_SAMPLER_1D_ARRAY_SHADOW);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_ARRAY);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_ARRAY_SHADOW);
		PY_DICT_ADD_INT(GL_SAMPLER_CUBE_SHADOW);
		PY_DICT_ADD_INT(GL_SEPARATE_ATTRIBS);
		PY_DICT_ADD_INT(GL_STENCIL_ATTACHMENT);
		PY_DICT_ADD_INT(GL_STENCIL_INDEX1);
		PY_DICT_ADD_INT(GL_STENCIL_INDEX16);
		PY_DICT_ADD_INT(GL_STENCIL_INDEX4);
		PY_DICT_ADD_INT(GL_STENCIL_INDEX8);
		PY_DICT_ADD_INT(GL_TEXTURE_1D_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_2D_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_ALPHA_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_1D_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_2D_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_BLUE_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_DEPTH_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_GREEN_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_RED_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_SHARED_SIZE);
		PY_DICT_ADD_INT(GL_TEXTURE_STENCIL_SIZE);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_BUFFER);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_BUFFER_MODE);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_BUFFER_START);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_VARYINGS);
		PY_DICT_ADD_INT(GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_10F_11F_11F_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_24_8);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_5_9_9_9_REV);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_1D);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_2D);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_3D);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_CUBE);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_VEC2);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_VEC3);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_VEC4);
		PY_DICT_ADD_INT(GL_UNSIGNED_NORMALIZED);
		PY_DICT_ADD_INT(GL_VERTEX_ARRAY_BINDING);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_INTEGER);
	}
	/* adding in GL_VERSION_3_0 removed from core profile */
	if (use_deprecated == true) {
		PY_DICT_ADD_INT(GL_ALPHA_INTEGER);
		PY_DICT_ADD_INT(GL_CLAMP_FRAGMENT_COLOR);
		PY_DICT_ADD_INT(GL_CLAMP_VERTEX_COLOR);
		PY_DICT_ADD_INT(GL_TEXTURE_INTENSITY_TYPE);
		PY_DICT_ADD_INT(GL_TEXTURE_LUMINANCE_TYPE);
	}

	/* GL_VERSION_3_1 */
	{
		PY_DICT_ADD_INT(GL_ACTIVE_UNIFORM_BLOCKS);
		PY_DICT_ADD_INT(GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH);
		PY_DICT_ADD_INT(GL_COPY_READ_BUFFER);
		PY_DICT_ADD_INT(GL_COPY_WRITE_BUFFER);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_2D_RECT);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_BUFFER);
		PY_DICT_ADD_INT(GL_INVALID_INDEX);
		PY_DICT_ADD_INT(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_COMBINED_UNIFORM_BLOCKS);
		PY_DICT_ADD_INT(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_FRAGMENT_UNIFORM_BLOCKS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_UNIFORM_BLOCKS);
		PY_DICT_ADD_INT(GL_MAX_RECTANGLE_TEXTURE_SIZE);
		PY_DICT_ADD_INT(GL_MAX_TEXTURE_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_MAX_UNIFORM_BLOCK_SIZE);
		PY_DICT_ADD_INT(GL_MAX_UNIFORM_BUFFER_BINDINGS);
		PY_DICT_ADD_INT(GL_MAX_VERTEX_UNIFORM_BLOCKS);
		PY_DICT_ADD_INT(GL_PRIMITIVE_RESTART);
		PY_DICT_ADD_INT(GL_PRIMITIVE_RESTART_INDEX);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_RECTANGLE);
		PY_DICT_ADD_INT(GL_R16_SNORM);
		PY_DICT_ADD_INT(GL_R8_SNORM);
		PY_DICT_ADD_INT(GL_RG16_SNORM);
		PY_DICT_ADD_INT(GL_RG8_SNORM);
		PY_DICT_ADD_INT(GL_RGB16_SNORM);
		PY_DICT_ADD_INT(GL_RGB8_SNORM);
		PY_DICT_ADD_INT(GL_RGBA16_SNORM);
		PY_DICT_ADD_INT(GL_RGBA8_SNORM);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_RECT);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_RECT_SHADOW);
		PY_DICT_ADD_INT(GL_SAMPLER_BUFFER);
		PY_DICT_ADD_INT(GL_SIGNED_NORMALIZED);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_BUFFER);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_RECTANGLE);
		PY_DICT_ADD_INT(GL_TEXTURE_BUFFER);
		PY_DICT_ADD_INT(GL_TEXTURE_BUFFER_DATA_STORE_BINDING);
		PY_DICT_ADD_INT(GL_TEXTURE_RECTANGLE);
		PY_DICT_ADD_INT(GL_UNIFORM_ARRAY_STRIDE);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_BINDING);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_DATA_SIZE);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_INDEX);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_NAME_LENGTH);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER);
		PY_DICT_ADD_INT(GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER);
		PY_DICT_ADD_INT(GL_UNIFORM_BUFFER);
		PY_DICT_ADD_INT(GL_UNIFORM_BUFFER_BINDING);
		PY_DICT_ADD_INT(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
		PY_DICT_ADD_INT(GL_UNIFORM_BUFFER_SIZE);
		PY_DICT_ADD_INT(GL_UNIFORM_BUFFER_START);
		PY_DICT_ADD_INT(GL_UNIFORM_IS_ROW_MAJOR);
		PY_DICT_ADD_INT(GL_UNIFORM_MATRIX_STRIDE);
		PY_DICT_ADD_INT(GL_UNIFORM_NAME_LENGTH);
		PY_DICT_ADD_INT(GL_UNIFORM_OFFSET);
		PY_DICT_ADD_INT(GL_UNIFORM_SIZE);
		PY_DICT_ADD_INT(GL_UNIFORM_TYPE);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_2D_RECT);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_BUFFER);
	}


	/* GL_VERSION_3_2 */
	{
		PY_DICT_ADD_INT(GL_ALREADY_SIGNALED);
		PY_DICT_ADD_INT(GL_CONDITION_SATISFIED);
		PY_DICT_ADD_INT(GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
		PY_DICT_ADD_INT(GL_CONTEXT_CORE_PROFILE_BIT);
		PY_DICT_ADD_INT(GL_CONTEXT_PROFILE_MASK);
		PY_DICT_ADD_INT(GL_DEPTH_CLAMP);
		PY_DICT_ADD_INT(GL_FIRST_VERTEX_CONVENTION);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_ATTACHMENT_LAYERED);
		PY_DICT_ADD_INT(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);
		PY_DICT_ADD_INT(GL_GEOMETRY_INPUT_TYPE);
		PY_DICT_ADD_INT(GL_GEOMETRY_OUTPUT_TYPE);
		PY_DICT_ADD_INT(GL_GEOMETRY_SHADER);
		PY_DICT_ADD_INT(GL_GEOMETRY_VERTICES_OUT);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_LAST_VERTEX_CONVENTION);
		PY_DICT_ADD_INT(GL_LINES_ADJACENCY);
		PY_DICT_ADD_INT(GL_LINE_STRIP_ADJACENCY);
		PY_DICT_ADD_INT(GL_MAX_COLOR_TEXTURE_SAMPLES);
		PY_DICT_ADD_INT(GL_MAX_DEPTH_TEXTURE_SAMPLES);
		PY_DICT_ADD_INT(GL_MAX_FRAGMENT_INPUT_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_INPUT_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_OUTPUT_VERTICES);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS);
		PY_DICT_ADD_INT(GL_MAX_INTEGER_SAMPLES);
		PY_DICT_ADD_INT(GL_MAX_SAMPLE_MASK_WORDS);
		PY_DICT_ADD_INT(GL_MAX_SERVER_WAIT_TIMEOUT);
		PY_DICT_ADD_INT(GL_MAX_VERTEX_OUTPUT_COMPONENTS);
		PY_DICT_ADD_INT(GL_OBJECT_TYPE);
		PY_DICT_ADD_INT(GL_PROGRAM_POINT_SIZE);
		PY_DICT_ADD_INT(GL_PROVOKING_VERTEX);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_SAMPLER_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_SAMPLE_MASK);
		PY_DICT_ADD_INT(GL_SAMPLE_MASK_VALUE);
		PY_DICT_ADD_INT(GL_SAMPLE_POSITION);
		PY_DICT_ADD_INT(GL_SIGNALED);
		PY_DICT_ADD_INT(GL_SYNC_CONDITION);
		PY_DICT_ADD_INT(GL_SYNC_FENCE);
		PY_DICT_ADD_INT(GL_SYNC_FLAGS);
		PY_DICT_ADD_INT(GL_SYNC_FLUSH_COMMANDS_BIT);
		PY_DICT_ADD_INT(GL_SYNC_GPU_COMMANDS_COMPLETE);
		PY_DICT_ADD_INT(GL_SYNC_STATUS);
		PY_DICT_ADD_INT(GL_TEXTURE_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		PY_DICT_ADD_INT(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS);
		PY_DICT_ADD_INT(GL_TEXTURE_SAMPLES);
		PY_DICT_ADD_INT(GL_TIMEOUT_EXPIRED);
		PY_DICT_ADD_INT64(GL_TIMEOUT_IGNORED);
		PY_DICT_ADD_INT(GL_TRIANGLES_ADJACENCY);
		PY_DICT_ADD_INT(GL_TRIANGLE_STRIP_ADJACENCY);
		PY_DICT_ADD_INT(GL_UNSIGNALED);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE);
		PY_DICT_ADD_INT(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY);
		PY_DICT_ADD_INT(GL_WAIT_FAILED);
	}

	/* GL_VERSION_3_3 */
	{
		PY_DICT_ADD_INT(GL_ANY_SAMPLES_PASSED);
		PY_DICT_ADD_INT(GL_INT_2_10_10_10_REV);
		PY_DICT_ADD_INT(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS);
		PY_DICT_ADD_INT(GL_ONE_MINUS_SRC1_ALPHA);
		PY_DICT_ADD_INT(GL_ONE_MINUS_SRC1_COLOR);
		PY_DICT_ADD_INT(GL_RGB10_A2UI);
		PY_DICT_ADD_INT(GL_SAMPLER_BINDING);
		PY_DICT_ADD_INT(GL_SRC1_COLOR);
		PY_DICT_ADD_INT(GL_TEXTURE_SWIZZLE_A);
		PY_DICT_ADD_INT(GL_TEXTURE_SWIZZLE_B);
		PY_DICT_ADD_INT(GL_TEXTURE_SWIZZLE_G);
		PY_DICT_ADD_INT(GL_TEXTURE_SWIZZLE_R);
		PY_DICT_ADD_INT(GL_TEXTURE_SWIZZLE_RGBA);
		PY_DICT_ADD_INT(GL_TIMESTAMP);
		PY_DICT_ADD_INT(GL_TIME_ELAPSED);
		PY_DICT_ADD_INT(GL_VERTEX_ATTRIB_ARRAY_DIVISOR);
	}

	return submodule;
}

static PyObject *Method_ShaderSource(PyObject *UNUSED(self), PyObject *args)
{
	unsigned int shader;
	const char *source;

	if (!PyArg_ParseTuple(args, "Is", &shader, &source))
		return NULL;

	glShaderSource(shader, 1, (const char **)&source, NULL);

	Py_RETURN_NONE;
}


/** \} */