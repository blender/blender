#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

/*local flag defines*/
#define DUPE_INPUT		1			/*input from operator*/
#define DUPE_NEW		2
#define DUPE_DONE		4

#ifndef SELECT
#define SELECT		1
#endif

/*
 *  COPY VERTEX
 *
 *   Copy an existing vertex from one bmesh to another.
 *
*/
static BMVert *copy_vertex(BMesh *source_mesh, BMVert *source_vertex, BMesh *target_mesh, GHash *vhash)
{
	BMVert *target_vertex = NULL;

	/*create a new vertex*/
	target_vertex = BM_Make_Vert(target_mesh, source_vertex->co,  NULL);

	/*insert new vertex into the vert hash*/
	BLI_ghash_insert(vhash, source_vertex, target_vertex);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata, source_vertex->data, &target_vertex->data);	

	/*Copy Flags*/
	if(source_vertex->head.flag & SELECT) BM_Select_Vert(target_mesh, target_vertex, 1);
	target_vertex->head.flag |= source_vertex->head.flag;

	BMO_SetFlag(target_mesh, (BMHeader*)target_vertex, DUPE_NEW);
	
	return target_vertex;
}

/*
 * COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 *
*/
static BMEdge *copy_edge(BMesh *source_mesh, BMEdge *source_edge, BMesh *target_mesh, GHash *vhash, GHash *ehash)
{
	BMEdge *target_edge = NULL;
	BMVert *target_vert1, *target_vert2;
	
	/*lookup v1 and v2*/
	target_vert1 = BLI_ghash_lookup(vhash, source_edge->v1);
	target_vert2 = BLI_ghash_lookup(vhash, source_edge->v2);
	
	/*create a new edge*/
	target_edge = BM_Make_Edge(target_mesh, target_vert1, target_vert2, NULL, 0);

	/*insert new edge into the edge hash*/
	BLI_ghash_insert(ehash, source_edge, target_edge);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/	
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata, source_edge->data, &target_edge->data);
	
	/*copy flags*/
	target_edge->head.flag = source_edge->head.flag;

	BMO_SetFlag(target_mesh, (BMHeader*)target_edge, DUPE_NEW);
	
	return target_edge;
}

/*
 * COPY FACE
 *
 *  Copy an existing face from one bmesh to another.
 *
*/
static BMFace *copy_face(BMesh *source_mesh, BMFace *source_face, BMesh *target_mesh, BMEdge **edar, GHash *vhash, GHash *ehash)
{
	BMVert *target_vert1, *target_vert2;
	BMLoop *source_loop, *target_loop;
	BMFace *target_face = NULL;
	BMIter iter, iter2;
	int i;
	
	/*lookup the first and second verts*/
	target_vert1 = BLI_ghash_lookup(vhash, source_face->loopbase->v);
	target_vert2 = BLI_ghash_lookup(vhash, source_face->loopbase->head.next);
	
	/*lookup edges*/
	for (i=0,source_loop=BMIter_New(source_mesh, &iter, BM_LOOPS_OF_FACE, source_face); 
		     source_loop; source_loop=BMIter_Step(&iter), i++) {
		edar[i] = BLI_ghash_lookup(ehash, source_loop->e);
	}
	
	/*create new face*/
	target_face = BM_Make_Ngon(target_mesh, target_vert1, target_vert2, edar, source_face->len, 0);	
	
	/*we copy custom data by hand, we cannot assume that customdata byte layout will be exactly the same....*/
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata, source_face->data, &target_face->data);	

	/*copy flags*/
	target_face->head.flags = source_face->head.flags;
	if(source_face->head.flag & SELECT) BM_Select_Face(target_mesh, target_face, 1);

	/*mark the face for output*/
	BMO_SetFlag(target_mesh, (BMHeader*)target_face, DUPE_NEW);
	
	/*copy per-loop custom data*/
	for (i=0,source_loop=BMIter_New(source_mesh, &iter, BM_LOOPS_OF_FACE, source_face),
	  target_loop=BMIter_New(target_mesh, &iter2, BM_LOOPS_OF_FACE, target_face);
	  source_loop && target_loop; source_loop=BMIter_Step(&iter), target_loop=BMIter_Step(&iter2),
	  i++) {
		CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata, source_loop->data, &target_loop->data);		
	}	
	return target_face;
}
	/*
 * COPY MESH
 *
 * Internal Copy function.
*/

static void copy_mesh(BMesh *source, BMesh *target)
{

	BMVert *v = NULL;
	BMEdge *e = NULL, **edar = NULL;
	BMLoop *l = NULL;
	BMFace *f = NULL;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	GHash *vhash;
	GHash *ehash;
	
	int maxlength = 0;

	/*initialize pointer hashes*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp); 
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	/*initialize edge pointer array*/
	for(f = BMIter_New(&faces, source, BM_FACES, source); f; f = BMIter_Step(&faces)){
		if(f->len > maxlength) maxlength = f->len;
	}
	edar = MEM_callocN(sizeof(BMEdge*) * maxlength, "BM copy mesh edge pointer array");
	

	/*first we dupe all flagged faces and their elements from source*/
	for(f = BMIter_New(&faces, source, BM_FACES, source); f; f= BMIter_Step(&faces)){
		if(BMO_TestFlag(source, (BMHeader*)f, DUPE_INPUT)){
			/*vertex pass*/
			for(v = BMIter_New(&verts, source, BM_VERTS_OF_FACE, f); v; v = BMIter_Step(&verts)){
				if(!BMO_TestFlag(source, (BMHeader*)v, DUPE_DONE)){ 
					copy_vertex(source,v, target, vhash);
					BMO_SetFlag(source, (BMHeader*)v, DUPE_DONE);
				}
			}

			/*edge pass*/
			for(e = BMIter_New(&edges, source, BM_EDGES_OF_FACE, f); e; e = BMIter_Step(&edges)){
				if(!BMO_TestFlag(source, (BMHeader*)e, DUPE_DONE)){
					copy_edge(source, e, target,  vhash,  ehash);
					BMO_SetFlag(source, (BMHeader*)e, DUPE_DONE);
				}
			}
			copy_face(source, f, target, edar, vhash, ehash);
			BMO_SetFlag(source, (BMHeader*)f, DUPE_DONE);
		}
	}
	
	/*now we dupe all the edges*/
	for(e = BMIter_New(&edges, source, BM_EDGES, source); e; e = BMIter_Step(&edges)){
		if(BMO_TestFlag(source, (BMHeader*)e, DUPE_INPUT) && (!BMO_TestFlag(source, (BMHeader*)e, DUPE_DONE))){
			/*make sure that verts are copied*/
			if(!BMO_TestFlag(source, (BMHeader*)e->v1, DUPE_DONE)) {
				copy_vertex(source, e->v1, target, vhash);
				BMO_SetFlag(source, (BMHeader*)e->v1, DUPE_DONE);
			}
			if(!BMO_TestFlag(source, (BMHeader*)e->v2, DUPE_DONE)) {
				copy_vertex(source, e->v2, target, vhash);
				BMO_SetFlag(source, (BMHeader*)e->v2, DUPE_DONE);
			}
			/*now copy the actual edge*/
			copy_edge(source, e, target,  vhash,  ehash);			
			BMO_SetFlag(source, (BMHeader*)e, DUPE_DONE); 
		}
	}
	
	/*finally dupe all loose vertices*/
	for(v = BMIter_New(&verts, source, BM_VERTS, source); v; v = BMIter_Step(&verts)){
		if(BMO_TestFlag(source, (BMHeader*)v, DUPE_INPUT) && (!BMO_TestFlag(source, (BMHeader*)v, DUPE_DONE))){
			copy_vertex(source, v, target, vhash);
			BMO_SetFlag(source, (BMHeader*)v, DUPE_DONE);
		}
	}

	/*free pointer hashes*/
	BLI_ghash_free(vhash, NULL, NULL);
	BLI_ghash_free(ehash, NULL, NULL);	

	/*free edge pointer array*/
	if(edar)
		MEM_freeN(edar);
}
/*
BMesh *bmesh_make_mesh_from_mesh(BMesh *bm, int allocsize[4])
{
	BMesh *target = NULL;
	target = bmesh_make_mesh(allocsize);
	

	CustomData_copy(&bm->vdata, &target->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->edata, &target->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->ldata, &target->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->pdata, &target->pdata, CD_MASK_BMESH, CD_CALLOC, 0);
	

	CustomData_bmesh_init_pool(&target->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&target->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&target->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&target->pdata, allocsize[3]);
	
	bmesh_begin_edit(bm);
	bmesh_begin_edit(target);
	
	bmesh_copy_mesh(bm, target, 0);

	bmesh_end_edit(bm);
	bmesh_end_edit(target);
	
	return target;

}
*/

/*
 * Duplicate Operator
 *
 * Duplicates verts, edges and faces of a mesh.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be duplicated
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be duplicated
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be duplicated
 *
 * OUTPUT SLOTS:
 * 
 * BMOP_DUPE_VORIGINAL: Buffer containing pointers to the original mesh vertices
 * BMOP_DUPE_EORIGINAL: Buffer containing pointers to the original mesh edges
 * BMOP_DUPE_FORIGINAL: Buffer containing pointers to the original mesh faces
 * BMOP_DUPE_VNEW: Buffer containing pointers to the new mesh vertices
 * BMOP_DUPE_ENEW: Buffer containing pointers to the new mesh edges
 * BMOP_DUPE_FNEW: Buffer containing pointers to the new mesh faces
 *
*/

void dupeop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *dupeop = op;
	BMOpSlot *vinput, *einput, *finput;
	
	vinput = BMO_GetSlot(dupeop, BMOP_DUPE_VINPUT);
	einput = BMO_GetSlot(dupeop, BMOP_DUPE_EINPUT);
	finput = BMO_GetSlot(dupeop, BMOP_DUPE_FINPUT);

	/*go through vinput, einput, and finput and flag elements with private flags*/
	BMO_Flag_Buffer(bm, dupeop, BMOP_DUPE_VINPUT, DUPE_INPUT);
	BMO_Flag_Buffer(bm, dupeop, BMOP_DUPE_EINPUT, DUPE_INPUT);
	BMO_Flag_Buffer(bm, dupeop, BMOP_DUPE_FINPUT, DUPE_INPUT);

	/*use the internal copy function*/
	copy_mesh(bm, bm);
	
	/*Output*/
	/*First copy the input buffers to output buffers - original data*/
	BMO_CopySlot(dupeop, dupeop, vinput->index, BMOP_DUPE_VORIGINAL);
	BMO_CopySlot(dupeop, dupeop, einput->index, BMOP_DUPE_EORIGINAL);
	BMO_CopySlot(dupeop, dupeop, finput->index, BMOP_DUPE_FORIGINAL);

	/*Now alloc the new output buffers*/
	BMO_Flag_To_Slot(bm, dupeop, BMOP_DUPE_VNEW, DUPE_NEW, BMESH_VERT);
	BMO_Flag_To_Slot(bm, dupeop, BMOP_DUPE_ENEW, DUPE_NEW, BMESH_EDGE);
	BMO_Flag_To_Slot(bm, dupeop, BMOP_DUPE_FNEW, DUPE_NEW, BMESH_FACE);
}


/*
 * Split Operator
 *
 * Duplicates verts, edges and faces of a mesh but also deletes the originals.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be split
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be split
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be split
 *
 * OUTPUT SLOTS:
 * 
 * BMOP_DUPE_VOUTPUT: Buffer containing pointers to the split mesh vertices
 * BMOP_DUPE_EOUTPUT: Buffer containing pointers to the split mesh edges
 * BMOP_DUPE_FOUTPUT: Buffer containing pointers to the split mesh faces
 *
*/

void splitop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *splitop = op;
	BMOperator dupeop;
	BMOperator delop;

	/*initialize our sub-operators*/
	BMO_Init_Op(&dupeop, BMOP_DUPE);
	BMO_Init_Op(&delop, BMOP_DEL);

	BMO_CopySlot(&dupeop, &delop, BMOP_SPLIT_VINPUT, BMOP_DUPE_VINPUT);
	BMO_CopySlot(&dupeop, &delop, BMOP_SPLIT_EINPUT, BMOP_DUPE_EINPUT);
	BMO_CopySlot(&dupeop, &delop, BMOP_SPLIT_FINPUT, BMOP_DUPE_FINPUT);

	BMO_Exec_Op(bm, &dupeop);

	/*connect outputs of dupe to delete*/
	BMO_CopySlot(&dupeop, &delop, BMOP_DUPE_VORIGINAL, BMOP_DEL_VINPUT);
	BMO_CopySlot(&dupeop, &delop, BMOP_DUPE_EORIGINAL, BMOP_DEL_EINPUT);
	BMO_CopySlot(&dupeop, &delop, BMOP_DUPE_FORIGINAL, BMOP_DEL_FINPUT);

	BMO_Exec_Op(bm, &delop);

	/*now we make our outputs by copying the dupe outputs*/
	BMO_CopySlot(&dupeop, splitop, BMOP_DUPE_VNEW, BMOP_SPLIT_VOUTPUT);
	BMO_CopySlot(&dupeop, splitop, BMOP_DUPE_ENEW, BMOP_SPLIT_EOUTPUT);
	BMO_CopySlot(&dupeop, splitop, BMOP_DUPE_FNEW, BMOP_SPLIT_FOUTPUT);

	/*cleanup*/
	BMO_Finish_Op(bm, &dupeop);
	BMO_Finish_Op(bm, &delop);
}

#define DEL_INPUT		1
#define DEL_WIREVERT	2

void delop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *delop = op;

	/*Mark Buffers*/
	BMO_Flag_Buffer(bm, delop, BMOP_DEL_VINPUT, DEL_INPUT);
	BMO_Flag_Buffer(bm, delop, BMOP_DEL_EINPUT, DEL_INPUT);
	BMO_Flag_Buffer(bm, delop, BMOP_DEL_FINPUT, DEL_INPUT);

}
static void delete_verts(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMLoop *f;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	for(v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts)){
		if(BMO_TestFlag(bm, (BMHeader*)v, DEL_INPUT)) {
			/*Visit edges*/
			for(e = BMIter_New(&edges, bm, BM_EDGES_OF_VERT, v); e; e = BMIter_Step(&edges))
				BMO_SetFlag(bm, (BMHeader*)e, DEL_INPUT);
			/*Visit faces*/
			for(f = BMIter_New(&faces, bm, BM_FACES_OF_VERT, v); f; f = BMIter_Step(&faces))
				BMO_SetFlag(bm, (BMHeader*)f, DEL_INPUT);
		}
	}

	remove_tagged_faces(bm, DEL_INPUT);
	remove_tagged_edges(bm, DEL_INPUT);
	remove_tagged_verts(bm, DEL_INPUT);
}

static void delete_edges(BMesh *bm){
	BMEdge *e;
	BMFace *f;
	
	BMIter edges;
	BMIter faces;

	for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges)){
		if(BMO_TestFlag(bm, (BMHeader*)e, DEL_INPUT)) {
			for(f = BMIter_New(&faces, bm, BM_FACES_OF_EDGE, e); f; f = BMIter_Step(&faces)){
					BMO_SetFlag(bm, (BMHeader*)f, DEL_INPUT);
			}
		}
	}
	remove_tagged_faces(bm, DEL_INPUT);
	remove_tagged_edges(bm, DEL_INPUT);
}

/*you need to make remove tagged verts/edges/faces
	api functions that take a filter callback.....
	and this new filter type will be for opstack flags.
	This is because the remove_taggedXXX functions bypass iterator API.
		 -Ops dont care about 'UI' considerations like selection state, hide state, ect. If you want to work on unhidden selections for instance,
		 copy output from a 'select context' operator to another operator....
*/

/*Break this into smaller functions*/

#define DEL_VERTS		1
#define DEL_EDGES		2
#define DEL_ONLYFACES	3
#define DEL_EDGESFACES	4
#define DEL_FACES		5
#define DEL_ALL			6

static void delete_context(BMesh *bm, int type){
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	if(type == DEL_VERTS) delete_verts(bm);
	else if(type == DEL_EDGES){
		/*flush down to verts*/
		for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges)){
			if(BMO_TestFlag(bm, (BMHeader*)e, DEL_INPUT)){
				BMO_SetFlag(bm, (BMHeader*)(e->v1), DEL_INPUT);
				BMO_SetFlag(bm, (BMHeader*)(e->v2), DEL_INPUT);
			}
		}
		delete_edges(bm);
		/*remove loose vertices*/
		for(v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts)){
			if(BMO_TestFlag(bm, (BMHeader*)v, DEL_INPUT) && (!(v->edge)))
				BMO_SetFlag(bm, (BMHeader*)v, DEL_WIREVERT);
		}
		remove_tagged_verts(bm, DEL_WIREVERT);
	}
	else if(type == DEL_EDGESFACES) delete_edges(bm);
	else if(type == DEL_ONLYFACES) remove_tagged_faces(bm, DEL_INPUT);
	else if(type == DEL_FACES){
		/*go through and mark all edges and all verts of all faces for delete*/
		for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces)){
			if(BMO_TestFlag(bm, (BMHeader*)f, DEL_INPUT)){
				for(e = BMIter_New(&edges, bm, BM_EDGES_OF_FACE, f); e; e = BMIter_Step(&edges))
					BMO_SetFlag(bm, (BMHeader*)e, DEL_INPUT);
				for(v = BMIter_New(&verts, bm, BM_VERTS_OF_FACE, v); v; v = BMIter_Step(&verts))
					BMO_SetFlag(bm, (BMHeader*)v, DEL_INPUT);
			}
		}
		/*now go through and mark all remaining faces all edges for keeping.*/
		for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces)){
			if(!BMO_TestFlag(bm, (BMHeader*)f, DEL_INPUT)){
				for(e = BMIter_New(&edges, bm, BM_EDGES_OF_FACE, f); e; e= BMIter_Step(&edges))
					BMO_ClearFlag(bm, (BMHeader*)e, DEL_INPUT);
				for(v = BMIter_New(&verts, bm, BM_VERTS_OF_FACE, v); v; v= BMIter_Step(&verts))
					BMO_ClearFlag(bm, (BMHeader*)v, DEL_INPUT);
			}
		}
		/*now delete marked faces*/
		remove_tagged_faces(bm, DEL_INPUT);
		/*delete marked edges*/
		remove_tagged_edges(bm, DEL_INPUT);
		/*remove loose vertices*/
		remove_tagged_verts(bm, DEL_INPUT);
	}
	/*does this option even belong in here?*/
	else if(type == DEL_ALL){
		for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces))
			BMO_SetFlag(bm, (BMHeader*)f, DEL_INPUT);
		for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges))
			BMO_SetFlag(bm, (BMHeader*)e, DEL_INPUT);
		for(v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts))
			BMO_SetFlag(bm, (BMHeader*)v, DEL_INPUT);
		
		remove_tagged_faces(bm, DEL_INPUT);
		remove_tagged_edges(bm, DEL_INPUT);
		remove_tagged_verts(bm, DEL_INPUT);
	}
}