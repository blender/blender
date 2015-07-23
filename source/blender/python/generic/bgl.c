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

#if 0
#define GLint64P_str      "O!"
#define GLint64P_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLint64P_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLint64P_def(number)  Buffer *bgl_buffer##number
#endif

#if 0
#define GLenumP_str      "O!"
#define GLenumP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLenumP_ref(number)  &BGL_bufferType, &bgl_buffer##number
#define GLenumP_def(number)  Buffer *bgl_buffer##number
#endif

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

#define GLclampfP_str     "O!"
#define GLclampfP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLclampfP_ref(number) &BGL_bufferType, &bgl_buffer##number
#define GLclampfP_def(number) Buffer *bgl_buffer##number

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
#if 0
#define GLsizeiptr_str       "n"
#define GLsizeiptr_var(num)    bgl_var##num
#define GLsizeiptr_ref(num)    &bgl_var##num
#define GLsizeiptr_def(num)    size_t GLsizeiptr_var(num)
#endif

/* typedef int GLintptr; */
#if 0
#define GLintptr_str       "n"
#define GLintptr_var(num)    bgl_var##num
#define GLintptr_ref(num)    &bgl_var##num
#define GLintptr_def(num)    size_t GLintptr_var(num)
#endif

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
#define GLclampf_str      "f"
#define GLclampf_var(num)   bgl_var##num
#define GLclampf_ref(num)   &bgl_var##num
#define GLclampf_def(num)   float GLclampf_var(num)

/* typedef double GLdouble; */
#define GLdouble_str      "d"
#define GLdouble_var(num)   bgl_var##num
#define GLdouble_ref(num)   &bgl_var##num
#define GLdouble_def(num)   double GLdouble_var(num)

/* typedef double GLclampd; */
#define GLclampd_str      "d"
#define GLclampd_var(num)   bgl_var##num
#define GLclampd_ref(num)   &bgl_var##num
#define GLclampd_def(num)   double GLclampd_var(num)

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
#if 0
#define _arg_def11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_def10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10); a11##_def(11)
#endif
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
#if 0
#define _arg_var11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_var10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10), a11##_var(11)
#endif
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
#if 0
#define _arg_ref11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_ref10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10), a11##_ref(11)
#endif
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
#if 0
#define _arg_str11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
        _arg_str10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) a11##_str
#endif
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

/** \name Forward Declarations
 * \{ */

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

/** \} */


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

/*@ Buffer sequence methods */

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
		case GL_INT:    typestr = "GL_BYTE"; break;
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

BGL_Wrap(Accum,          void,       (GLenum, GLfloat))
BGL_Wrap(ActiveTexture,  void,       (GLenum))
BGL_Wrap(AlphaFunc,      void,       (GLenum, GLclampf))
BGL_Wrap(AreTexturesResident,  GLboolean,  (GLsizei, GLuintP, GLbooleanP))
BGL_Wrap(AttachShader,   void,       (GLuint, GLuint))
BGL_Wrap(Begin,          void,       (GLenum))
BGL_Wrap(BindTexture,    void,       (GLenum, GLuint))
BGL_Wrap(Bitmap,         void,       (GLsizei, GLsizei, GLfloat,
                                      GLfloat, GLfloat, GLfloat, GLubyteP))
BGL_Wrap(BlendFunc,        void,     (GLenum, GLenum))
BGL_Wrap(CallList,         void,     (GLuint))
BGL_Wrap(CallLists,        void,     (GLsizei, GLenum, GLvoidP))
BGL_Wrap(Clear,            void,     (GLbitfield))
BGL_Wrap(ClearAccum,       void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(ClearColor,       void,     (GLclampf, GLclampf, GLclampf, GLclampf))
BGL_Wrap(ClearDepth,       void,     (GLclampd))
BGL_Wrap(ClearIndex,       void,     (GLfloat))
BGL_Wrap(ClearStencil,     void,     (GLint))
BGL_Wrap(ClipPlane,        void,     (GLenum, GLdoubleP))
BGL_Wrap(Color3b,          void,     (GLbyte, GLbyte, GLbyte))
BGL_Wrap(Color3bv,         void,     (GLbyteP))
BGL_Wrap(Color3d,          void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Color3dv,         void,     (GLdoubleP))
BGL_Wrap(Color3f,          void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Color3fv,         void,     (GLfloatP))
BGL_Wrap(Color3i,          void,     (GLint, GLint, GLint))
BGL_Wrap(Color3iv,         void,     (GLintP))
BGL_Wrap(Color3s,          void,     (GLshort, GLshort, GLshort))
BGL_Wrap(Color3sv,         void,     (GLshortP))
BGL_Wrap(Color3ub,         void,     (GLubyte, GLubyte, GLubyte))
BGL_Wrap(Color3ubv,        void,     (GLubyteP))
BGL_Wrap(Color3ui,         void,     (GLuint, GLuint, GLuint))
BGL_Wrap(Color3uiv,        void,     (GLuintP))
BGL_Wrap(Color3us,         void,     (GLushort, GLushort, GLushort))
BGL_Wrap(Color3usv,        void,     (GLushortP))
BGL_Wrap(Color4b,          void,     (GLbyte, GLbyte, GLbyte, GLbyte))
BGL_Wrap(Color4bv,         void,     (GLbyteP))
BGL_Wrap(Color4d,          void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Color4dv,         void,     (GLdoubleP))
BGL_Wrap(Color4f,          void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Color4fv,         void,     (GLfloatP))
BGL_Wrap(Color4i,          void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(Color4iv,         void,     (GLintP))
BGL_Wrap(Color4s,          void,     (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Color4sv,         void,     (GLshortP))
BGL_Wrap(Color4ub,         void,     (GLubyte, GLubyte, GLubyte, GLubyte))
BGL_Wrap(Color4ubv,        void,     (GLubyteP))
BGL_Wrap(Color4ui,         void,     (GLuint, GLuint, GLuint, GLuint))
BGL_Wrap(Color4uiv,        void,     (GLuintP))
BGL_Wrap(Color4us,         void,     (GLushort, GLushort, GLushort, GLushort))
BGL_Wrap(Color4usv,        void,     (GLushortP))
BGL_Wrap(ColorMask,        void,     (GLboolean, GLboolean, GLboolean, GLboolean))
BGL_Wrap(ColorMaterial,    void,     (GLenum, GLenum))
BGL_Wrap(CompileShader,    void,     (GLuint))
BGL_Wrap(CopyPixels,       void,     (GLint, GLint, GLsizei, GLsizei, GLenum))
BGL_Wrap(CopyTexImage2D,   void,     (GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint))
BGL_Wrap(CreateProgram,    GLuint,   (void))
BGL_Wrap(CreateShader,     GLuint,   (GLenum))
BGL_Wrap(CullFace,         void,     (GLenum))
BGL_Wrap(DeleteLists,      void,     (GLuint, GLsizei))
BGL_Wrap(DeleteProgram,    void,     (GLuint))
BGL_Wrap(DeleteShader,     void,     (GLuint))
BGL_Wrap(DeleteTextures,   void,     (GLsizei, GLuintP))
BGL_Wrap(DepthFunc,        void,     (GLenum))
BGL_Wrap(DepthMask,        void,     (GLboolean))
BGL_Wrap(DepthRange,       void,     (GLclampd, GLclampd))
BGL_Wrap(DetachShader,     void,     (GLuint, GLuint))
BGL_Wrap(Disable,          void,     (GLenum))
BGL_Wrap(DrawBuffer,       void,     (GLenum))
BGL_Wrap(DrawPixels,       void,     (GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(EdgeFlag,         void,     (GLboolean))
BGL_Wrap(EdgeFlagv,        void,     (GLbooleanP))
BGL_Wrap(Enable,           void,     (GLenum))
BGL_Wrap(End,              void,     (void))
BGL_Wrap(EndList,          void,     (void))
BGL_Wrap(EvalCoord1d,      void,     (GLdouble))
BGL_Wrap(EvalCoord1dv,     void,     (GLdoubleP))
BGL_Wrap(EvalCoord1f,      void,     (GLfloat))
BGL_Wrap(EvalCoord1fv,     void,     (GLfloatP))
BGL_Wrap(EvalCoord2d,      void,     (GLdouble, GLdouble))
BGL_Wrap(EvalCoord2dv,     void,     (GLdoubleP))
BGL_Wrap(EvalCoord2f,      void,     (GLfloat, GLfloat))
BGL_Wrap(EvalCoord2fv,     void,     (GLfloatP))
BGL_Wrap(EvalMesh1,        void,     (GLenum, GLint, GLint))
BGL_Wrap(EvalMesh2,        void,     (GLenum, GLint, GLint, GLint, GLint))
BGL_Wrap(EvalPoint1,       void,     (GLint))
BGL_Wrap(EvalPoint2,       void,     (GLint, GLint))
BGL_Wrap(FeedbackBuffer,   void,     (GLsizei, GLenum, GLfloatP))
BGL_Wrap(Finish,           void,     (void))
BGL_Wrap(Flush,            void,     (void))
BGL_Wrap(Fogf,             void,     (GLenum, GLfloat))
BGL_Wrap(Fogfv,            void,     (GLenum, GLfloatP))
BGL_Wrap(Fogi,             void,     (GLenum, GLint))
BGL_Wrap(Fogiv,            void,     (GLenum, GLintP))
BGL_Wrap(FrontFace,        void,     (GLenum))
BGL_Wrap(Frustum,          void,     (GLdouble, GLdouble,
                                      GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(GenLists,         GLuint,   (GLsizei))
BGL_Wrap(GenTextures,      void,   (GLsizei, GLuintP))
BGL_Wrap(GetAttachedShaders, void,   (GLuint, GLsizei, GLsizeiP, GLuintP))
BGL_Wrap(GetBooleanv,      void,     (GLenum, GLbooleanP))
BGL_Wrap(GetClipPlane,     void,     (GLenum, GLdoubleP))
BGL_Wrap(GetDoublev,       void,     (GLenum, GLdoubleP))
BGL_Wrap(GetError,         GLenum,   (void))
BGL_Wrap(GetFloatv,        void,     (GLenum, GLfloatP))
BGL_Wrap(GetIntegerv,      void,     (GLenum, GLintP))
BGL_Wrap(GetLightfv,       void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetLightiv,       void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetMapdv,         void,     (GLenum, GLenum, GLdoubleP))
BGL_Wrap(GetMapfv,         void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetMapiv,         void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetMaterialfv,    void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetMaterialiv,    void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetPixelMapfv,    void,     (GLenum, GLfloatP))
BGL_Wrap(GetPixelMapuiv,   void,     (GLenum, GLuintP))
BGL_Wrap(GetPixelMapusv,   void,     (GLenum, GLushortP))
BGL_Wrap(GetPolygonStipple, void,     (GLubyteP))
BGL_Wrap(GetProgramInfoLog, void,    (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetProgramiv,     void,     (GLuint, GLenum, GLintP))
BGL_Wrap(GetShaderInfoLog, void,     (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetShaderiv,      void,     (GLuint, GLenum, GLintP))
BGL_Wrap(GetShaderSource,  void,     (GLuint, GLsizei, GLsizeiP, GLcharP))
BGL_Wrap(GetString,        GLstring, (GLenum))
BGL_Wrap(GetTexEnvfv,      void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexEnviv,      void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetTexGendv,      void,     (GLenum, GLenum, GLdoubleP))
BGL_Wrap(GetTexGenfv,      void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexGeniv,      void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetTexImage,      void,     (GLenum, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(GetTexLevelParameterfv, void,     (GLenum, GLint, GLenum, GLfloatP))
BGL_Wrap(GetTexLevelParameteriv, void,     (GLenum, GLint, GLenum, GLintP))
BGL_Wrap(GetTexParameterfv,    void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(GetTexParameteriv,    void,     (GLenum, GLenum, GLintP))
BGL_Wrap(GetUniformLocation, GLint, (GLuint, GLstring))
BGL_Wrap(Hint,           void,     (GLenum, GLenum))
BGL_Wrap(IndexMask,      void,     (GLuint))
BGL_Wrap(Indexd,         void,     (GLdouble))
BGL_Wrap(Indexdv,        void,     (GLdoubleP))
BGL_Wrap(Indexf,         void,     (GLfloat))
BGL_Wrap(Indexfv,        void,     (GLfloatP))
BGL_Wrap(Indexi,         void,     (GLint))
BGL_Wrap(Indexiv,        void,     (GLintP))
BGL_Wrap(Indexs,         void,     (GLshort))
BGL_Wrap(Indexsv,        void,     (GLshortP))
BGL_Wrap(InitNames,      void,     (void))
BGL_Wrap(IsEnabled,      GLboolean,  (GLenum))
BGL_Wrap(IsList,         GLboolean,  (GLuint))
BGL_Wrap(IsProgram,      GLboolean,  (GLuint))
BGL_Wrap(IsShader,       GLboolean,  (GLuint))
BGL_Wrap(IsTexture,      GLboolean,  (GLuint))
BGL_Wrap(LightModelf,    void,     (GLenum, GLfloat))
BGL_Wrap(LightModelfv,   void,     (GLenum, GLfloatP))
BGL_Wrap(LightModeli,    void,     (GLenum, GLint))
BGL_Wrap(LightModeliv,   void,     (GLenum, GLintP))
BGL_Wrap(Lightf,         void,     (GLenum, GLenum, GLfloat))
BGL_Wrap(Lightfv,        void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(Lighti,         void,     (GLenum, GLenum, GLint))
BGL_Wrap(Lightiv,        void,     (GLenum, GLenum, GLintP))
BGL_Wrap(LineStipple,    void,     (GLint, GLushort))
BGL_Wrap(LineWidth,      void,     (GLfloat))
BGL_Wrap(LinkProgram,    void,     (GLuint))
BGL_Wrap(ListBase,       void,     (GLuint))
BGL_Wrap(LoadIdentity,   void,     (void))
BGL_Wrap(LoadMatrixd,    void,     (GLdoubleP))
BGL_Wrap(LoadMatrixf,    void,     (GLfloatP))
BGL_Wrap(LoadName,       void,     (GLuint))
BGL_Wrap(LogicOp,        void,     (GLenum))
BGL_Wrap(Map1d,          void,     (GLenum, GLdouble, GLdouble,
                                    GLint, GLint, GLdoubleP))
BGL_Wrap(Map1f,          void,     (GLenum, GLfloat, GLfloat,
                                    GLint, GLint, GLfloatP))
BGL_Wrap(Map2d,          void,     (GLenum, GLdouble, GLdouble,
                                    GLint, GLint, GLdouble, GLdouble, GLint, GLint, GLdoubleP))
BGL_Wrap(Map2f,          void,     (GLenum, GLfloat, GLfloat,
                                    GLint, GLint, GLfloat, GLfloat, GLint, GLint, GLfloatP))
BGL_Wrap(MapGrid1d,        void,     (GLint, GLdouble, GLdouble))
BGL_Wrap(MapGrid1f,        void,     (GLint, GLfloat, GLfloat))
BGL_Wrap(MapGrid2d,        void,     (GLint, GLdouble, GLdouble,
                                      GLint, GLdouble, GLdouble))
BGL_Wrap(MapGrid2f,        void,     (GLint, GLfloat, GLfloat,
                                      GLint, GLfloat, GLfloat))
BGL_Wrap(Materialf,        void,     (GLenum, GLenum, GLfloat))
BGL_Wrap(Materialfv,       void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(Materiali,        void,     (GLenum, GLenum, GLint))
BGL_Wrap(Materialiv,       void,     (GLenum, GLenum, GLintP))
BGL_Wrap(MatrixMode,       void,     (GLenum))
BGL_Wrap(MultMatrixd,      void,     (GLdoubleP))
BGL_Wrap(MultMatrixf,      void,     (GLfloatP))
BGL_Wrap(NewList,          void,     (GLuint, GLenum))
BGL_Wrap(Normal3b,         void,     (GLbyte, GLbyte, GLbyte))
BGL_Wrap(Normal3bv,        void,     (GLbyteP))
BGL_Wrap(Normal3d,         void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Normal3dv,        void,     (GLdoubleP))
BGL_Wrap(Normal3f,         void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Normal3fv,        void,     (GLfloatP))
BGL_Wrap(Normal3i,         void,     (GLint, GLint, GLint))
BGL_Wrap(Normal3iv,        void,     (GLintP))
BGL_Wrap(Normal3s,         void,     (GLshort, GLshort, GLshort))
BGL_Wrap(Normal3sv,        void,     (GLshortP))
BGL_Wrap(Ortho,            void,     (GLdouble, GLdouble,
                                      GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(PassThrough,      void,     (GLfloat))
BGL_Wrap(PixelMapfv,       void,     (GLenum, GLint, GLfloatP))
BGL_Wrap(PixelMapuiv,      void,     (GLenum, GLint, GLuintP))
BGL_Wrap(PixelMapusv,      void,     (GLenum, GLint, GLushortP))
BGL_Wrap(PixelStoref,      void,     (GLenum, GLfloat))
BGL_Wrap(PixelStorei,      void,     (GLenum, GLint))
BGL_Wrap(PixelTransferf,   void,     (GLenum, GLfloat))
BGL_Wrap(PixelTransferi,   void,     (GLenum, GLint))
BGL_Wrap(PixelZoom,        void,     (GLfloat, GLfloat))
BGL_Wrap(PointSize,        void,     (GLfloat))
BGL_Wrap(PolygonMode,      void,     (GLenum, GLenum))
BGL_Wrap(PolygonOffset,    void,     (GLfloat, GLfloat))
BGL_Wrap(PolygonStipple,   void,     (GLubyteP))
BGL_Wrap(PopAttrib,        void,     (void))
BGL_Wrap(PopClientAttrib,  void,     (void))
BGL_Wrap(PopMatrix,        void,     (void))
BGL_Wrap(PopName,          void,     (void))
BGL_Wrap(PrioritizeTextures,   void,   (GLsizei, GLuintP, GLclampfP))
BGL_Wrap(PushAttrib,       void,     (GLbitfield))
BGL_Wrap(PushClientAttrib, void,     (GLbitfield))
BGL_Wrap(PushMatrix,       void,     (void))
BGL_Wrap(PushName,         void,     (GLuint))
BGL_Wrap(RasterPos2d,      void,     (GLdouble, GLdouble))
BGL_Wrap(RasterPos2dv,     void,     (GLdoubleP))
BGL_Wrap(RasterPos2f,      void,     (GLfloat, GLfloat))
BGL_Wrap(RasterPos2fv,     void,     (GLfloatP))
BGL_Wrap(RasterPos2i,      void,     (GLint, GLint))
BGL_Wrap(RasterPos2iv,     void,     (GLintP))
BGL_Wrap(RasterPos2s,      void,     (GLshort, GLshort))
BGL_Wrap(RasterPos2sv,     void,     (GLshortP))
BGL_Wrap(RasterPos3d,      void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(RasterPos3dv,     void,     (GLdoubleP))
BGL_Wrap(RasterPos3f,      void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(RasterPos3fv,     void,     (GLfloatP))
BGL_Wrap(RasterPos3i,      void,     (GLint, GLint, GLint))
BGL_Wrap(RasterPos3iv,     void,     (GLintP))
BGL_Wrap(RasterPos3s,      void,     (GLshort, GLshort, GLshort))
BGL_Wrap(RasterPos3sv,     void,     (GLshortP))
BGL_Wrap(RasterPos4d,      void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(RasterPos4dv,     void,     (GLdoubleP))
BGL_Wrap(RasterPos4f,      void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(RasterPos4fv,     void,     (GLfloatP))
BGL_Wrap(RasterPos4i,      void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(RasterPos4iv,     void,     (GLintP))
BGL_Wrap(RasterPos4s,      void,     (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(RasterPos4sv,     void,     (GLshortP))
BGL_Wrap(ReadBuffer,       void,     (GLenum))
BGL_Wrap(ReadPixels,       void,     (GLint, GLint, GLsizei,
                                      GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(Rectd,          void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Rectdv,         void,     (GLdoubleP, GLdoubleP))
BGL_Wrap(Rectf,          void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Rectfv,         void,     (GLfloatP, GLfloatP))
BGL_Wrap(Recti,          void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(Rectiv,         void,     (GLintP, GLintP))
BGL_Wrap(Rects,          void,     (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Rectsv,         void,     (GLshortP, GLshortP))
BGL_Wrap(RenderMode,     GLint,    (GLenum))
BGL_Wrap(Rotated,        void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Rotatef,        void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Scaled,         void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Scalef,         void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Scissor,        void,     (GLint, GLint, GLsizei, GLsizei))
BGL_Wrap(SelectBuffer,   void,     (GLsizei, GLuintP))
BGL_Wrap(ShadeModel,       void,     (GLenum))
BGL_Wrap(StencilFunc,      void,     (GLenum, GLint, GLuint))
BGL_Wrap(StencilMask,      void,     (GLuint))
BGL_Wrap(StencilOp,        void,     (GLenum, GLenum, GLenum))
BGL_Wrap(TexCoord1d,       void,     (GLdouble))
BGL_Wrap(TexCoord1dv,      void,     (GLdoubleP))
BGL_Wrap(TexCoord1f,       void,     (GLfloat))
BGL_Wrap(TexCoord1fv,      void,     (GLfloatP))
BGL_Wrap(TexCoord1i,       void,     (GLint))
BGL_Wrap(TexCoord1iv,      void,     (GLintP))
BGL_Wrap(TexCoord1s,       void,     (GLshort))
BGL_Wrap(TexCoord1sv,      void,     (GLshortP))
BGL_Wrap(TexCoord2d,       void,     (GLdouble, GLdouble))
BGL_Wrap(TexCoord2dv,      void,     (GLdoubleP))
BGL_Wrap(TexCoord2f,       void,     (GLfloat, GLfloat))
BGL_Wrap(TexCoord2fv,      void,     (GLfloatP))
BGL_Wrap(TexCoord2i,       void,     (GLint, GLint))
BGL_Wrap(TexCoord2iv,      void,     (GLintP))
BGL_Wrap(TexCoord2s,       void,     (GLshort, GLshort))
BGL_Wrap(TexCoord2sv,      void,     (GLshortP))
BGL_Wrap(TexCoord3d,       void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(TexCoord3dv,      void,     (GLdoubleP))
BGL_Wrap(TexCoord3f,       void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(TexCoord3fv,      void,     (GLfloatP))
BGL_Wrap(TexCoord3i,       void,     (GLint, GLint, GLint))
BGL_Wrap(TexCoord3iv,      void,     (GLintP))
BGL_Wrap(TexCoord3s,       void,     (GLshort, GLshort, GLshort))
BGL_Wrap(TexCoord3sv,      void,     (GLshortP))
BGL_Wrap(TexCoord4d,       void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(TexCoord4dv,      void,     (GLdoubleP))
BGL_Wrap(TexCoord4f,       void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(TexCoord4fv,      void,     (GLfloatP))
BGL_Wrap(TexCoord4i,       void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(TexCoord4iv,      void,     (GLintP))
BGL_Wrap(TexCoord4s,       void,     (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(TexCoord4sv,      void,     (GLshortP))
BGL_Wrap(TexEnvf,        void,     (GLenum, GLenum, GLfloat))
BGL_Wrap(TexEnvfv,       void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexEnvi,        void,     (GLenum, GLenum, GLint))
BGL_Wrap(TexEnviv,       void,     (GLenum, GLenum, GLintP))
BGL_Wrap(TexGend,        void,     (GLenum, GLenum, GLdouble))
BGL_Wrap(TexGendv,       void,     (GLenum, GLenum, GLdoubleP))
BGL_Wrap(TexGenf,        void,     (GLenum, GLenum, GLfloat))
BGL_Wrap(TexGenfv,       void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexGeni,        void,     (GLenum, GLenum, GLint))
BGL_Wrap(TexGeniv,       void,     (GLenum, GLenum, GLintP))
BGL_Wrap(TexImage1D,     void,     (GLenum, GLint, GLint,
                                    GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexImage2D,     void,     (GLenum, GLint, GLint,
                                    GLsizei, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(TexParameterf,      void,     (GLenum, GLenum, GLfloat))
BGL_Wrap(TexParameterfv,     void,     (GLenum, GLenum, GLfloatP))
BGL_Wrap(TexParameteri,      void,     (GLenum, GLenum, GLint))
BGL_Wrap(TexParameteriv,     void,     (GLenum, GLenum, GLintP))
BGL_Wrap(Translated,         void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Translatef,         void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Uniform1f,          void,     (GLint, GLfloat))
BGL_Wrap(Uniform2f,          void,     (GLint, GLfloat, GLfloat))
BGL_Wrap(Uniform3f,          void,     (GLint, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Uniform4f,          void,     (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Uniform1fv,         void,     (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform2fv,         void,     (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform3fv,         void,     (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform4fv,         void,     (GLint, GLsizei, GLfloatP))
BGL_Wrap(Uniform1i,          void,     (GLint, GLint))
BGL_Wrap(Uniform2i,          void,     (GLint, GLint, GLint))
BGL_Wrap(Uniform3i,          void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(Uniform4i,          void,     (GLint, GLint, GLint, GLint, GLint))
BGL_Wrap(Uniform1iv,         void,     (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform2iv,         void,     (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform3iv,         void,     (GLint, GLsizei, GLintP))
BGL_Wrap(Uniform4iv,         void,     (GLint, GLsizei, GLintP))
BGL_Wrap(UniformMatrix2fv,   void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3fv,   void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4fv,   void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix2x3fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3x2fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix2x4fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4x2fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix3x4fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UniformMatrix4x3fv, void,     (GLint, GLsizei, GLboolean, GLfloatP))
BGL_Wrap(UseProgram,         void,     (GLuint))
BGL_Wrap(ValidateProgram,    void,     (GLuint))
BGL_Wrap(Vertex2d,           void,     (GLdouble, GLdouble))
BGL_Wrap(Vertex2dv,          void,     (GLdoubleP))
BGL_Wrap(Vertex2f,           void,     (GLfloat, GLfloat))
BGL_Wrap(Vertex2fv,          void,     (GLfloatP))
BGL_Wrap(Vertex2i,           void,     (GLint, GLint))
BGL_Wrap(Vertex2iv,          void,     (GLintP))
BGL_Wrap(Vertex2s,           void,     (GLshort, GLshort))
BGL_Wrap(Vertex2sv,          void,     (GLshortP))
BGL_Wrap(Vertex3d,           void,     (GLdouble, GLdouble, GLdouble))
BGL_Wrap(Vertex3dv,          void,     (GLdoubleP))
BGL_Wrap(Vertex3f,           void,     (GLfloat, GLfloat, GLfloat))
BGL_Wrap(Vertex3fv,          void,     (GLfloatP))
BGL_Wrap(Vertex3i,           void,     (GLint, GLint, GLint))
BGL_Wrap(Vertex3iv,          void,     (GLintP))
BGL_Wrap(Vertex3s,           void,     (GLshort, GLshort, GLshort))
BGL_Wrap(Vertex3sv,          void,     (GLshortP))
BGL_Wrap(Vertex4d,           void,     (GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(Vertex4dv,          void,     (GLdoubleP))
BGL_Wrap(Vertex4f,           void,     (GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(Vertex4fv,          void,     (GLfloatP))
BGL_Wrap(Vertex4i,           void,     (GLint, GLint, GLint, GLint))
BGL_Wrap(Vertex4iv,          void,     (GLintP))
BGL_Wrap(Vertex4s,           void,     (GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(Vertex4sv,          void,     (GLshortP))
BGL_Wrap(Viewport,           void,     (GLint, GLint, GLsizei, GLsizei))
BGLU_Wrap(Perspective,       void,       (GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(LookAt,            void,       (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(Ortho2D,           void,       (GLdouble, GLdouble, GLdouble, GLdouble))
BGLU_Wrap(PickMatrix,        void,       (GLdouble, GLdouble, GLdouble, GLdouble, GLintP))
BGLU_Wrap(Project,           GLint,      (GLdouble, GLdouble, GLdouble, GLdoubleP, GLdoubleP, GLintP, GLdoubleP, GLdoubleP, GLdoubleP))
BGLU_Wrap(UnProject,         GLint,      (GLdouble, GLdouble, GLdouble, GLdoubleP, GLdoubleP, GLintP, GLdoubleP, GLdoubleP, GLdoubleP))

#undef MethodDef

/** \} */


/* -------------------------------------------------------------------- */

/** \name Module Definition
 * \{ */

#define MethodDef(func) {"gl"#func, Method_##func, METH_VARARGS, NULL}
#define MethodDefu(func) {"glu"#func, Method_##func, METH_VARARGS, NULL}
/* So that MethodDef(Accum) becomes:
 * {"glAccum", Method_Accumfunc, METH_VARARGS} */

static struct PyMethodDef BGL_methods[] = {
	MethodDef(Accum),
	MethodDef(ActiveTexture),
	MethodDef(AlphaFunc),
	MethodDef(AreTexturesResident), 
	MethodDef(AttachShader),
	MethodDef(Begin),
	MethodDef(BindTexture), 
	MethodDef(Bitmap),
	MethodDef(BlendFunc),
	MethodDef(CallList),
	MethodDef(CallLists),
	MethodDef(Clear),
	MethodDef(ClearAccum),
	MethodDef(ClearColor),
	MethodDef(ClearDepth),
	MethodDef(ClearIndex),
	MethodDef(ClearStencil),
	MethodDef(ClipPlane),
	MethodDef(Color3b),
	MethodDef(Color3bv),
	MethodDef(Color3d),
	MethodDef(Color3dv),
	MethodDef(Color3f),
	MethodDef(Color3fv),
	MethodDef(Color3i),
	MethodDef(Color3iv),
	MethodDef(Color3s),
	MethodDef(Color3sv),
	MethodDef(Color3ub),
	MethodDef(Color3ubv),
	MethodDef(Color3ui),
	MethodDef(Color3uiv),
	MethodDef(Color3us),
	MethodDef(Color3usv),
	MethodDef(Color4b),
	MethodDef(Color4bv),
	MethodDef(Color4d),
	MethodDef(Color4dv),
	MethodDef(Color4f),
	MethodDef(Color4fv),
	MethodDef(Color4i),
	MethodDef(Color4iv),
	MethodDef(Color4s),
	MethodDef(Color4sv),
	MethodDef(Color4ub),
	MethodDef(Color4ubv),
	MethodDef(Color4ui),
	MethodDef(Color4uiv),
	MethodDef(Color4us),
	MethodDef(Color4usv),
	MethodDef(ColorMask),
	MethodDef(ColorMaterial),
	MethodDef(CompileShader),
	MethodDef(CopyPixels),
	MethodDef(CopyTexImage2D),
	MethodDef(CreateProgram),
	MethodDef(CreateShader),
	MethodDef(CullFace),
	MethodDef(DeleteLists),
	MethodDef(DeleteProgram),
	MethodDef(DeleteShader),
	MethodDef(DeleteTextures),
	MethodDef(DepthFunc),
	MethodDef(DepthMask),
	MethodDef(DepthRange),
	MethodDef(DetachShader),
	MethodDef(Disable),
	MethodDef(DrawBuffer),
	MethodDef(DrawPixels),
	MethodDef(EdgeFlag),
	MethodDef(EdgeFlagv),
	MethodDef(Enable),
	MethodDef(End),
	MethodDef(EndList),
	MethodDef(EvalCoord1d),
	MethodDef(EvalCoord1dv),
	MethodDef(EvalCoord1f),
	MethodDef(EvalCoord1fv),
	MethodDef(EvalCoord2d),
	MethodDef(EvalCoord2dv),
	MethodDef(EvalCoord2f),
	MethodDef(EvalCoord2fv),
	MethodDef(EvalMesh1),
	MethodDef(EvalMesh2),
	MethodDef(EvalPoint1),
	MethodDef(EvalPoint2),
	MethodDef(FeedbackBuffer),
	MethodDef(Finish),
	MethodDef(Flush),
	MethodDef(Fogf),
	MethodDef(Fogfv),
	MethodDef(Fogi),
	MethodDef(Fogiv),
	MethodDef(FrontFace),
	MethodDef(Frustum),
	MethodDef(GenLists),
	MethodDef(GenTextures), 
	MethodDef(GetAttachedShaders),
	MethodDef(GetBooleanv),
	MethodDef(GetClipPlane),
	MethodDef(GetDoublev),
	MethodDef(GetError),
	MethodDef(GetFloatv),
	MethodDef(GetIntegerv),
	MethodDef(GetLightfv),
	MethodDef(GetLightiv),
	MethodDef(GetMapdv),
	MethodDef(GetMapfv),
	MethodDef(GetMapiv),
	MethodDef(GetMaterialfv),
	MethodDef(GetMaterialiv),
	MethodDef(GetPixelMapfv),
	MethodDef(GetPixelMapuiv),
	MethodDef(GetPixelMapusv),
	MethodDef(GetPolygonStipple),
	MethodDef(GetProgramInfoLog),
	MethodDef(GetProgramiv),
	MethodDef(GetShaderInfoLog),
	MethodDef(GetShaderiv),
	MethodDef(GetShaderSource),
	MethodDef(GetString),
	MethodDef(GetTexEnvfv),
	MethodDef(GetTexEnviv),
	MethodDef(GetTexGendv),
	MethodDef(GetTexGenfv),
	MethodDef(GetTexGeniv),
	MethodDef(GetTexImage),
	MethodDef(GetTexLevelParameterfv),
	MethodDef(GetTexLevelParameteriv),
	MethodDef(GetTexParameterfv),
	MethodDef(GetTexParameteriv),
	MethodDef(GetUniformLocation),
	MethodDef(Hint),
	MethodDef(IndexMask),
	MethodDef(Indexd),
	MethodDef(Indexdv),
	MethodDef(Indexf),
	MethodDef(Indexfv),
	MethodDef(Indexi),
	MethodDef(Indexiv),
	MethodDef(Indexs),
	MethodDef(Indexsv),
	MethodDef(InitNames),
	MethodDef(IsEnabled),
	MethodDef(IsList),
	MethodDef(IsProgram),
	MethodDef(IsShader),
	MethodDef(IsTexture), 
	MethodDef(LightModelf),
	MethodDef(LightModelfv),
	MethodDef(LightModeli),
	MethodDef(LightModeliv),
	MethodDef(Lightf),
	MethodDef(Lightfv),
	MethodDef(Lighti),
	MethodDef(Lightiv),
	MethodDef(LineStipple),
	MethodDef(LineWidth),
	MethodDef(LinkProgram),
	MethodDef(ListBase),
	MethodDef(LoadIdentity),
	MethodDef(LoadMatrixd),
	MethodDef(LoadMatrixf),
	MethodDef(LoadName),
	MethodDef(LogicOp),
	MethodDef(Map1d),
	MethodDef(Map1f),
	MethodDef(Map2d),
	MethodDef(Map2f),
	MethodDef(MapGrid1d),
	MethodDef(MapGrid1f),
	MethodDef(MapGrid2d),
	MethodDef(MapGrid2f),
	MethodDef(Materialf),
	MethodDef(Materialfv),
	MethodDef(Materiali),
	MethodDef(Materialiv),
	MethodDef(MatrixMode),
	MethodDef(MultMatrixd),
	MethodDef(MultMatrixf),
	MethodDef(NewList),
	MethodDef(Normal3b),
	MethodDef(Normal3bv),
	MethodDef(Normal3d),
	MethodDef(Normal3dv),
	MethodDef(Normal3f),
	MethodDef(Normal3fv),
	MethodDef(Normal3i),
	MethodDef(Normal3iv),
	MethodDef(Normal3s),
	MethodDef(Normal3sv),
	MethodDef(Ortho),
	MethodDef(PassThrough),
	MethodDef(PixelMapfv),
	MethodDef(PixelMapuiv),
	MethodDef(PixelMapusv),
	MethodDef(PixelStoref),
	MethodDef(PixelStorei),
	MethodDef(PixelTransferf),
	MethodDef(PixelTransferi),
	MethodDef(PixelZoom),
	MethodDef(PointSize),
	MethodDef(PolygonMode),
	MethodDef(PolygonOffset),
	MethodDef(PolygonStipple),
	MethodDef(PopAttrib),
	MethodDef(PopClientAttrib),
	MethodDef(PopMatrix),
	MethodDef(PopName),
	MethodDef(PrioritizeTextures), 
	MethodDef(PushAttrib),
	MethodDef(PushClientAttrib),
	MethodDef(PushMatrix),
	MethodDef(PushName),
	MethodDef(RasterPos2d),
	MethodDef(RasterPos2dv),
	MethodDef(RasterPos2f),
	MethodDef(RasterPos2fv),
	MethodDef(RasterPos2i),
	MethodDef(RasterPos2iv),
	MethodDef(RasterPos2s),
	MethodDef(RasterPos2sv),
	MethodDef(RasterPos3d),
	MethodDef(RasterPos3dv),
	MethodDef(RasterPos3f),
	MethodDef(RasterPos3fv),
	MethodDef(RasterPos3i),
	MethodDef(RasterPos3iv),
	MethodDef(RasterPos3s),
	MethodDef(RasterPos3sv),
	MethodDef(RasterPos4d),
	MethodDef(RasterPos4dv),
	MethodDef(RasterPos4f),
	MethodDef(RasterPos4fv),
	MethodDef(RasterPos4i),
	MethodDef(RasterPos4iv),
	MethodDef(RasterPos4s),
	MethodDef(RasterPos4sv),
	MethodDef(ReadBuffer),
	MethodDef(ReadPixels),
	MethodDef(Rectd),
	MethodDef(Rectdv),
	MethodDef(Rectf),
	MethodDef(Rectfv),
	MethodDef(Recti),
	MethodDef(Rectiv),
	MethodDef(Rects),
	MethodDef(Rectsv),
	MethodDef(RenderMode),
	MethodDef(Rotated),
	MethodDef(Rotatef),
	MethodDef(Scaled),
	MethodDef(Scalef),
	MethodDef(Scissor),
	MethodDef(SelectBuffer),
	MethodDef(ShadeModel),
	MethodDef(ShaderSource),
	MethodDef(StencilFunc),
	MethodDef(StencilMask),
	MethodDef(StencilOp),
	MethodDef(TexCoord1d),
	MethodDef(TexCoord1dv),
	MethodDef(TexCoord1f),
	MethodDef(TexCoord1fv),
	MethodDef(TexCoord1i),
	MethodDef(TexCoord1iv),
	MethodDef(TexCoord1s),
	MethodDef(TexCoord1sv),
	MethodDef(TexCoord2d),
	MethodDef(TexCoord2dv),
	MethodDef(TexCoord2f),
	MethodDef(TexCoord2fv),
	MethodDef(TexCoord2i),
	MethodDef(TexCoord2iv),
	MethodDef(TexCoord2s),
	MethodDef(TexCoord2sv),
	MethodDef(TexCoord3d),
	MethodDef(TexCoord3dv),
	MethodDef(TexCoord3f),
	MethodDef(TexCoord3fv),
	MethodDef(TexCoord3i),
	MethodDef(TexCoord3iv),
	MethodDef(TexCoord3s),
	MethodDef(TexCoord3sv),
	MethodDef(TexCoord4d),
	MethodDef(TexCoord4dv),
	MethodDef(TexCoord4f),
	MethodDef(TexCoord4fv),
	MethodDef(TexCoord4i),
	MethodDef(TexCoord4iv),
	MethodDef(TexCoord4s),
	MethodDef(TexCoord4sv),
	MethodDef(TexEnvf),
	MethodDef(TexEnvfv),
	MethodDef(TexEnvi),
	MethodDef(TexEnviv),
	MethodDef(TexGend),
	MethodDef(TexGendv),
	MethodDef(TexGenf),
	MethodDef(TexGenfv),
	MethodDef(TexGeni),
	MethodDef(TexGeniv),
	MethodDef(TexImage1D),
	MethodDef(TexImage2D),
	MethodDef(TexParameterf),
	MethodDef(TexParameterfv),
	MethodDef(TexParameteri),
	MethodDef(TexParameteriv),
	MethodDef(Translated),
	MethodDef(Translatef),
	MethodDef(Uniform1f),
	MethodDef(Uniform2f),
	MethodDef(Uniform3f),
	MethodDef(Uniform4f),
	MethodDef(Uniform1fv),
	MethodDef(Uniform2fv),
	MethodDef(Uniform3fv),
	MethodDef(Uniform4fv),
	MethodDef(Uniform1i),
	MethodDef(Uniform2i),
	MethodDef(Uniform3i),
	MethodDef(Uniform4i),
	MethodDef(Uniform1iv),
	MethodDef(Uniform2iv),
	MethodDef(Uniform3iv),
	MethodDef(Uniform4iv),
	MethodDef(UniformMatrix2fv),
	MethodDef(UniformMatrix3fv),
	MethodDef(UniformMatrix4fv),
	MethodDef(UniformMatrix2x3fv),
	MethodDef(UniformMatrix3x2fv),
	MethodDef(UniformMatrix2x4fv),
	MethodDef(UniformMatrix4x2fv),
	MethodDef(UniformMatrix3x4fv),
	MethodDef(UniformMatrix4x3fv),
	MethodDef(UseProgram),
	MethodDef(ValidateProgram),
	MethodDef(Vertex2d),
	MethodDef(Vertex2dv),
	MethodDef(Vertex2f),
	MethodDef(Vertex2fv),
	MethodDef(Vertex2i),
	MethodDef(Vertex2iv),
	MethodDef(Vertex2s),
	MethodDef(Vertex2sv),
	MethodDef(Vertex3d),
	MethodDef(Vertex3dv),
	MethodDef(Vertex3f),
	MethodDef(Vertex3fv),
	MethodDef(Vertex3i),
	MethodDef(Vertex3iv),
	MethodDef(Vertex3s),
	MethodDef(Vertex3sv),
	MethodDef(Vertex4d),
	MethodDef(Vertex4dv),
	MethodDef(Vertex4f),
	MethodDef(Vertex4fv),
	MethodDef(Vertex4i),
	MethodDef(Vertex4iv),
	MethodDef(Vertex4s),
	MethodDef(Vertex4sv),
	MethodDef(Viewport),
	MethodDefu(Perspective),
	MethodDefu(LookAt),
	MethodDefu(Ortho2D),
	MethodDefu(PickMatrix),
	MethodDefu(Project),
	MethodDefu(UnProject),
	{NULL, NULL, 0, NULL}
};

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

static void expp_addconst_int(PyObject *dict, const char *name, int value)
{
	PyObject *item;
	PyDict_SetItemString(dict, name, item = PyLong_FromLong(value));
	Py_DECREF(item);
}

PyObject *BPyInit_bgl(void)
{
	PyObject *submodule, *dict;
	submodule = PyModule_Create(&BGL_module_def);
	dict = PyModule_GetDict(submodule);

	if (PyType_Ready(&BGL_bufferType) < 0)
		return NULL;  /* should never happen */

	PyModule_AddObject(submodule, "Buffer", (PyObject *)&BGL_bufferType);
	Py_INCREF((PyObject *)&BGL_bufferType);

#define EXPP_ADDCONST(x) expp_addconst_int(dict, #x, x)

	EXPP_ADDCONST(GL_CURRENT_BIT);
	EXPP_ADDCONST(GL_POINT_BIT);
	EXPP_ADDCONST(GL_LINE_BIT);
	EXPP_ADDCONST(GL_POLYGON_BIT);
	EXPP_ADDCONST(GL_POLYGON_STIPPLE_BIT);
	EXPP_ADDCONST(GL_PIXEL_MODE_BIT);
	EXPP_ADDCONST(GL_LIGHTING_BIT);
	EXPP_ADDCONST(GL_FOG_BIT);
	EXPP_ADDCONST(GL_DEPTH_BUFFER_BIT);
	EXPP_ADDCONST(GL_ACCUM_BUFFER_BIT);
	EXPP_ADDCONST(GL_STENCIL_BUFFER_BIT);
	EXPP_ADDCONST(GL_VIEWPORT_BIT);
	EXPP_ADDCONST(GL_TRANSFORM_BIT);
	EXPP_ADDCONST(GL_ENABLE_BIT);
	EXPP_ADDCONST(GL_COLOR_BUFFER_BIT);
	EXPP_ADDCONST(GL_HINT_BIT);
	EXPP_ADDCONST(GL_EVAL_BIT);
	EXPP_ADDCONST(GL_LIST_BIT);
	EXPP_ADDCONST(GL_TEXTURE_BIT);
	EXPP_ADDCONST(GL_SCISSOR_BIT);
	EXPP_ADDCONST(GL_ALL_ATTRIB_BITS);
	EXPP_ADDCONST(GL_CLIENT_ALL_ATTRIB_BITS);
	
	EXPP_ADDCONST(GL_FALSE);
	EXPP_ADDCONST(GL_TRUE);

	EXPP_ADDCONST(GL_POINTS);
	EXPP_ADDCONST(GL_LINES);
	EXPP_ADDCONST(GL_LINE_LOOP);
	EXPP_ADDCONST(GL_LINE_STRIP);
	EXPP_ADDCONST(GL_TRIANGLES);
	EXPP_ADDCONST(GL_TRIANGLE_STRIP);
	EXPP_ADDCONST(GL_TRIANGLE_FAN);
	EXPP_ADDCONST(GL_QUADS);
	EXPP_ADDCONST(GL_QUAD_STRIP);
	EXPP_ADDCONST(GL_POLYGON);

	EXPP_ADDCONST(GL_ACCUM);
	EXPP_ADDCONST(GL_LOAD);
	EXPP_ADDCONST(GL_RETURN);
	EXPP_ADDCONST(GL_MULT);
	EXPP_ADDCONST(GL_ADD);

	EXPP_ADDCONST(GL_NEVER);
	EXPP_ADDCONST(GL_LESS);
	EXPP_ADDCONST(GL_EQUAL);
	EXPP_ADDCONST(GL_LEQUAL);
	EXPP_ADDCONST(GL_GREATER);
	EXPP_ADDCONST(GL_NOTEQUAL);
	EXPP_ADDCONST(GL_GEQUAL);
	EXPP_ADDCONST(GL_ALWAYS);

	EXPP_ADDCONST(GL_ZERO);
	EXPP_ADDCONST(GL_ONE);
	EXPP_ADDCONST(GL_SRC_COLOR);
	EXPP_ADDCONST(GL_ONE_MINUS_SRC_COLOR);
	EXPP_ADDCONST(GL_SRC_ALPHA);
	EXPP_ADDCONST(GL_ONE_MINUS_SRC_ALPHA);
	EXPP_ADDCONST(GL_DST_ALPHA);
	EXPP_ADDCONST(GL_ONE_MINUS_DST_ALPHA);

	EXPP_ADDCONST(GL_DST_COLOR);
	EXPP_ADDCONST(GL_ONE_MINUS_DST_COLOR);
	EXPP_ADDCONST(GL_SRC_ALPHA_SATURATE);

	EXPP_ADDCONST(GL_NONE);
	EXPP_ADDCONST(GL_FRONT_LEFT);
	EXPP_ADDCONST(GL_FRONT_RIGHT);
	EXPP_ADDCONST(GL_BACK_LEFT);
	EXPP_ADDCONST(GL_BACK_RIGHT);
	EXPP_ADDCONST(GL_FRONT);
	EXPP_ADDCONST(GL_BACK);
	EXPP_ADDCONST(GL_LEFT);
	EXPP_ADDCONST(GL_RIGHT);
	EXPP_ADDCONST(GL_FRONT_AND_BACK);
	EXPP_ADDCONST(GL_AUX0);
	EXPP_ADDCONST(GL_AUX1);
	EXPP_ADDCONST(GL_AUX2);
	EXPP_ADDCONST(GL_AUX3);

	EXPP_ADDCONST(GL_NO_ERROR);
	EXPP_ADDCONST(GL_INVALID_ENUM);
	EXPP_ADDCONST(GL_INVALID_VALUE);
	EXPP_ADDCONST(GL_INVALID_OPERATION);
	EXPP_ADDCONST(GL_STACK_OVERFLOW);
	EXPP_ADDCONST(GL_STACK_UNDERFLOW);
	EXPP_ADDCONST(GL_OUT_OF_MEMORY);

	EXPP_ADDCONST(GL_2D);
	EXPP_ADDCONST(GL_3D);
	EXPP_ADDCONST(GL_3D_COLOR);
	EXPP_ADDCONST(GL_3D_COLOR_TEXTURE);
	EXPP_ADDCONST(GL_4D_COLOR_TEXTURE);

	EXPP_ADDCONST(GL_PASS_THROUGH_TOKEN);
	EXPP_ADDCONST(GL_POINT_TOKEN);
	EXPP_ADDCONST(GL_LINE_TOKEN);
	EXPP_ADDCONST(GL_POLYGON_TOKEN);
	EXPP_ADDCONST(GL_BITMAP_TOKEN);
	EXPP_ADDCONST(GL_DRAW_PIXEL_TOKEN);
	EXPP_ADDCONST(GL_COPY_PIXEL_TOKEN);
	EXPP_ADDCONST(GL_LINE_RESET_TOKEN);

	EXPP_ADDCONST(GL_EXP);
	EXPP_ADDCONST(GL_EXP2);

	EXPP_ADDCONST(GL_CW);
	EXPP_ADDCONST(GL_CCW);

	EXPP_ADDCONST(GL_COEFF);
	EXPP_ADDCONST(GL_ORDER);
	EXPP_ADDCONST(GL_DOMAIN);

	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_I);
	EXPP_ADDCONST(GL_PIXEL_MAP_S_TO_S);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_R);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_G);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_B);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_A);
	EXPP_ADDCONST(GL_PIXEL_MAP_R_TO_R);
	EXPP_ADDCONST(GL_PIXEL_MAP_G_TO_G);
	EXPP_ADDCONST(GL_PIXEL_MAP_B_TO_B);
	EXPP_ADDCONST(GL_PIXEL_MAP_A_TO_A);

	EXPP_ADDCONST(GL_CURRENT_COLOR);
	EXPP_ADDCONST(GL_CURRENT_INDEX);
	EXPP_ADDCONST(GL_CURRENT_NORMAL);
	EXPP_ADDCONST(GL_CURRENT_TEXTURE_COORDS);
	EXPP_ADDCONST(GL_CURRENT_RASTER_COLOR);
	EXPP_ADDCONST(GL_CURRENT_RASTER_INDEX);
	EXPP_ADDCONST(GL_CURRENT_RASTER_TEXTURE_COORDS);
	EXPP_ADDCONST(GL_CURRENT_RASTER_POSITION);
	EXPP_ADDCONST(GL_CURRENT_RASTER_POSITION_VALID);
	EXPP_ADDCONST(GL_CURRENT_RASTER_DISTANCE);
	EXPP_ADDCONST(GL_POINT_SMOOTH);
	EXPP_ADDCONST(GL_POINT_SIZE);
	EXPP_ADDCONST(GL_POINT_SIZE_RANGE);
	EXPP_ADDCONST(GL_POINT_SIZE_GRANULARITY);
	EXPP_ADDCONST(GL_LINE_SMOOTH);
	EXPP_ADDCONST(GL_LINE_WIDTH);
	EXPP_ADDCONST(GL_LINE_WIDTH_RANGE);
	EXPP_ADDCONST(GL_LINE_WIDTH_GRANULARITY);
	EXPP_ADDCONST(GL_LINE_STIPPLE);
	EXPP_ADDCONST(GL_LINE_STIPPLE_PATTERN);
	EXPP_ADDCONST(GL_LINE_STIPPLE_REPEAT);
	EXPP_ADDCONST(GL_LIST_MODE);
	EXPP_ADDCONST(GL_MAX_LIST_NESTING);
	EXPP_ADDCONST(GL_LIST_BASE);
	EXPP_ADDCONST(GL_LIST_INDEX);
	EXPP_ADDCONST(GL_POLYGON_MODE);
	EXPP_ADDCONST(GL_POLYGON_SMOOTH);
	EXPP_ADDCONST(GL_POLYGON_STIPPLE);
	EXPP_ADDCONST(GL_EDGE_FLAG);
	EXPP_ADDCONST(GL_CULL_FACE);
	EXPP_ADDCONST(GL_CULL_FACE_MODE);
	EXPP_ADDCONST(GL_FRONT_FACE);
	EXPP_ADDCONST(GL_LIGHTING);
	EXPP_ADDCONST(GL_LIGHT_MODEL_LOCAL_VIEWER);
	EXPP_ADDCONST(GL_LIGHT_MODEL_TWO_SIDE);
	EXPP_ADDCONST(GL_LIGHT_MODEL_AMBIENT);
	EXPP_ADDCONST(GL_SHADE_MODEL);
	EXPP_ADDCONST(GL_COLOR_MATERIAL_FACE);
	EXPP_ADDCONST(GL_COLOR_MATERIAL_PARAMETER);
	EXPP_ADDCONST(GL_COLOR_MATERIAL);
	EXPP_ADDCONST(GL_FOG);
	EXPP_ADDCONST(GL_FOG_INDEX);
	EXPP_ADDCONST(GL_FOG_DENSITY);
	EXPP_ADDCONST(GL_FOG_START);
	EXPP_ADDCONST(GL_FOG_END);
	EXPP_ADDCONST(GL_FOG_MODE);
	EXPP_ADDCONST(GL_FOG_COLOR);
	EXPP_ADDCONST(GL_DEPTH_RANGE);
	EXPP_ADDCONST(GL_DEPTH_TEST);
	EXPP_ADDCONST(GL_DEPTH_WRITEMASK);
	EXPP_ADDCONST(GL_DEPTH_CLEAR_VALUE);
	EXPP_ADDCONST(GL_DEPTH_FUNC);
	EXPP_ADDCONST(GL_ACCUM_CLEAR_VALUE);
	EXPP_ADDCONST(GL_STENCIL_TEST);
	EXPP_ADDCONST(GL_STENCIL_CLEAR_VALUE);
	EXPP_ADDCONST(GL_STENCIL_FUNC);
	EXPP_ADDCONST(GL_STENCIL_VALUE_MASK);
	EXPP_ADDCONST(GL_STENCIL_FAIL);
	EXPP_ADDCONST(GL_STENCIL_PASS_DEPTH_FAIL);
	EXPP_ADDCONST(GL_STENCIL_PASS_DEPTH_PASS);
	EXPP_ADDCONST(GL_STENCIL_REF);
	EXPP_ADDCONST(GL_STENCIL_WRITEMASK);
	EXPP_ADDCONST(GL_MATRIX_MODE);
	EXPP_ADDCONST(GL_NORMALIZE);
	EXPP_ADDCONST(GL_VIEWPORT);
	EXPP_ADDCONST(GL_MODELVIEW_STACK_DEPTH);
	EXPP_ADDCONST(GL_PROJECTION_STACK_DEPTH);
	EXPP_ADDCONST(GL_TEXTURE_STACK_DEPTH);
	EXPP_ADDCONST(GL_MODELVIEW_MATRIX);
	EXPP_ADDCONST(GL_PROJECTION_MATRIX);
	EXPP_ADDCONST(GL_TEXTURE_MATRIX);
	EXPP_ADDCONST(GL_ATTRIB_STACK_DEPTH);
	EXPP_ADDCONST(GL_ALPHA_TEST);
	EXPP_ADDCONST(GL_ALPHA_TEST_FUNC);
	EXPP_ADDCONST(GL_ALPHA_TEST_REF);
	EXPP_ADDCONST(GL_DITHER);
	EXPP_ADDCONST(GL_BLEND_DST);
	EXPP_ADDCONST(GL_BLEND_SRC);
	EXPP_ADDCONST(GL_BLEND);
	EXPP_ADDCONST(GL_LOGIC_OP_MODE);
	EXPP_ADDCONST(GL_LOGIC_OP);
	EXPP_ADDCONST(GL_AUX_BUFFERS);
	EXPP_ADDCONST(GL_DRAW_BUFFER);
	EXPP_ADDCONST(GL_READ_BUFFER);
	EXPP_ADDCONST(GL_SCISSOR_BOX);
	EXPP_ADDCONST(GL_SCISSOR_TEST);
	EXPP_ADDCONST(GL_INDEX_CLEAR_VALUE);
	EXPP_ADDCONST(GL_INDEX_WRITEMASK);
	EXPP_ADDCONST(GL_COLOR_CLEAR_VALUE);
	EXPP_ADDCONST(GL_COLOR_WRITEMASK);
	EXPP_ADDCONST(GL_INDEX_MODE);
	EXPP_ADDCONST(GL_RGBA_MODE);
	EXPP_ADDCONST(GL_DOUBLEBUFFER);
	EXPP_ADDCONST(GL_STEREO);
	EXPP_ADDCONST(GL_RENDER_MODE);
	EXPP_ADDCONST(GL_PERSPECTIVE_CORRECTION_HINT);
	EXPP_ADDCONST(GL_POINT_SMOOTH_HINT);
	EXPP_ADDCONST(GL_LINE_SMOOTH_HINT);
	EXPP_ADDCONST(GL_POLYGON_SMOOTH_HINT);
	EXPP_ADDCONST(GL_FOG_HINT);
	EXPP_ADDCONST(GL_TEXTURE_GEN_S);
	EXPP_ADDCONST(GL_TEXTURE_GEN_T);
	EXPP_ADDCONST(GL_TEXTURE_GEN_R);
	EXPP_ADDCONST(GL_TEXTURE_GEN_Q);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_I_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_S_TO_S_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_R_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_G_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_B_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_I_TO_A_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_R_TO_R_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_G_TO_G_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_B_TO_B_SIZE);
	EXPP_ADDCONST(GL_PIXEL_MAP_A_TO_A_SIZE);
	EXPP_ADDCONST(GL_UNPACK_SWAP_BYTES);
	EXPP_ADDCONST(GL_UNPACK_LSB_FIRST);
	EXPP_ADDCONST(GL_UNPACK_ROW_LENGTH);
	EXPP_ADDCONST(GL_UNPACK_SKIP_ROWS);
	EXPP_ADDCONST(GL_UNPACK_SKIP_PIXELS);
	EXPP_ADDCONST(GL_UNPACK_ALIGNMENT);
	EXPP_ADDCONST(GL_PACK_SWAP_BYTES);
	EXPP_ADDCONST(GL_PACK_LSB_FIRST);
	EXPP_ADDCONST(GL_PACK_ROW_LENGTH);
	EXPP_ADDCONST(GL_PACK_SKIP_ROWS);
	EXPP_ADDCONST(GL_PACK_SKIP_PIXELS);
	EXPP_ADDCONST(GL_PACK_ALIGNMENT);
	EXPP_ADDCONST(GL_MAP_COLOR);
	EXPP_ADDCONST(GL_MAP_STENCIL);
	EXPP_ADDCONST(GL_INDEX_SHIFT);
	EXPP_ADDCONST(GL_INDEX_OFFSET);
	EXPP_ADDCONST(GL_RED_SCALE);
	EXPP_ADDCONST(GL_RED_BIAS);
	EXPP_ADDCONST(GL_ZOOM_X);
	EXPP_ADDCONST(GL_ZOOM_Y);
	EXPP_ADDCONST(GL_GREEN_SCALE);
	EXPP_ADDCONST(GL_GREEN_BIAS);
	EXPP_ADDCONST(GL_BLUE_SCALE);
	EXPP_ADDCONST(GL_BLUE_BIAS);
	EXPP_ADDCONST(GL_ALPHA_SCALE);
	EXPP_ADDCONST(GL_ALPHA_BIAS);
	EXPP_ADDCONST(GL_DEPTH_SCALE);
	EXPP_ADDCONST(GL_DEPTH_BIAS);
	EXPP_ADDCONST(GL_MAX_EVAL_ORDER);
	EXPP_ADDCONST(GL_MAX_LIGHTS);
	EXPP_ADDCONST(GL_MAX_CLIP_PLANES);
	EXPP_ADDCONST(GL_MAX_TEXTURE_SIZE);
	EXPP_ADDCONST(GL_MAX_PIXEL_MAP_TABLE);
	EXPP_ADDCONST(GL_MAX_ATTRIB_STACK_DEPTH);
	EXPP_ADDCONST(GL_MAX_MODELVIEW_STACK_DEPTH);
	EXPP_ADDCONST(GL_MAX_NAME_STACK_DEPTH);
	EXPP_ADDCONST(GL_MAX_PROJECTION_STACK_DEPTH);
	EXPP_ADDCONST(GL_MAX_TEXTURE_STACK_DEPTH);
	EXPP_ADDCONST(GL_MAX_VIEWPORT_DIMS);
	EXPP_ADDCONST(GL_SUBPIXEL_BITS);
	EXPP_ADDCONST(GL_INDEX_BITS);
	EXPP_ADDCONST(GL_RED_BITS);
	EXPP_ADDCONST(GL_GREEN_BITS);
	EXPP_ADDCONST(GL_BLUE_BITS);
	EXPP_ADDCONST(GL_ALPHA_BITS);
	EXPP_ADDCONST(GL_DEPTH_BITS);
	EXPP_ADDCONST(GL_STENCIL_BITS);
	EXPP_ADDCONST(GL_ACCUM_RED_BITS);
	EXPP_ADDCONST(GL_ACCUM_GREEN_BITS);
	EXPP_ADDCONST(GL_ACCUM_BLUE_BITS);
	EXPP_ADDCONST(GL_ACCUM_ALPHA_BITS);
	EXPP_ADDCONST(GL_NAME_STACK_DEPTH);
	EXPP_ADDCONST(GL_AUTO_NORMAL);
	EXPP_ADDCONST(GL_MAP1_COLOR_4);
	EXPP_ADDCONST(GL_MAP1_INDEX);
	EXPP_ADDCONST(GL_MAP1_NORMAL);
	EXPP_ADDCONST(GL_MAP1_TEXTURE_COORD_1);
	EXPP_ADDCONST(GL_MAP1_TEXTURE_COORD_2);
	EXPP_ADDCONST(GL_MAP1_TEXTURE_COORD_3);
	EXPP_ADDCONST(GL_MAP1_TEXTURE_COORD_4);
	EXPP_ADDCONST(GL_MAP1_VERTEX_3);
	EXPP_ADDCONST(GL_MAP1_VERTEX_4);
	EXPP_ADDCONST(GL_MAP2_COLOR_4);
	EXPP_ADDCONST(GL_MAP2_INDEX);
	EXPP_ADDCONST(GL_MAP2_NORMAL);
	EXPP_ADDCONST(GL_MAP2_TEXTURE_COORD_1);
	EXPP_ADDCONST(GL_MAP2_TEXTURE_COORD_2);
	EXPP_ADDCONST(GL_MAP2_TEXTURE_COORD_3);
	EXPP_ADDCONST(GL_MAP2_TEXTURE_COORD_4);
	EXPP_ADDCONST(GL_MAP2_VERTEX_3);
	EXPP_ADDCONST(GL_MAP2_VERTEX_4);
	EXPP_ADDCONST(GL_MAP1_GRID_DOMAIN);
	EXPP_ADDCONST(GL_MAP1_GRID_SEGMENTS);
	EXPP_ADDCONST(GL_MAP2_GRID_DOMAIN);
	EXPP_ADDCONST(GL_MAP2_GRID_SEGMENTS);
	EXPP_ADDCONST(GL_TEXTURE_1D);
	EXPP_ADDCONST(GL_TEXTURE_2D);

	EXPP_ADDCONST(GL_TEXTURE_WIDTH);
	EXPP_ADDCONST(GL_TEXTURE_HEIGHT);
	EXPP_ADDCONST(GL_TEXTURE_COMPONENTS);
	EXPP_ADDCONST(GL_TEXTURE_BORDER_COLOR);
	EXPP_ADDCONST(GL_TEXTURE_BORDER);

	EXPP_ADDCONST(GL_DONT_CARE);
	EXPP_ADDCONST(GL_FASTEST);
	EXPP_ADDCONST(GL_NICEST);

	EXPP_ADDCONST(GL_AMBIENT);
	EXPP_ADDCONST(GL_DIFFUSE);
	EXPP_ADDCONST(GL_SPECULAR);
	EXPP_ADDCONST(GL_POSITION);
	EXPP_ADDCONST(GL_SPOT_DIRECTION);
	EXPP_ADDCONST(GL_SPOT_EXPONENT);
	EXPP_ADDCONST(GL_SPOT_CUTOFF);
	EXPP_ADDCONST(GL_CONSTANT_ATTENUATION);
	EXPP_ADDCONST(GL_LINEAR_ATTENUATION);
	EXPP_ADDCONST(GL_QUADRATIC_ATTENUATION);

	EXPP_ADDCONST(GL_COMPILE);
	EXPP_ADDCONST(GL_COMPILE_AND_EXECUTE);

	EXPP_ADDCONST(GL_BYTE);
	EXPP_ADDCONST(GL_UNSIGNED_BYTE);
	EXPP_ADDCONST(GL_SHORT);
	EXPP_ADDCONST(GL_UNSIGNED_SHORT);
	EXPP_ADDCONST(GL_INT);
	EXPP_ADDCONST(GL_UNSIGNED_INT);
	EXPP_ADDCONST(GL_FLOAT);
	EXPP_ADDCONST(GL_DOUBLE);
	EXPP_ADDCONST(GL_2_BYTES);
	EXPP_ADDCONST(GL_3_BYTES);
	EXPP_ADDCONST(GL_4_BYTES);

	EXPP_ADDCONST(GL_CLEAR);
	EXPP_ADDCONST(GL_AND);
	EXPP_ADDCONST(GL_AND_REVERSE);
	EXPP_ADDCONST(GL_COPY);
	EXPP_ADDCONST(GL_AND_INVERTED);
	EXPP_ADDCONST(GL_NOOP);
	EXPP_ADDCONST(GL_XOR);
	EXPP_ADDCONST(GL_OR);
	EXPP_ADDCONST(GL_NOR);
	EXPP_ADDCONST(GL_EQUIV);
	EXPP_ADDCONST(GL_INVERT);
	EXPP_ADDCONST(GL_OR_REVERSE);
	EXPP_ADDCONST(GL_COPY_INVERTED);
	EXPP_ADDCONST(GL_OR_INVERTED);
	EXPP_ADDCONST(GL_NAND);
	EXPP_ADDCONST(GL_SET);

	EXPP_ADDCONST(GL_EMISSION);
	EXPP_ADDCONST(GL_SHININESS);
	EXPP_ADDCONST(GL_AMBIENT_AND_DIFFUSE);
	EXPP_ADDCONST(GL_COLOR_INDEXES);

	EXPP_ADDCONST(GL_MODELVIEW);
	EXPP_ADDCONST(GL_PROJECTION);
	EXPP_ADDCONST(GL_TEXTURE);

	EXPP_ADDCONST(GL_COLOR);
	EXPP_ADDCONST(GL_DEPTH);
	EXPP_ADDCONST(GL_STENCIL);

	EXPP_ADDCONST(GL_COLOR_INDEX);
	EXPP_ADDCONST(GL_STENCIL_INDEX);
	EXPP_ADDCONST(GL_DEPTH_COMPONENT);
	EXPP_ADDCONST(GL_RED);
	EXPP_ADDCONST(GL_GREEN);
	EXPP_ADDCONST(GL_BLUE);
	EXPP_ADDCONST(GL_ALPHA);
	EXPP_ADDCONST(GL_RGB);
	EXPP_ADDCONST(GL_RGBA);
	EXPP_ADDCONST(GL_LUMINANCE);
	EXPP_ADDCONST(GL_LUMINANCE_ALPHA);

	EXPP_ADDCONST(GL_BITMAP);

	EXPP_ADDCONST(GL_POINT);
	EXPP_ADDCONST(GL_LINE);
	EXPP_ADDCONST(GL_FILL);

	EXPP_ADDCONST(GL_RENDER);
	EXPP_ADDCONST(GL_FEEDBACK);
	EXPP_ADDCONST(GL_SELECT);

	EXPP_ADDCONST(GL_FLAT);
	EXPP_ADDCONST(GL_SMOOTH);

	EXPP_ADDCONST(GL_KEEP);
	EXPP_ADDCONST(GL_REPLACE);
	EXPP_ADDCONST(GL_INCR);
	EXPP_ADDCONST(GL_DECR);

	EXPP_ADDCONST(GL_VENDOR);
	EXPP_ADDCONST(GL_RENDERER);
	EXPP_ADDCONST(GL_VERSION);
	EXPP_ADDCONST(GL_EXTENSIONS);

	EXPP_ADDCONST(GL_S);
	EXPP_ADDCONST(GL_T);
	EXPP_ADDCONST(GL_R);
	EXPP_ADDCONST(GL_Q);

	EXPP_ADDCONST(GL_MODULATE);
	EXPP_ADDCONST(GL_DECAL);

	EXPP_ADDCONST(GL_TEXTURE_ENV_MODE);
	EXPP_ADDCONST(GL_TEXTURE_ENV_COLOR);

	EXPP_ADDCONST(GL_TEXTURE_ENV);

	EXPP_ADDCONST(GL_EYE_LINEAR);
	EXPP_ADDCONST(GL_OBJECT_LINEAR);
	EXPP_ADDCONST(GL_SPHERE_MAP);

	EXPP_ADDCONST(GL_TEXTURE_GEN_MODE);
	EXPP_ADDCONST(GL_OBJECT_PLANE);
	EXPP_ADDCONST(GL_EYE_PLANE);

	EXPP_ADDCONST(GL_NEAREST);
	EXPP_ADDCONST(GL_LINEAR);

	EXPP_ADDCONST(GL_NEAREST_MIPMAP_NEAREST);
	EXPP_ADDCONST(GL_LINEAR_MIPMAP_NEAREST);
	EXPP_ADDCONST(GL_NEAREST_MIPMAP_LINEAR);
	EXPP_ADDCONST(GL_LINEAR_MIPMAP_LINEAR);

	EXPP_ADDCONST(GL_TEXTURE_MAG_FILTER);
	EXPP_ADDCONST(GL_TEXTURE_MIN_FILTER);
	EXPP_ADDCONST(GL_TEXTURE_WRAP_S);
	EXPP_ADDCONST(GL_TEXTURE_WRAP_T);

	EXPP_ADDCONST(GL_CLAMP);
	EXPP_ADDCONST(GL_REPEAT);

	EXPP_ADDCONST(GL_CLIP_PLANE0);
	EXPP_ADDCONST(GL_CLIP_PLANE1);
	EXPP_ADDCONST(GL_CLIP_PLANE2);
	EXPP_ADDCONST(GL_CLIP_PLANE3);
	EXPP_ADDCONST(GL_CLIP_PLANE4);
	EXPP_ADDCONST(GL_CLIP_PLANE5);

	EXPP_ADDCONST(GL_LIGHT0);
	EXPP_ADDCONST(GL_LIGHT1);
	EXPP_ADDCONST(GL_LIGHT2);
	EXPP_ADDCONST(GL_LIGHT3);
	EXPP_ADDCONST(GL_LIGHT4);
	EXPP_ADDCONST(GL_LIGHT5);
	EXPP_ADDCONST(GL_LIGHT6);
	EXPP_ADDCONST(GL_LIGHT7);
	
	EXPP_ADDCONST(GL_POLYGON_OFFSET_UNITS);
	EXPP_ADDCONST(GL_POLYGON_OFFSET_POINT);
	EXPP_ADDCONST(GL_POLYGON_OFFSET_LINE);
	EXPP_ADDCONST(GL_POLYGON_OFFSET_FILL);
	EXPP_ADDCONST(GL_POLYGON_OFFSET_FACTOR);
	
	EXPP_ADDCONST(GL_TEXTURE_PRIORITY);
	EXPP_ADDCONST(GL_TEXTURE_RESIDENT);
	EXPP_ADDCONST(GL_TEXTURE_BINDING_1D);
	EXPP_ADDCONST(GL_TEXTURE_BINDING_2D);

	EXPP_ADDCONST(GL_VERTEX_SHADER);
	EXPP_ADDCONST(GL_FRAGMENT_SHADER);
	EXPP_ADDCONST(GL_COMPILE_STATUS);
	EXPP_ADDCONST(GL_ACTIVE_TEXTURE);

	EXPP_ADDCONST(GL_TEXTURE0);
	EXPP_ADDCONST(GL_TEXTURE1);
	EXPP_ADDCONST(GL_TEXTURE2);
	EXPP_ADDCONST(GL_TEXTURE3);
	EXPP_ADDCONST(GL_TEXTURE4);
	EXPP_ADDCONST(GL_TEXTURE5);
	EXPP_ADDCONST(GL_TEXTURE6);
	EXPP_ADDCONST(GL_TEXTURE7);
	EXPP_ADDCONST(GL_TEXTURE8);

	EXPP_ADDCONST(GL_MAX_TEXTURE_UNITS);
	
	EXPP_ADDCONST(GL_DEPTH_COMPONENT32);
	EXPP_ADDCONST(GL_TEXTURE_COMPARE_MODE);

	EXPP_ADDCONST(GL_MAX_VARYING_FLOATS);
	EXPP_ADDCONST(GL_MAX_VERTEX_ATTRIBS);	
	EXPP_ADDCONST(GL_MAX_VARYING_FLOATS);
	EXPP_ADDCONST(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
	EXPP_ADDCONST(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
	EXPP_ADDCONST(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	EXPP_ADDCONST(GL_MAX_TEXTURE_IMAGE_UNITS);
	EXPP_ADDCONST(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);

	return submodule;
}

static PyObject *Method_ShaderSource(PyObject *UNUSED(self), PyObject *args)
{
	unsigned int shader;
	const char *source;

	if (!PyArg_ParseTuple(args, "Is", &shader, &source))
		return NULL;

	glShaderSource(shader, 1, (const char **)&source, NULL);

	return Py_INCREF(Py_None), Py_None;
}


/** \} */