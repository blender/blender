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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* This is the Blender.BGL part of opy_draw.c, from the old bpython/intern
 * dir, with minor changes to adapt it to the new Python implementation.
 * The BGL submodule "wraps" OpenGL functions and constants, allowing script
 * writers to make OpenGL calls in their Python scripts for Blender.  The
 * more important original comments are marked with an @ symbol. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"

#include "interface.h"
#include "mydevice.h"  /*@ for all the event constants */

#include "Python.h"

#include "gen_utils.h"
#include "modules.h"

/*@ Buffer Object */
/*@ For Python access to OpenGL functions requiring a pointer. */

typedef struct _Buffer {
  PyObject_VAR_HEAD

  PyObject *parent;
  
  int type; /* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
  int ndimensions;
  int *dimensions;
  
  union {
    char  *asbyte;
    short *asshort;
    int   *asint;
    float *asfloat;

    void  *asvoid;
  } buf;
} Buffer;

static int type_size(int type);
static Buffer *make_buffer(int type, int ndimensions, int *dimensions);
static int Buffer_ass_slice(PyObject *self, int begin, int end, PyObject *seq);

static char Method_Buffer_doc[]=
"(type, dimensions, [template]) - Create a new Buffer object\n\n\
(type) - The format to store data in\n\
(dimensions) - An int or sequence specifying the dimensions of the buffer\n\
[template] - A sequence of matching dimensions to the buffer to be created\n\
  which will be used to initialize the Buffer.\n\n\
If a template is not passed in all fields will be initialized to 0.\n\n\
The type should be one of GL_BYTE, GL_SHORT, GL_INT, or GL_FLOAT.\n\
If the dimensions are specified as an int a linear buffer will be\n\
created. If a sequence is passed for the dimensions the buffer\n\
will have len(sequence) dimensions, where the size for each dimension\n\
is determined by the value in the sequence at that index.\n\n\
For example, passing [100, 100] will create a 2 dimensional\n\
square buffer. Passing [16, 16, 32] will create a 3 dimensional\n\
buffer which is twice as deep as it is wide or high.";

static PyObject *Method_Buffer (PyObject *self, PyObject *args);

/* Buffer sequence methods */

static int Buffer_len(PyObject *self);
static PyObject *Buffer_item(PyObject *self, int i);
static PyObject *Buffer_slice(PyObject *self, int begin, int end);
static int Buffer_ass_item(PyObject *self, int i, PyObject *v);
static int Buffer_ass_slice(PyObject *self, int begin, int end, PyObject *seq);

static PySequenceMethods Buffer_SeqMethods = {
  (inquiry)           Buffer_len,       /*sq_length*/
  (binaryfunc)        0,                /*sq_concat*/
  (intargfunc)        0,                /*sq_repeat*/
  (intargfunc)        Buffer_item,      /*sq_item*/
  (intintargfunc)     Buffer_slice,     /*sq_slice*/
  (intobjargproc)     Buffer_ass_item,  /*sq_ass_item*/
  (intintobjargproc)  Buffer_ass_slice, /*sq_ass_slice*/
};

static void Buffer_dealloc(PyObject *self);
static PyObject *Buffer_tolist(PyObject *self);
static PyObject *Buffer_dimensions(PyObject *self);
static PyObject *Buffer_getattr(PyObject *self, char *name);
static PyObject *Buffer_repr(PyObject *self);

PyTypeObject buffer_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                            /*ob_size*/
  "Buffer",                     /*tp_name*/
  sizeof(Buffer),               /*tp_basicsize*/
  0,                            /*tp_itemsize*/
  (destructor) Buffer_dealloc,  /*tp_dealloc*/
  (printfunc)  0,               /*tp_print*/
  (getattrfunc) Buffer_getattr, /*tp_getattr*/
  (setattrfunc) 0,              /*tp_setattr*/
  (cmpfunc) 0,                  /*tp_compare*/
  (reprfunc) Buffer_repr,       /*tp_repr*/
  0,                            /*tp_as_number*/
  &Buffer_SeqMethods,           /*tp_as_sequence*/
};

/* #ifndef __APPLE__ */

/*@ By golly George! It looks like fancy pants macro time!!! */

/*
#define int_str       "i"
#define int_var(number)   bgl_int##number
#define int_ref(number)   &bgl_int##number
#define int_def(number)   int int_var(number)

#define float_str     "f"
#define float_var(number) bgl_float##number
#define float_ref(number) &bgl_float##number
#define float_def(number) float float_var(number)
*/

/* TYPE_str is the string to pass to Py_ArgParse (for the format) */
/* TYPE_var is the name to pass to the GL function */
/* TYPE_ref is the pointer to pass to Py_ArgParse (to store in) */
/* TYPE_def is the C initialization of the variable */

#define void_str      ""
#define void_var(num)   
#define void_ref(num)   &bgl_var##num
#define void_def(num)   char bgl_var##num

#define buffer_str      "O!"
#define buffer_var(number)  (bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)  &buffer_Type, &bgl_buffer##number
#define buffer_def(number)  Buffer *bgl_buffer##number

/* GL Pointer fields, handled by buffer type */
/* GLdoubleP, GLfloatP, GLintP, GLuintP, GLshortP */

#define GLbooleanP_str      "O!"
#define GLbooleanP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLbooleanP_ref(number)  &buffer_Type, &bgl_buffer##number
#define GLbooleanP_def(number)  Buffer *bgl_buffer##number

#define GLbyteP_str     "O!"
#define GLbyteP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLbyteP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLbyteP_def(number) Buffer *bgl_buffer##number

#define GLubyteP_str      "O!"
#define GLubyteP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLubyteP_ref(number)  &buffer_Type, &bgl_buffer##number
#define GLubyteP_def(number)  Buffer *bgl_buffer##number

#define GLintP_str      "O!"
#define GLintP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLintP_ref(number)  &buffer_Type, &bgl_buffer##number
#define GLintP_def(number)  Buffer *bgl_buffer##number

#define GLuintP_str     "O!"
#define GLuintP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLuintP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLuintP_def(number) Buffer *bgl_buffer##number

#define GLshortP_str      "O!"
#define GLshortP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLshortP_ref(number)  &buffer_Type, &bgl_buffer##number
#define GLshortP_def(number)  Buffer *bgl_buffer##number

#define GLushortP_str     "O!"
#define GLushortP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLushortP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLushortP_def(number) Buffer *bgl_buffer##number

#define GLfloatP_str      "O!"
#define GLfloatP_var(number)  (bgl_buffer##number)->buf.asvoid
#define GLfloatP_ref(number)  &buffer_Type, &bgl_buffer##number
#define GLfloatP_def(number)  Buffer *bgl_buffer##number

#define GLdoubleP_str     "O!"
#define GLdoubleP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLdoubleP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLdoubleP_def(number) Buffer *bgl_buffer##number

#define GLclampfP_str     "O!"
#define GLclampfP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLclampfP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLclampfP_def(number) Buffer *bgl_buffer##number

#define GLvoidP_str     "O!"
#define GLvoidP_var(number) (bgl_buffer##number)->buf.asvoid
#define GLvoidP_ref(number) &buffer_Type, &bgl_buffer##number
#define GLvoidP_def(number) Buffer *bgl_buffer##number

#define buffer_str      "O!"
#define buffer_var(number)  (bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)  &buffer_Type, &bgl_buffer##number
#define buffer_def(number)  Buffer *bgl_buffer##number

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
#define GLsizei_str       "i"
#define GLsizei_var(num)    bgl_var##num
#define GLsizei_ref(num)    &bgl_var##num
#define GLsizei_def(num)    int GLsizei_var(num)

/* typedef unsigned char GLubyte; */
#define GLubyte_str       "b"
#define GLubyte_var(num)    bgl_var##num
#define GLubyte_ref(num)    &bgl_var##num
#define GLubyte_def(num)    /* unsigned */ char GLubyte_var(num)

/* typedef unsigned short GLushort; */
#define GLushort_str      "h"
#define GLushort_var(num)   bgl_var##num
#define GLushort_ref(num)   &bgl_var##num
#define GLushort_def(num)   /* unsigned */ short GLushort_var(num)

/* typedef unsigned int GLuint; */
#define GLuint_str        "i"
#define GLuint_var(num)     bgl_var##num
#define GLuint_ref(num)     &bgl_var##num
#define GLuint_def(num)     /* unsigned */ int GLuint_var(num)

/* typedef float GLfloat; */
#define GLfloat_str       "f"
#define GLfloat_var(num)    bgl_var##num
#define GLfloat_ref(num)    &bgl_var##num
#define GLfloat_def(num)    float GLfloat_var(num)

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

/* typedef void GLvoid; */
/* #define GLvoid_str       "" */
/* #define GLvoid_var(num)      bgl_var##num */
/* #define GLvoid_ref(num)      &bgl_var##num */
/* #define GLvoid_def(num)      char bgl_var##num */

#define arg_def1(a1)          a1##_def(1)
#define arg_def2(a1, a2)        arg_def1(a1); a2##_def(2)
#define arg_def3(a1, a2, a3)      arg_def2(a1, a2); a3##_def(3)
#define arg_def4(a1, a2, a3, a4)    arg_def3(a1, a2, a3); a4##_def(4)
#define arg_def5(a1, a2, a3, a4, a5)  arg_def4(a1, a2, a3, a4); a5##_def(5)
#define arg_def6(a1, a2, a3, a4, a5, a6)arg_def5(a1, a2, a3, a4, a5); a6##_def(6)
#define arg_def7(a1, a2, a3, a4, a5, a6, a7)arg_def6(a1, a2, a3, a4, a5, a6); a7##_def(7)
#define arg_def8(a1, a2, a3, a4, a5, a6, a7, a8)arg_def7(a1, a2, a3, a4, a5, a6, a7); a8##_def(8)
#define arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_def8(a1, a2, a3, a4, a5, a6, a7, a8); a9##_def(9)
#define arg_def10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9); a10##_def(10)

#define arg_var1(a1)          a1##_var(1)
#define arg_var2(a1, a2)        arg_var1(a1), a2##_var(2)
#define arg_var3(a1, a2, a3)      arg_var2(a1, a2), a3##_var(3)
#define arg_var4(a1, a2, a3, a4)    arg_var3(a1, a2, a3), a4##_var(4)
#define arg_var5(a1, a2, a3, a4, a5)  arg_var4(a1, a2, a3, a4), a5##_var(5)
#define arg_var6(a1, a2, a3, a4, a5, a6)arg_var5(a1, a2, a3, a4, a5), a6##_var(6)
#define arg_var7(a1, a2, a3, a4, a5, a6, a7)arg_var6(a1, a2, a3, a4, a5, a6), a7##_var(7)
#define arg_var8(a1, a2, a3, a4, a5, a6, a7, a8)arg_var7(a1, a2, a3, a4, a5, a6, a7), a8##_var(8)
#define arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_var8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_var(9)
#define arg_var10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_var(10)

#define arg_ref1(a1)          a1##_ref(1)
#define arg_ref2(a1, a2)        arg_ref1(a1), a2##_ref(2)
#define arg_ref3(a1, a2, a3)      arg_ref2(a1, a2), a3##_ref(3)
#define arg_ref4(a1, a2, a3, a4)    arg_ref3(a1, a2, a3), a4##_ref(4)
#define arg_ref5(a1, a2, a3, a4, a5)  arg_ref4(a1, a2, a3, a4), a5##_ref(5)
#define arg_ref6(a1, a2, a3, a4, a5, a6)arg_ref5(a1, a2, a3, a4, a5), a6##_ref(6)
#define arg_ref7(a1, a2, a3, a4, a5, a6, a7)arg_ref6(a1, a2, a3, a4, a5, a6), a7##_ref(7)
#define arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8)arg_ref7(a1, a2, a3, a4, a5, a6, a7), a8##_ref(8)
#define arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_ref(9)
#define arg_ref10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_ref(10)

#define arg_str1(a1)          a1##_str
#define arg_str2(a1, a2)        arg_str1(a1) a2##_str
#define arg_str3(a1, a2, a3)      arg_str2(a1, a2) a3##_str
#define arg_str4(a1, a2, a3, a4)    arg_str3(a1, a2, a3) a4##_str
#define arg_str5(a1, a2, a3, a4, a5)  arg_str4(a1, a2, a3, a4) a5##_str
#define arg_str6(a1, a2, a3, a4, a5, a6)arg_str5(a1, a2, a3, a4, a5) a6##_str
#define arg_str7(a1, a2, a3, a4, a5, a6, a7)arg_str6(a1, a2, a3, a4, a5, a6) a7##_str
#define arg_str8(a1, a2, a3, a4, a5, a6, a7, a8)arg_str7(a1, a2, a3, a4, a5, a6, a7) a8##_str
#define arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_str8(a1, a2, a3, a4, a5, a6, a7, a8) a9##_str
#define arg_str10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9) a10##_str

#define ret_def_void  
#define ret_set_void  
#define ret_ret_void    return EXPP_incr_ret(Py_None)

#define ret_def_GLint   int ret_int
#define ret_set_GLint   ret_int= 
#define ret_ret_GLint   return PyInt_FromLong(ret_int);

#define ret_def_GLuint    unsigned int ret_uint
#define ret_set_GLuint    ret_uint= 
#define ret_ret_GLuint    return PyInt_FromLong((long) ret_uint);

#define ret_def_GLenum    unsigned int ret_uint
#define ret_set_GLenum    ret_uint= 
#define ret_ret_GLenum    return PyInt_FromLong((long) ret_uint);

#define ret_def_GLboolean unsigned char ret_bool
#define ret_set_GLboolean ret_bool= 
#define ret_ret_GLboolean return PyInt_FromLong((long) ret_bool);

#define ret_def_GLstring  const unsigned char *ret_str;
#define ret_set_GLstring  ret_str= 
#define ret_ret_GLstring  return PyString_FromString(ret_str);

#define BGL_Wrap(nargs, funcname, ret, arg_list) \
static PyObject *Method_##funcname (PyObject *self, PyObject *args) {\
  arg_def##nargs arg_list; \
  ret_def_##ret; \
  if(!PyArg_ParseTuple(args, arg_str##nargs arg_list, arg_ref##nargs arg_list)) return NULL;\
  ret_set_##ret gl##funcname (arg_var##nargs arg_list);\
  ret_ret_##ret; \
}

/* #endif */

PyObject *M_BGL_Init(void); 
