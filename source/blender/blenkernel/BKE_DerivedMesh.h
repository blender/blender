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

#ifndef __BKE_DERIVEDMESH_H__
#define __BKE_DERIVEDMESH_H__

/*
 * Basic design of the DerivedMesh system:
 *
 * DerivedMesh is a common set of interfaces for mesh systems.
 *
 * There are three main mesh data structures in Blender: Mesh, CDDM, and BMesh.
 * These, and a few others, all implement DerivedMesh interfaces, 
 * which contains unified drawing interfaces, a few utility interfaces, 
 * and a bunch of read-only interfaces intended mostly for conversion from 
 * one format to another.
 *
 * All Mesh structures in blender make use of CustomData, which is used to store
 * per-element attributes and interpolate them (e.g. uvs, vcols, vgroups, etc).
 * 
 * Mesh is the "serialized" structure, used for storing object-mode mesh data
 * and also for saving stuff to disk.  It's interfaces are also what DerivedMesh
 * uses to communicate with.
 * 
 * CDDM is a little mesh library, that uses Mesh data structures in the backend.
 * It's mostly used for modifiers, and has the advantages of not taking much
 * resources.
 *
 * BMesh is a full-on brep, used for editmode, some modifiers, etc.  It's much
 * more capable (if memory-intensive) then CDDM.
 *
 * DerivedMesh is somewhat hackish.  Many places assumes that a DerivedMesh is
 * a CDDM (most of the time by simply copying it and converting it to one).
 * CDDM is the original structure for modifiers, but has since been superseded
 * by BMesh, at least for the foreseeable future.
 */

/*
 * Note: This sturcture is read-only, for all practical purposes.
 *       At some point in the future, we may want to consider
 *       creating a replacement structure that implements a proper
 *       abstract mesh kernel interface.  Or, we can leave this
 *       as it is and stick with using BMesh and CDDM.
 */


#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_bvhutils.h"

struct MVert;
struct MEdge;
struct MFace;
struct MTFace;
struct Object;
struct Scene;
struct Mesh;
struct BMEditMesh;
struct KeyBlock;
struct ModifierData;
struct MCol;
struct ColorBand;
struct GPUVertexAttribs;
struct GPUDrawObject;
struct BMEditMesh;
struct ListBase;
struct PBVH;

/* number of sub-elements each mesh element has (for interpolation) */
#define SUB_ELEMS_VERT 0
#define SUB_ELEMS_EDGE 2
#define SUB_ELEMS_FACE 50

/*
 * Note: all mface interfaces now officially operate on tesselated data.
 *       Also, the mface origindex layer indexes mpolys, not mfaces.
 */

typedef struct DMGridData {
	float co[3];
	float no[3];
} DMGridData;

typedef struct DMGridAdjacency {
	int index[4];
	int rotation[4];
} DMGridAdjacency;

typedef enum DerivedMeshType {
	DM_TYPE_CDDM,
	DM_TYPE_EDITBMESH,
	DM_TYPE_CCGDM
} DerivedMeshType;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
	/* Private DerivedMesh data, only for internal DerivedMesh use */
	CustomData vertData, edgeData, faceData, loopData, polyData;
	int numVertData, numEdgeData, numTessFaceData, numLoopData, numPolyData;
	int needsFree; /* checked on ->release, is set to 0 for cached results */
	int deformedOnly; /* set by modifier stack if only deformed from original */
	BVHCache bvhCache;
	struct GPUDrawObject *drawObject;
	DerivedMeshType type;
	float auto_bump_scale;

	/* calculate vert and face normals */
	void (*calcNormals)(DerivedMesh *dm);

	/* recalculates mesh tessellation */
	void (*recalcTessellation)(DerivedMesh *dm);

	/* Misc. Queries */

	/* Also called in Editmode */
	int (*getNumVerts)(DerivedMesh *dm);
	int (*getNumEdges)(DerivedMesh *dm);
	int (*getNumTessFaces)(DerivedMesh *dm);
	int (*getNumLoops)(DerivedMesh *dm);
	int (*getNumPolys)(DerivedMesh *dm);

	/* copy a single vert/edge/tesselated face from the derived mesh into
	 * *{vert/edge/face}_r. note that the current implementation
	 * of this function can be quite slow, iterating over all
	 * elements (editmesh)
	 */
	void (*getVert)(DerivedMesh *dm, int index, struct MVert *vert_r);
	void (*getEdge)(DerivedMesh *dm, int index, struct MEdge *edge_r);
	void (*getTessFace)(DerivedMesh *dm, int index, struct MFace *face_r);

	/* return a pointer to the entire array of verts/edges/face from the
	 * derived mesh. if such an array does not exist yet, it will be created,
	 * and freed on the next ->release(). consider using getVert/Edge/Face if
	 * you are only interested in a few verts/edges/faces.
	 */
	struct MVert *(*getVertArray)(DerivedMesh *dm);
	struct MEdge *(*getEdgeArray)(DerivedMesh *dm);
	struct MFace *(*getTessFaceArray)(DerivedMesh *dm);
	struct MLoop *(*getLoopArray)(DerivedMesh *dm);
	struct MPoly *(*getPolyArray)(DerivedMesh *dm);

	/* copy all verts/edges/faces from the derived mesh into
	 * *{vert/edge/face}_r (must point to a buffer large enough)
	 */
	void (*copyVertArray)(DerivedMesh *dm, struct MVert *vert_r);
	void (*copyEdgeArray)(DerivedMesh *dm, struct MEdge *edge_r);
	void (*copyTessFaceArray)(DerivedMesh *dm, struct MFace *face_r);
	void (*copyLoopArray)(DerivedMesh *dm, struct MLoop *loop_r);
	void (*copyPolyArray)(DerivedMesh *dm, struct MPoly *poly_r);

	/* return a copy of all verts/edges/faces from the derived mesh
	 * it is the caller's responsibility to free the returned pointer
	 */
	struct MVert *(*dupVertArray)(DerivedMesh *dm);
	struct MEdge *(*dupEdgeArray)(DerivedMesh *dm);
	struct MFace *(*dupTessFaceArray)(DerivedMesh *dm);
	struct MLoop *(*dupLoopArray)(DerivedMesh *dm);
	struct MPoly *(*dupPolyArray)(DerivedMesh *dm);

	/* return a pointer to a single element of vert/edge/face custom data
	 * from the derived mesh (this gives a pointer to the actual data, not
	 * a copy)
	 */
	void *(*getVertData)(DerivedMesh *dm, int index, int type);
	void *(*getEdgeData)(DerivedMesh *dm, int index, int type);
	void *(*getTessFaceData)(DerivedMesh *dm, int index, int type);

	/* return a pointer to the entire array of vert/edge/face custom data
	 * from the derived mesh (this gives a pointer to the actual data, not
	 * a copy)
	 */
	void *(*getVertDataArray)(DerivedMesh *dm, int type);
	void *(*getEdgeDataArray)(DerivedMesh *dm, int type);
	void *(*getTessFaceDataArray)(DerivedMesh *dm, int type);
	
	/* retrieves the base CustomData structures for 
	 * verts/edges/tessfaces/loops/facdes*/
	CustomData *(*getVertDataLayout)(DerivedMesh *dm);
	CustomData *(*getEdgeDataLayout)(DerivedMesh *dm);
	CustomData *(*getTessFaceDataLayout)(DerivedMesh *dm);
	CustomData *(*getLoopDataLayout)(DerivedMesh *dm);
	CustomData *(*getPolyDataLayout)(DerivedMesh *dm);
	
	/*copies all customdata for an element source into dst at index dest*/
	void (*copyFromVertCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);
	void (*copyFromEdgeCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);
	void (*copyFromFaceCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);
	
	/* optional grid access for subsurf */
	int (*getNumGrids)(DerivedMesh *dm);
	int (*getGridSize)(DerivedMesh *dm);
	DMGridData **(*getGridData)(DerivedMesh *dm);
	DMGridAdjacency *(*getGridAdjacency)(DerivedMesh *dm);
	int *(*getGridOffset)(DerivedMesh *dm);

	/* Iterate over each mapped vertex in the derived mesh, calling the
	 * given function with the original vert and the mapped vert's new
	 * coordinate and normal. For historical reasons the normal can be
	 * passed as a float or short array, only one should be non-NULL.
	 */
	void (*foreachMappedVert)(
						  DerivedMesh *dm,
						  void (*func)(void *userData, int index, float *co,
									   float *no_f, short *no_s),
						  void *userData);

	/* Iterate over each mapped edge in the derived mesh, calling the
	 * given function with the original edge and the mapped edge's new
	 * coordinates.
	 */
	void (*foreachMappedEdge)(DerivedMesh *dm,
							  void (*func)(void *userData, int index,
										   float *v0co, float *v1co),
							  void *userData);

	/* Iterate over each mapped face in the derived mesh, calling the
	 * given function with the original face and the mapped face's (or
	 * faces') center and normal.
	 */
	void (*foreachMappedFaceCenter)(DerivedMesh *dm,
									void (*func)(void *userData, int index,
												 float *cent, float *no),
									void *userData);

	/* Iterate over all vertex points, calling DO_MINMAX with given args.
	 *
	 * Also called in Editmode
	 */
	void (*getMinMax)(DerivedMesh *dm, float min_r[3], float max_r[3]);

	/* Direct Access Operations */
	/*  o Can be undefined */
	/*  o Must be defined for modifiers that only deform however */

	/* Get vertex location, undefined if index is not valid */
	void (*getVertCo)(DerivedMesh *dm, int index, float co_r[3]);

	/* Fill the array (of length .getNumVerts()) with all vertex locations */
	void (*getVertCos)(DerivedMesh *dm, float (*cos_r)[3]);

	/* Get smooth vertex normal, undefined if index is not valid */
	void (*getVertNo)(DerivedMesh *dm, int index, float no_r[3]);

	/* Get a map of vertices to faces
	 */
	struct ListBase *(*getPolyMap)(struct Object *ob, DerivedMesh *dm);

	/* Get the BVH used for paint modes
	 */
	struct PBVH *(*getPBVH)(struct Object *ob, DerivedMesh *dm);

	/* Drawing Operations */

	/* Draw all vertices as bgl points (no options) */
	void (*drawVerts)(DerivedMesh *dm);

	/* Draw edges in the UV mesh (if exists) */
	void (*drawUVEdges)(DerivedMesh *dm);

	/* Draw all edges as lines (no options) 
	 *
	 * Also called for *final* editmode DerivedMeshes
	 */
	void (*drawEdges)(DerivedMesh *dm, int drawLooseEdges, int drawAllEdges);
	
	/* Draw all loose edges (edges w/ no adjoining faces) */
	void (*drawLooseEdges)(DerivedMesh *dm);

	/* Draw all faces
	 *  o Set face normal or vertex normal based on inherited face flag
	 *  o Use inherited face material index to call setMaterial
	 *  o Only if setMaterial returns true
	 *
	 * Also called for *final* editmode DerivedMeshes
	 */
	void (*drawFacesSolid)(DerivedMesh *dm, float (*partial_redraw_planes)[4],
						   int fast, int (*setMaterial)(int, void *attribs));

	/* Draw all faces
	 *  o If useTwoSided, draw front and back using col arrays
	 *  o col1,col2 are arrays of length numFace*4 of 4 component colors
	 *    in ABGR format, and should be passed as per-face vertex color.
	 */
	void (*drawFacesColored)(DerivedMesh *dm, int useTwoSided,
							 unsigned char *col1, unsigned char *col2);

	/* Draw all faces using MTFace 
	 *  o Drawing options too complicated to enumerate, look at code.
	 */
	void (*drawFacesTex)(DerivedMesh *dm,
	                     int (*setDrawOptions)(struct MTFace *tface,
	                     int has_vcol, int matnr),
						int (*compareDrawOptions)(void *userData,
							 int cur_index,
							 int next_index),
						void *userData);

	/* Draw all faces with GLSL materials
	 *  o setMaterial is called for every different material nr
	 *  o Only if setMaterial returns true
	 */
	void (*drawFacesGLSL)(DerivedMesh *dm,
		int (*setMaterial)(int, void *attribs));

	/* Draw mapped faces (no color, or texture)
	 *  o Only if !setDrawOptions or
	 *    setDrawOptions(userData, mapped-face-index, drawSmooth_r)
	 *    returns true
	 *
	 * If drawSmooth is set to true then vertex normals should be set and
	 * glShadeModel called with GL_SMOOTH. Otherwise the face normal should
	 * be set and glShadeModel called with GL_FLAT.
	 *
	 * The setDrawOptions is allowed to not set drawSmooth (for example, when
	 * lighting is disabled), in which case the implementation should draw as
	 * smooth shaded.
	 */
	void (*drawMappedFaces)(DerivedMesh *dm,
							int (*setDrawOptions)(void *userData, int index,
												  int *drawSmooth_r),
							int (*setMaterial)(int, void *attribs),
							int (*compareDrawOptions)(void *userData,
							                          int cur_index,
							                          int next_index),
							void *userData, int useColors);

	/* Draw mapped faces using MTFace 
	 *  o Drawing options too complicated to enumerate, look at code.
	 */
	void (*drawMappedFacesTex)(DerivedMesh *dm,
							   int (*setDrawOptions)(void *userData,
													 int index),
							   int (*compareDrawOptions)(void *userData,
							                             int cur_index,
							                             int next_index),
							   void *userData);

	/* Draw mapped faces with GLSL materials
	 *  o setMaterial is called for every different material nr
	 *  o setDrawOptions is called for every face
	 *  o Only if setMaterial and setDrawOptions return true
	 */
	void (*drawMappedFacesGLSL)(DerivedMesh *dm,
		int (*setMaterial)(int, void *attribs),
		int (*setDrawOptions)(void *userData, int index),
		void *userData);

	/* Draw mapped edges as lines
	 *  o Only if !setDrawOptions or setDrawOptions(userData, mapped-edge)
	 *    returns true
	 */
	void (*drawMappedEdges)(DerivedMesh *dm,
							int (*setDrawOptions)(void *userData, int index),
							void *userData);

	/* Draw mapped edges as lines with interpolation values
	 *  o Only if !setDrawOptions or
	 *    setDrawOptions(userData, mapped-edge, mapped-v0, mapped-v1, t)
	 *    returns true
	 *
	 * NOTE: This routine is optional!
	 */
	void (*drawMappedEdgesInterp)(DerivedMesh *dm, 
								  int (*setDrawOptions)(void *userData,
														int index), 
								  void (*setDrawInterpOptions)(void *userData,
															   int index,
															   float t),
								  void *userData);

	/* Draw all faces with materials
	 *  o setMaterial is called for every different material nr
	 *  o setFace is called to verify if a face must be hidden
	 */
	void (*drawMappedFacesMat)(DerivedMesh *dm,
		void (*setMaterial)(void *userData, int, void *attribs),
		int (*setFace)(void *userData, int index), void *userData);

	/* Release reference to the DerivedMesh. This function decides internally
	 * if the DerivedMesh will be freed, or cached for later use. */
	void (*release)(DerivedMesh *dm);
};

/* utility function to initialize a DerivedMesh's function pointers to
 * the default implementation (for those functions which have a default)
 */
void DM_init_funcs(DerivedMesh *dm);

/* utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces (doesn't allocate memory for them, just
 * sets up the custom data layers)
 */
void DM_init(DerivedMesh *dm, DerivedMeshType type, int numVerts, int numEdges, 
             int numFaces, int numLoops, int numPolys);

/* utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces, with a layer setup copied from source
 */
void DM_from_template(DerivedMesh *dm, DerivedMesh *source,
			  DerivedMeshType type,
			  int numVerts, int numEdges, int numFaces,
		      int numLoops, int numPolys);

/* utility function to release a DerivedMesh's layers
 * returns 1 if DerivedMesh has to be released by the backend, 0 otherwise
 */
int DM_release(DerivedMesh *dm);

/* utility function to convert a DerivedMesh to a Mesh
 */
void DM_to_mesh(DerivedMesh *dm, struct Mesh *me, struct Object *ob);

struct BMEditMesh *DM_to_editbmesh(struct Object *ob, struct DerivedMesh *dm,
                                   struct BMEditMesh *existing, int do_tesselate);

/* conversion to bmesh only */
void          DM_to_bmesh_ex(struct DerivedMesh *dm, struct BMesh *bm);
struct BMesh *DM_to_bmesh(struct Object *ob, struct DerivedMesh *dm);


/* utility function to convert a DerivedMesh to a shape key block 
 */
void DM_to_meshkey(DerivedMesh *dm, struct Mesh *me, struct KeyBlock *kb);

/* set the CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask
 * will be copied
 */
void DM_set_only_copy(DerivedMesh *dm, CustomDataMask mask);

/* adds a vertex/edge/face custom data layer to a DerivedMesh, optionally
 * backed by an external data array
 * alloctype defines how the layer is allocated or copied, and how it is
 * freed, see BKE_customdata.h for the different options
 */
void DM_add_vert_layer(struct DerivedMesh *dm, int type, int alloctype,
                       void *layer);
void DM_add_edge_layer(struct DerivedMesh *dm, int type, int alloctype,
                       void *layer);
void DM_add_tessface_layer(struct DerivedMesh *dm, int type, int alloctype,
                           void *layer);
void DM_add_loop_layer(DerivedMesh *dm, int type, int alloctype,
                       void *layer);
void DM_add_poly_layer(struct DerivedMesh *dm, int type, int alloctype,
                       void *layer);

/* custom data access functions
 * return pointer to data from first layer which matches type
 * if they return NULL for valid indices, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_edge_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_tessface_data(struct DerivedMesh *dm, int index, int type);

/* custom data layer access functions
 * return pointer to first data layer which matches type (a flat array)
 * if they return NULL, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_edge_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_tessface_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_poly_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_loop_data_layer(struct DerivedMesh *dm, int type);

/* custom data setting functions
 * copy supplied data into first layer of type using layer's copy function
 * (deep copy if appropriate)
 */
void DM_set_vert_data(struct DerivedMesh *dm, int index, int type, void *data);
void DM_set_edge_data(struct DerivedMesh *dm, int index, int type, void *data);
void DM_set_tessface_data(struct DerivedMesh *dm, int index, int type, void *data);

/* custom data copy functions
 * copy count elements from source_index in source to dest_index in dest
 * these copy all layers for which the CD_FLAG_NOCOPY flag is not set
 */
void DM_copy_vert_data(struct DerivedMesh *source, struct DerivedMesh *dest,
					   int source_index, int dest_index, int count);
void DM_copy_edge_data(struct DerivedMesh *source, struct DerivedMesh *dest,
					   int source_index, int dest_index, int count);
void DM_copy_tessface_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                       int source_index, int dest_index, int count);
void DM_copy_loop_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                       int source_index, int dest_index, int count);
void DM_copy_poly_data(struct DerivedMesh *source, struct DerivedMesh *dest,
					   int source_index, int dest_index, int count);

/* custom data free functions
 * free count elements, starting at index
 * they free all layers for which the CD_FLAG_NOCOPY flag is not set
 */
void DM_free_vert_data(struct DerivedMesh *dm, int index, int count);
void DM_free_edge_data(struct DerivedMesh *dm, int index, int count);
void DM_free_tessface_data(struct DerivedMesh *dm, int index, int count);
void DM_free_loop_data(struct DerivedMesh *dm, int index, int count);
void DM_free_poly_data(struct DerivedMesh *dm, int index, int count);

/*sets up mpolys for a DM based on face iterators in source*/
void DM_DupPolys(DerivedMesh *source, DerivedMesh *target);

void DM_ensure_tessface(DerivedMesh *dm);

/* interpolates vertex data from the vertices indexed by src_indices in the
 * source mesh using the given weights and stores the result in the vertex
 * indexed by dest_index in the dest mesh
 */
void DM_interp_vert_data(struct DerivedMesh *source, struct DerivedMesh *dest,
						 int *src_indices, float *weights,
						 int count, int dest_index);

/* interpolates edge data from the edges indexed by src_indices in the
 * source mesh using the given weights and stores the result in the edge indexed
 * by dest_index in the dest mesh.
 * if weights is NULL, all weights default to 1.
 * if vert_weights is non-NULL, any per-vertex edge data is interpolated using
 * vert_weights[i] multiplied by weights[i].
 */
typedef float EdgeVertWeight[SUB_ELEMS_EDGE][SUB_ELEMS_EDGE];
void DM_interp_edge_data(struct DerivedMesh *source, struct DerivedMesh *dest,
						 int *src_indices,
						 float *weights, EdgeVertWeight *vert_weights,
						 int count, int dest_index);

/* interpolates face data from the faces indexed by src_indices in the
 * source mesh using the given weights and stores the result in the face indexed
 * by dest_index in the dest mesh.
 * if weights is NULL, all weights default to 1.
 * if vert_weights is non-NULL, any per-vertex face data is interpolated using
 * vert_weights[i] multiplied by weights[i].
 */
typedef float FaceVertWeight[SUB_ELEMS_FACE][SUB_ELEMS_FACE];
void DM_interp_tessface_data(struct DerivedMesh *source, struct DerivedMesh *dest,
						 int *src_indices,
						 float *weights, FaceVertWeight *vert_weights,
						 int count, int dest_index);

void DM_swap_tessface_data(struct DerivedMesh *dm, int index, const int *corner_indices);

void DM_interp_loop_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                         int *src_indices,
                         float *weights, int count, int dest_index);

void DM_interp_poly_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                         int *src_indices,
                         float *weights, int count, int dest_index);

/* Temporary? A function to give a colorband to derivedmesh for vertexcolor ranges */
void vDM_ColorBand_store(struct ColorBand *coba);

/* Simple function to get me->totvert amount of vertices/normals,
 * correctly deformed and subsurfered. Needed especially when vertexgroups are involved.
 * In use now by vertex/weigt paint and particles */
float *mesh_get_mapped_verts_nors(struct Scene *scene, struct Object *ob);

	/* */
DerivedMesh *mesh_get_derived_final(struct Scene *scene, struct Object *ob,
									CustomDataMask dataMask);
DerivedMesh *mesh_get_derived_deform(struct Scene *scene, struct Object *ob,
									 CustomDataMask dataMask);

DerivedMesh *mesh_create_derived_for_modifier(struct Scene *scene, struct Object *ob,
											  struct ModifierData *md, int build_shapekey_layers);

DerivedMesh *mesh_create_derived_render(struct Scene *scene, struct Object *ob,
										CustomDataMask dataMask);

DerivedMesh *getEditDerivedBMesh(struct BMEditMesh *em, struct Object *ob,
                                           float (*vertexCos)[3]);

DerivedMesh *mesh_create_derived_index_render(struct Scene *scene, struct Object *ob, CustomDataMask dataMask, int index);

		/* same as above but wont use render settings */
DerivedMesh *mesh_create_derived(struct Mesh *me, struct Object *ob, float (*vertCos)[3]);
DerivedMesh *mesh_create_derived_view(struct Scene *scene, struct Object *ob,
									  CustomDataMask dataMask);
DerivedMesh *mesh_create_derived_no_deform(struct Scene *scene, struct Object *ob,
										   float (*vertCos)[3],
										   CustomDataMask dataMask);
DerivedMesh *mesh_create_derived_no_deform_render(struct Scene *scene, struct Object *ob,
												  float (*vertCos)[3],
												  CustomDataMask dataMask);
/* for gameengine */
DerivedMesh *mesh_create_derived_no_virtual(struct Scene *scene, struct Object *ob, float (*vertCos)[3],
											CustomDataMask dataMask);
DerivedMesh *mesh_create_derived_physics(struct Scene *scene, struct Object *ob, float (*vertCos)[3],
											CustomDataMask dataMask);

DerivedMesh *editbmesh_get_derived_base(struct Object *, struct BMEditMesh *em);
DerivedMesh *editbmesh_get_derived_cage(struct Scene *scene, struct Object *, 
									   struct BMEditMesh *em, CustomDataMask dataMask);
DerivedMesh *editbmesh_get_derived_cage_and_final(struct Scene *scene, struct Object *, 
						 struct BMEditMesh *em, DerivedMesh **final_r,
												 CustomDataMask dataMask);
float (*editbmesh_get_vertex_cos(struct BMEditMesh *em, int *numVerts_r))[3];
int editbmesh_modifier_is_enabled(struct Scene *scene, struct ModifierData *md, DerivedMesh *dm);
void makeDerivedMesh(struct Scene *scene, struct Object *ob, struct BMEditMesh *em, 
	CustomDataMask dataMask, int build_shapekey_layers);

/* returns an array of deform matrices for crazyspace correction, and the
 * number of modifiers left */
int editbmesh_get_first_deform_matrices(struct Scene *, struct Object *, struct BMEditMesh *em,
									   float (**deformmats)[3][3], float (**deformcos)[3]);

void weight_to_rgb(float r_rgb[3], const float weight);
/* Update the weight MCOL preview layer.
 * If weights are NULL, use object's active vgroup(s).
 * Else, weights must be an array of weight float values.
 *     If indices is NULL, it must be of numVerts length.
 *     Else, it must be of num length, as indices, which contains vertices' idx to apply weights to.
 *         (other vertices are assumed zero weight).
 */
void DM_update_weight_mcol(struct Object *ob, struct DerivedMesh *dm, int const draw_flag,
                           float *weights, int num, const int *indices);

/* convert layers requested by a GLSL material to actually available layers in
 * the DerivedMesh, with both a pointer for arrays and an offset for editmesh */
typedef struct DMVertexAttribs {
	struct {
		struct MTFace *array;
		int emOffset, glIndex, glTexco;
	} tface[MAX_MTFACE];

	struct {
		struct MCol *array;
		int emOffset, glIndex;
	} mcol[MAX_MCOL];

	struct {
		float (*array)[4];
		int emOffset, glIndex;
	} tang;

	struct {
		float (*array)[3];
		int emOffset, glIndex, glTexco;
	} orco;

	int tottface, totmcol, tottang, totorco;
} DMVertexAttribs;

void DM_vertex_attributes_from_gpu(DerivedMesh *dm,
	struct GPUVertexAttribs *gattribs, DMVertexAttribs *attribs);

void DM_add_tangent_layer(DerivedMesh *dm);
void DM_calc_auto_bump_scale(DerivedMesh *dm);

/* Set object's bounding box based on DerivedMesh min/max data */
void DM_set_object_boundbox(struct Object *ob, DerivedMesh *dm);

void DM_init_origspace(DerivedMesh *dm);

/* debug only */
#ifndef NDEBUG
char *DM_debug_info(DerivedMesh *dm);
void DM_debug_print(DerivedMesh *dm);
#endif

#endif
