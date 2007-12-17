/* 
 * $Id: sceneSequence.h 11400 2007-07-28 09:26:53Z campbellbarton $
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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_SEQUENCE_H
#define EXPP_SEQUENCE_H

#include <Python.h>
#include "DNA_sequence_types.h"

/* The Sequence PyTypeObject defined in Sequence.c */
extern PyTypeObject Sequence_Type;
extern PyTypeObject SceneSeq_Type;

#define BPy_Sequence_Check(v)       ((v)->ob_type == &Sequence_Type)
#define BPy_SceneSeq_Check(v)       ((v)->ob_type == &SceneSeq_Type)


/*****************************************************************************/
/* Python BPy_Sequence structure definition.                                  */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD /* required python macro   */
	
	/*one of the folowing must be NULL*/
	struct Sequence *seq;/* if not NULL, this sequence is a Metaseq */
	
	/* used for looping over the scene or the strips strips */
	struct Sequence *iter;/* if not NULL, this sequence is a Metaseq */
	
	struct Scene *scene;
	
} BPy_Sequence;



/*****************************************************************************/
/* Python BPy_Sequence structure definition.                                  */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD /* required python macro   */
	
	/*one of the folowing must be NULL*/
	struct Scene *scene; /* if not NULL, this sequence is the root sequence for the scene*/
	
	/* used for looping over the scene or the strips strips */
	struct Sequence *iter;/* if not NULL, this sequence is a Metaseq */
} BPy_SceneSeq;

PyObject *Sequence_Init( void );
PyObject *Sequence_CreatePyObject( struct Sequence * seq, struct Sequence * iter, struct Scene * scn);
PyObject *SceneSeq_CreatePyObject( struct Scene * scn, struct Sequence * iter);
struct Sequence *Sequence_FromPyObject( PyObject * py_obj );

#endif				/* EXPP_SEQUENCE_H */
