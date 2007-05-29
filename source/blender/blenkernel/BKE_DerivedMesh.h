/**
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BKE_DERIVEDMESH_H
#define BKE_DERIVEDMESH_H

/* TODO (Probably)
 *
 *  o Make drawMapped* functions take a predicate function that
 *    determines whether to draw the edge (this predicate can
 *    also set color, etc). This will be slightly more general 
 *    and allow some of the functions to be collapsed.
 *  o Once accessor functions are added then single element draw
 *    functions can be implemented using primitive accessors.
 *  o Add function to dispatch to renderer instead of using
 *    conversion to DLM.
 */

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"

struct MVert;
struct MEdge;
struct MFace; /* EVIL! */
struct MTFace; /* EVIL! */
struct MPoly;
struct MLoop;
struct Object;
struct Mesh;
struct EditMesh;
struct ModifierData;
struct MCol;
struct Material;
struct MemArena;

/* number of sub-elements each mesh element has (for interpolation) */
#define SUB_ELEMS_VERT 0
#define SUB_ELEMS_EDGE 2
#define SUB_ELEMS_FACE 4

/*Only accepts triangles!*/
typedef struct bglCacheDrawInterface {
	/*this must be called first!*/
	void (*setMaterials)(void *vself, int totmat, struct Material **materials);
	void (*beginCache)(void *vself);
	
	/*highcols are for drawing transparent highlight faces.*/
	void (*addTriangle)(void *vself, float verts[][3], float normals[][3], char cols[][3],
	                              char highcols[4], int mat);
	                              
	/*if index > 0, then this edge wire is included in backbuffered select draw.*/
	void (*addEdgeWire)(void *vself, float v1[3], float v2[3], char c1[3], char c2[3], int index);
	void (*addVertPoint)(void *vself, float v[3], char col[3]);
	void (*addFacePoint)(void *vself, float v[3], char col[3]);
	void (*endCache)(void *vself);
	
	void (*drawFacesSolid)(void *vself, int usecolors);
	void (*drawFacesTransp)(void *vself);
	void (*drawVertPoints)(void *vself, float alpha, float size);
	void (*drawWireEdges)(void *vself, float alpha);
	void (*drawFacePoints)(void *vself, float alpha, float size);

	void (*drawFacesBackSel)(void *vself, int offset);

	/*returns start offset to be used with faces*/
	int (*drawEdgesBackSel)(void *vself, int offset);

	/*returns start offset to be used with edges*/
	int (*drawVertsBackSel)(void *vself);

	/*NOTE: does not free the struct pointed to at vself! just direct data*/
	void (*release)(void *vself);
} bglCacheDrawInterface;

/** Interal data structure for bglCacheMesh, should not be accessed directly.
    Note that all this data is converted to opengl arrays and freed on cache->endCache()
 **/
typedef struct bglTriangle {
	struct bglTriangle *next, *prev;
	float uv[3][2];
	unsigned char colors[3][3];
	float cos[3][3];
	float nos[3][3];
	unsigned char highlightclr[4];
	int mat;
} bglTriangle;

typedef struct bglEdgeWire {
	struct bglEdgeWire *next, *pref;
	float v1[3], v2[3];
	char c1[3], c2[3];
	int index; /*backbuffered select index*/
} bglEdgeWire;

typedef struct bglVertPoint {
	struct bglVertPoint *next, *prev;
	float co[3];
	char col[3];
} bglVertPoint;

/*theres always one group per material, that is
  entirely triangles.*/
typedef struct bglCacheFaceGroup {
	float *faceverts;
	char *facecolors;
	char *highcolors; /*highlight colors for editmode*/
	float *facenormals;
	int tottris;
} bglCacheFaceGroup;

#define MAX_FACEGROUP	16 /*actually a copy of the maximum number of materials, which is 16*/

typedef struct bglCacheMesh {
	bglCacheDrawInterface cinterface;
	
	/*temporary stuff used until ->endCache() is called:*/
	ListBase triangles[MAX_FACEGROUP], wires, points, fpoints;
	int tottri, totwire, totpoint, totfpoint;
	struct MemArena *arena;
	struct MemArena *gl_arena; /*this holds only opengl arrays*/

	struct Material **mats;

	/*initilzed is set to 1 by setmaterials and 2 by begincache, 
	  then 3 by endcache (e.g. ready to draw).*/
	int  totmat, initilized; 

	/*actual opengl array stuff:*/
	bglCacheFaceGroup facegroups[MAX_FACEGROUP];
	float *wireverts;
	char *wirecols;
	char *wire_selcols; /*selection colors*/

	float *pointverts;
	char *pointcols;
	char *point_selcols; /*backbuffered select colors for vert dots.  this is auto-generated.*/

	float *fpointverts;
	char *fpointcols;
	char *fpoint_selcols; /*backbuffered select colors for face dots. this is auto-generated.*/
} bglCacheMesh;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
	/* Private DerivedMesh data, only for internal DerivedMesh use */
	
	/*faceData is stored tesselated faces only for updated DerivedMeshes.*/
	CustomData vertData, edgeData, faceData, loopData, polyData;
	int numVertData, numEdgeData, numFaceData, numLoopData, numPolyData;
	int needsFree; /* checked on ->release, is set to 0 for cached results */
	int needsDrawCacheUpdate;
	int backbuf_wireoff, backbuf_faceoff;
	
	/* Misc. Queries */

	/* Also called in Editmode */
	int (*getNumVerts)(DerivedMesh *dm);
	/* Also called in Editmode */
	int (*getNumEdges)(DerivedMesh *dm);
	int (*getNumFaces)(DerivedMesh *dm);
	int (*getNumLoops)(DerivedMesh *dm);
	int (*getNumPolys)(DerivedMesh *dm);
	
	/* copy a single vert/edge/tesselated face/face ngon from the derived mesh into
	 * *{vert/edge/face}_r. note that the current implementation
	 * of this function can be quite slow, iterating over all
	 * elements (editmesh, verse mesh)
	 */
	void (*getVert)(DerivedMesh *dm, int index, struct MVert *vert_r);
	void (*getEdge)(DerivedMesh *dm, int index, struct MEdge *edge_r);
	void (*getFace)(DerivedMesh *dm, int index, struct MFace *face_r);
	void (*getLoop)(DerivedMesh *dm, int index, struct MFace *face_r);
	void (*getPoly)(DerivedMesh *dm, int index, struct MPoly *face_r);
	
	/* return a pointer to the entire array of verts/edges/face from the
	 * derived mesh. if such an array does not exist yet, it will be created,
	 * and freed on the next ->release(). consider using getVert/Edge/Face if
	 * you are only interested in a few verts/edges/faces.
	 */
	struct MVert *(*getVertArray)(DerivedMesh *dm);
	struct MEdge *(*getEdgeArray)(DerivedMesh *dm);
	struct MFace *(*getFaceArray)(DerivedMesh *dm);
	struct MLoop *(*getLoopArray)(DerivedMesh *dm);
	struct MPoly *(*getPolyArray)(DerivedMesh *dm);
	
	/* copy all verts/edges/tesselated faces/polys from the derived mesh into
	 * *{vert/edge/face}_r (must point to a buffer large enough)
	 */
	void (*copyVertArray)(DerivedMesh *dm, struct MVert *vert_r);
	void (*copyEdgeArray)(DerivedMesh *dm, struct MEdge *edge_r);
	void (*copyFaceArray)(DerivedMesh *dm, struct MFace *face_r);
	void (*copyLoopArray)(DerivedMesh *dm, struct MLoop *loop_r);
	void (*copyPolyArray)(DerivedMesh *dm, struct MPoly *poly_r);
	
	/* return a copy of all verts/edges/tesselated faces/polys from the derived mesh
	 * it is the caller's responsibility to free the returned pointer with MEM_freeN(pointer).
	 */
	struct MVert *(*dupVertArray)(DerivedMesh *dm);
	struct MEdge *(*dupEdgeArray)(DerivedMesh *dm);
	struct MFace *(*dupFaceArray)(DerivedMesh *dm);
	struct MLoop *(*dupLoopArray)(DerivedMesh *dm);
	struct MPoly *(*dupPolyArray)(DerivedMesh *dm);
	
	/* return a pointer to a single element of vert/edge/tesselated faces/polys custom data
	 * from the derived mesh (this gives a pointer to the actual data, not
	 * a copy)
	 */
	void *(*getVertData)(DerivedMesh *dm, int index, int type);
	void *(*getEdgeData)(DerivedMesh *dm, int index, int type);
	void *(*getFaceData)(DerivedMesh *dm, int index, int type);
	void *(*getLoopData)(DerivedMesh *dm, int index, int type);
	void *(*getPolyData)(DerivedMesh *dm, int index, int type);
	
	/* return a pointer to the entire array of vert/edge/tesselated faces/polys custom data
	 * from the derived mesh (this gives a pointer to the actual data, not
	 * a copy)
	 */
	void *(*getVertDataArray)(DerivedMesh *dm, int type);
	void *(*getEdgeDataArray)(DerivedMesh *dm, int type);
	void *(*getFaceDataArray)(DerivedMesh *dm, int type);
	void *(*getLoopDataArray)(DerivedMesh *dm, int type);
	void *(*getPolyDataArray)(DerivedMesh *dm, int type);
	 
	/* DEPRECATED FOR DIRECT DRAWING: Iterate over each mapped vertex in the derived mesh, calling the
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

	/* Get vertex normal, undefined if index is not valid */
	void (*getVertNo)(DerivedMesh *dm, int index, float no_r[3]);

	/* Create opengl arrays with the new drawing API*/
	void (*UpdateDrawCache)(DerivedMesh *dm, bglCacheDrawInterface *interface);
	void (*setOverrideVerts)(DerivedMesh *dm, float *cos, float *nos);
	
	/* Drawing Operations -- These should use the cache opengl functions.*/

	/* Draw all vertices as bgl points (no options) */
	void (*drawVerts)(DerivedMesh *dm);

	/* Draw edit verts. 
	Note that the dm->backbuf_wireoff/faceoff will be used.*/
	void (*drawEditVerts)(DerivedMesh *dm, float alpha);

	/* Draw edit verts.*/
	int (*drawEditVertsBackbuffer)(DerivedMesh *dm);

	/* Draw edit face points.  */
	void (*drawEditFacePoints)(DerivedMesh *dm, float alpha);

	/* Draw edit face points for backbuffer select.  
	Note that the dm->backbuf_wireoff/faceoff will be used.*/
	void (*drawEditFacePointsBackbuffer)(DerivedMesh *dm);


	/* Draw edges in the UV mesh (if exists) */
	void (*drawUVEdges)(DerivedMesh *dm);

	/* Draw all edges as lines (no options) 
	 *
	 * Also called for *final* editmode DerivedMeshes
	 */
	void (*drawEdges)(DerivedMesh *dm, int drawLooseEdges);
	
	/* Draw all loose edges (edges w/ no adjoining faces) */
	void (*drawLooseEdges)(DerivedMesh *dm);

	/* Draw all faces
	 *  These are deprecated, use the drawPoly*** functions instead.
	 *  o Set face normal or vertex normal based on inherited face flag
	 *  o Use inherited face material index to call setMaterial
	 *  o Only if setMaterial returns true
	 *
	 * Also called for *final* editmode DerivedMeshes
	 */
	void (*drawFacesSolid)(DerivedMesh *dm, int (*setMaterial)(int));

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
	                     struct MCol *mcol, int matnr));


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
	                        void *userData, int useColors);

	/* Draw mapped faces using MTFace 
	 *  o Drawing options too complicated to enumerate, look at code.
	 */
	void (*drawMappedFacesTex)(DerivedMesh *dm,
	                           int (*setDrawOptions)(void *userData,
	                                                 int index),
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

	/* Release reference to the DerivedMesh. This function decides internally
	 * if the DerivedMesh will be freed, or cached for later use. */
	void (*release)(DerivedMesh *dm);
};

/*create a new cache drawing interface*/
bglCacheDrawInterface *bglCacheNew(void);

/* utility function to initialise a DerivedMesh's function pointers to
 * the default implementation (for those functions which have a default)
 */
void DM_init_funcs(DerivedMesh *dm);

/* utility function to initialise a DerivedMesh for the desired number
 * of vertices, edges and faces (doesn't allocate memory for them, just
 * sets up the custom data layers)
 */
void DM_init(DerivedMesh *dm, int numVerts, int numEdges, int numFaces, int numLoops, int numPolys);

/* utility function to initialise a DerivedMesh for the desired number
 * of vertices, edges and faces, with a layer setup copied from source
 */
void DM_from_template(DerivedMesh *dm, DerivedMesh *source,
                      int numVerts, int numEdges, int numFaces, int numLoops, int numPolys);

/* utility function to release a DerivedMesh's layers
 * returns 1 if DerivedMesh has to be released by the backend, 0 otherwise
 */
int DM_release(DerivedMesh *dm);

/* utility function to convert a DerivedMesh to a Mesh
 */
void DM_to_mesh(DerivedMesh *dm, struct Mesh *me);

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
void DM_add_face_layer(struct DerivedMesh *dm, int type, int alloctype,
                       void *layer);

/* custom data access functions
 * return pointer to data from first layer which matches type
 * if they return NULL for valid indices, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_edge_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_face_data(struct DerivedMesh *dm, int index, int type);

/* custom data layer access functions
 * return pointer to first data layer which matches type (a flat array)
 * if they return NULL, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_edge_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_face_data_layer(struct DerivedMesh *dm, int type);

/* custom data setting functions
 * copy supplied data into first layer of type using layer's copy function
 * (deep copy if appropriate)
 */
void DM_set_vert_data(struct DerivedMesh *dm, int index, int type, void *data);
void DM_set_edge_data(struct DerivedMesh *dm, int index, int type, void *data);
void DM_set_face_data(struct DerivedMesh *dm, int index, int type, void *data);

/* custom data copy functions
 * copy count elements from source_index in source to dest_index in dest
 * these copy all layers for which the CD_FLAG_NOCOPY flag is not set
 */
void DM_copy_vert_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                       int source_index, int dest_index, int count);
void DM_copy_edge_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                       int source_index, int dest_index, int count);
void DM_copy_face_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                       int source_index, int dest_index, int count);

/* custom data free functions
 * free count elements, starting at index
 * they free all layers for which the CD_FLAG_NOCOPY flag is not set
 */
void DM_free_vert_data(struct DerivedMesh *dm, int index, int count);
void DM_free_edge_data(struct DerivedMesh *dm, int index, int count);
void DM_free_face_data(struct DerivedMesh *dm, int index, int count);

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
void DM_interp_face_data(struct DerivedMesh *source, struct DerivedMesh *dest,
                         int *src_indices,
                         float *weights, FaceVertWeight *vert_weights,
                         int count, int dest_index);

void DM_swap_face_data(struct DerivedMesh *dm, int index, int *corner_indices);

/* Simple function to get me->totvert amount of vertices/normals,
   correctly deformed and subsurfered. Needed especially when vertexgroups are involved.
   In use now by vertex/weigt paint and particles */
float *mesh_get_mapped_verts_nors(struct Object *ob);

	/* */
DerivedMesh *mesh_get_derived_final(struct Object *ob,
                                    CustomDataMask dataMask);
DerivedMesh *mesh_get_derived_deform(struct Object *ob,
                                     CustomDataMask dataMask);

DerivedMesh *mesh_create_derived_for_modifier(struct Object *ob, struct ModifierData *md);

DerivedMesh *mesh_create_derived_render(struct Object *ob,
                                        CustomDataMask dataMask);
/* same as above but wont use render settings */
DerivedMesh *mesh_create_derived_view(struct Object *ob,
                                      CustomDataMask dataMask);
DerivedMesh *mesh_create_derived_no_deform(struct Object *ob,
                                           float (*vertCos)[3],
                                           CustomDataMask dataMask);
DerivedMesh *mesh_create_derived_no_deform_render(struct Object *ob,
                                                  float (*vertCos)[3],
                                                  CustomDataMask dataMask);

DerivedMesh *editmesh_get_derived_base(void);
DerivedMesh *editmesh_get_derived_cage(CustomDataMask dataMask);
DerivedMesh *editmesh_get_derived_cage_and_final(DerivedMesh **final_r,
                                                 CustomDataMask dataMask);

void weight_to_rgb(float input, float *fr, float *fg, float *fb);

/* determines required DerivedMesh data according to view and edit modes */
CustomDataMask get_viewedit_datamask();

#endif

