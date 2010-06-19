/**
 * $Id$
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

#ifndef CSG_BOOLEANOPS_H
#define CSG_BOOLEANOPS_H


/**
 * @section Interface structures for CSG module.
 * This interface falls into 2 categories.
 * The first section deals with an abstract mesh description 
 * between blender and this module. The second deals with 
 * the module functions. 
 * The CSG module needs to know about the following entities:
 */

/**
 * CSG_IFace -- an interface polygon structure. 
 * vertex_index is a fixed size array of 4 elements containing indices into
 * an abstract vertex container. 3 or 4 of these elements may be used to
 * describe either quads or triangles.
 * vertex_number is the number of vertices in this face - either 3 or 4.
 * vertex_colors is an array of {r,g,b} triplets one for each vertex index.
 * tex_coords is an array of {u,v} triplets one for each vertex index.
 * user_data is a pointer to arbitary data of fixed width ,
 * this data is copied around with the face, and duplicated if a face is
 * split. Contains things like material index. 
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  {
  int vertex_index[4];
  int vertex_number;
  int orig_face;
} CSG_IFace;

/**
 * CSG_IVertex -- an interface vertex structure.
 * position is the location of the vertex in 3d space.
 */

typedef struct  {
  float position[3];
} CSG_IVertex;

/**
 * The actual useful data contained in a group of faces is
 * described by the following struct
 */

/**
 * @section Iterator abstraction.
 * 
 * The CSG module asks blender to fill in an instance of the above
 * structure, and requests blender to move up and down (iterate) through
 * it's face and vertex containers.
 * 
 * An iterator supports the following functions.
 * int IsDone(iterator *it)  -- returns true if the iterator has reached
 * the end of it's container.
 *
 * void Fill(iterator *it,DataType *data) -- Return the contents of the
 * container at the current iterator position.
 *
 * void Step(iterator *it) -- increment the iterator to the next position
 * in the container. 
 *
 * The iterator is used in the following manner.
 * 
 *   MyIterator * iterator = ... 
 *   DataType data; 
 * 
 *   while (!IsDone(iterator)) {
 *		Fill(iterator,&data);
 *		//process information pointed to by data 
 *		...
 *		Step(iterator);
 *	 } 
 * 
 * The CSG module does not want to know about the implementation of these
 * functions  so we use the c function ptr mechanism to hide them. Our
 * iterator descriptor now looks like this.
 */

typedef void* CSG_IteratorPtr;

typedef int (*CSG_FaceItDoneFunc)(CSG_IteratorPtr it);
typedef void (*CSG_FaceItFillFunc)(CSG_IteratorPtr it,CSG_IFace *face);
typedef void (*CSG_FaceItStepFunc)(CSG_IteratorPtr it);
typedef void (*CSG_FaceItResetFunc)(CSG_IteratorPtr it);

typedef struct CSG_FaceIteratorDescriptor {
	CSG_IteratorPtr it;
	CSG_FaceItDoneFunc Done;
	CSG_FaceItFillFunc Fill;
	CSG_FaceItStepFunc Step;
	CSG_FaceItResetFunc Reset;
	unsigned int num_elements;
} CSG_FaceIteratorDescriptor; 

/**
 * Similarly to walk through the vertex arrays we have.
 */
typedef int (*CSG_VertexItDoneFunc)(CSG_IteratorPtr it);
typedef void (*CSG_VertexItFillFunc)(CSG_IteratorPtr it,CSG_IVertex *face);
typedef void (*CSG_VertexItStepFunc)(CSG_IteratorPtr it);
typedef void (*CSG_VertexItResetFunc)(CSG_IteratorPtr it);

typedef struct CSG_VertexIteratorDescriptor {
	CSG_IteratorPtr it;
	CSG_VertexItDoneFunc Done;
	CSG_VertexItFillFunc Fill;
	CSG_VertexItStepFunc Step;
	CSG_VertexItResetFunc Reset;
	unsigned int num_elements;
} CSG_VertexIteratorDescriptor; 

/**
 * The actual iterator structures are not exposed to the CSG module, they
 * will contain datatypes specific to blender.
 */

/**
 * @section CSG Module interface functions.
 * 
 * The functions below are to be used in the following way:
 * 
 *  // Create a boolean operation handle
 *  CSG_BooleanOperation *operation = CSG_NewBooleanFunction();
 *  if (operation == NULL) {
 *		// deal with low memory exception
 *  }
 *
 *  // Report to the user if they will loose any data!
 *  ...
 * 
 *  // Get some mesh iterators for your mesh data structures
 *  CSG_FaceIteratorDescriptor obA_faces = ...
 *  CSG_VertexIteratorDescriptor obA_verts = ...
 *  
 *  // same for object B
 *  CSG_FaceIteratorDescriptor obB_faces = ...
 *  CSG_VertexIteratorDescriptor obB_verts = ...
 *  
 *  // perform the operation...!
 *
 *  int success = CSG_PerformBooleanOperation(
 *     operation,
 *     e_csg_intersection,
 *     obA_faces,
 *     obA_vertices,
 *     obB_faces,
 *     obB_vertices
 *  );
 *
 *  // if the operation failes report miserable faiulre to user
 *  // and clear up data structures.
 *  if (!success) {
 *    ...
 *    CSG_FreeBooleanOperation(operation);
 *    return;
 *  }
 *
 *  // read the new mesh vertices back from the module
 *  // and assign to your own mesh structure.
 *
 *  // First we need to create a CSG_IVertex so the module can fill it in.
 *  CSG_IVertex vertex;
 *  CSG_VertexIteratorDescriptor * verts_it = CSG_OutputVertexDescriptor(operation);
 *
 *  // initialize your vertex container with the number of verts (verts_it->num_elements)
 * 
 *  while (!verts_it->Done(verts_it->it)) {
 *		verts_it->Fill(verts_it->it,&vertex);
 *
 *      // create a new vertex of your own type and add it
 *      // to your mesh structure.
 *      verts_it->Step(verts_it->it);
 *  }
 *  // Free the vertex iterator
 *	CSG_FreeVertexDescriptor(verts_it);
 * 
 *  // similarly for faces.
 *  CSG_IFace face;
 *
 *  // you may need to reserve some memory in face->user_data here.
 * 
 *  // initialize your face container with the number of faces (faces_it->num_elements)
 * 
 *  CSG_FaceIteratorDescriptor * faces_it = CSG_OutputFaceDescriptor(operation);
 * 
 *  while (!faces_it->Done(faces_it->it)) {
 *		faces_it->Fill(faces_it->it,&face);
 *
 *      // create a new face of your own type and add it
 *      // to your mesh structure.
 *      faces_it->Step(&faces_it->it);
 *  }
 *	
 *  // Free the face iterator
 *	CSG_FreeVertexDescriptor(faces_it);
 *
 *  // that's it free the operation.
 *
 *  CSG_FreeBooleanOperation(operation);
 *  return;
 *  
 */

/**
 * Description of boolean operation type.
 */

typedef enum {
	e_csg_union,
	e_csg_intersection,
	e_csg_difference,
	e_csg_classify
} CSG_OperationType;

/**
 * 'Handle' into the CSG module that identifies a particular CSG operation.
 *  the pointer CSG_info containers module specific data, and should not
 *  be touched in anyway outside of the module.
 */

typedef struct {
	void *CSG_info;
} CSG_BooleanOperation;

/**
 * Return a ptr to a CSG_BooleanOperation object allocated
 * on the heap. The CSG module owns the memory associated with 
 * the returned ptr, use CSG_FreeBooleanOperation() to free this memory.
 */
	CSG_BooleanOperation * 
CSG_NewBooleanFunction(
	void
);

/**
 * Attempt to perform a boolean operation between the 2 objects of the 
 * desired type. This may fail due to an internal error or lack of memory.
 * In this case 0 is returned, otehrwise 1 is returned indicating success.
 * @param operation is a 'handle' into the CSG_Module created by CSG_NewBooleanFunction()
 * @param op_type is the operation to perform.
 * @param obAFaces is an iterator over the faces of objectA,
 * @param obAVertices is an iterator over the vertices of objectA
 * @param obAFaces is an iterator over the faces of objectB,
 * @param obAVertices is an iterator over the vertices of objectB
 * @param interp_func the face_vertex data interpolation function.(see above)
 * 
 * All iterators must be valid and pointing to the first element in their
 * respective containers.
 */
	int
CSG_PerformBooleanOperation(
	CSG_BooleanOperation * operation,
	CSG_OperationType op_type,
	CSG_FaceIteratorDescriptor obAFaces,
	CSG_VertexIteratorDescriptor obAVertices,
	CSG_FaceIteratorDescriptor obBFaces,
	CSG_VertexIteratorDescriptor obBVertices
);

/**
 * If the a boolean operation was successful, you may access the results
 * through the following functions.
 *
 * CSG_OuputFaceDescriptor() returns a ptr to a CSG_FaceIteratorDesciptor 
 * allocated on the heap and owned by the CSG module. The iterator is
 * positioned at the start of the internal face container.
 * CSG_OutputVertexDescriptor() returns a ptr to a CSG_VertexIteratorDescriptor
 * allocated on the heap and owned by the CSG module. The iterator is
 * positioned at the start of the internal vertex container.
 * There is no function to rewind an iterator but you may obtain more
 * than one
 * iterator at a time. Please use the function CSG_FreeFaceIterator()
 * and CSG_FreeVertexIterator to free internal memory allocated for these
 * iterators.
 * 
 * If the last operation was not successful, these functions return NULL.
 */
	int
CSG_OutputFaceDescriptor(
	CSG_BooleanOperation * operation,
	CSG_FaceIteratorDescriptor * output
);

	int
CSG_OutputVertexDescriptor(
	CSG_BooleanOperation * operation,
	CSG_VertexIteratorDescriptor *output
);

/**
 * Clean up functions.
 * Free internal memory associated with CSG interface structures. You must
 * call these functions on any structures created by the module, even if
 * subsequent operations did not succeed.
 */
	void
CSG_FreeVertexDescriptor(
	CSG_VertexIteratorDescriptor * v_descriptor
);

	void
CSG_FreeFaceDescriptor(
	CSG_FaceIteratorDescriptor * f_descriptor
);

/**
 * Free the memory associated with a boolean operation. 
 * NOTE any iterator descriptor describing the output will become
 * invalid after this call and should be freed immediately.
 */
	void
CSG_FreeBooleanOperation(
	CSG_BooleanOperation *operation
);

#ifdef __cplusplus
}
#endif



#endif

