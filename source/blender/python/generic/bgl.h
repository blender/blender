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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/bgl.h
 *  \ingroup pygen
 */

#ifndef __BGL_H__
#define __BGL_H__

PyObject *BPyInit_bgl(void);

struct _Buffer *BGL_MakeBuffer(int type, int ndimensions, int *dimensions, void *initbuffer);

int BGL_typeSize(int type);

/**
 * Buffer Object
 *
 * For Python access to OpenGL functions requiring a pointer.
 */
typedef struct _Buffer {
	PyObject_VAR_HEAD 
	PyObject *parent;

	int type;		/* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
	int ndimensions;
	int *dimensions;

	union {
		char *asbyte;
		short *asshort;
		int *asint;
		float *asfloat;
		double *asdouble;

		void *asvoid;
	} buf;
} Buffer;

/** The type object */
extern PyTypeObject BGL_bufferType;

#endif /* __BGL_H__ */
