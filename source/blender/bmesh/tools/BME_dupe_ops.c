#if 0

/*
 *  BME_DUPLICATE.C
 *
 *  This file contains functions for duplicating, copying, and splitting
 *  elements from a bmesh.
 *
 */

/*
 *  COPY VERTEX
 *
 *   Copy an existing vertex from one bmesh to another.
 *
*/

static BMVert *copy_vertex(BMMesh *source_mesh, BMVert *source_vertex, BMMesh *target_mesh, GHash *vhash)
{
	BMVert *target_vertex = NULL;

	/*create a new vertex*/
	target_vertex = BM_vert_create(target, source_vertex->co,  NULL);

	/*insert new vertex into the vert hash*/
	BLI_ghash_insert(vhash, source_vertex, target_vertex);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata, source_vertex->data, &target_vertex->data);	

	/*Copy Markings*/
	if(BM_Is_Selected((BMHeader*)source_vertex)) BM_vert_select_set(target_mesh, target_vertex, TRUE);
	if(BM_Is_Hidden((BMHeader*)source_vertex)) BM_Mark_Hidden((BMHeader*)target_vertex, 1);

	BMO_elem_flag_enable(target_mesh, (BMHeader*)target_vertex, DUPE_NEW);
	
	return target_vertex;
}

/*
 * COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 *
*/

static BMEdge *copy_edge(BMMesh *source_mesh, BMEdge *source_edge, BMMesh *target_mesh, GHash *vhash, GHash *ehash)
{
	BMEdge *target_edge = NULL;
	BMVert *target_vert1, *target_vert2;
	
	/*lookup v1 and v2*/
	target_vert1 = BLI_ghash_lookup(vhash, source_edge->v1);
	target_vert2 = BLI_ghash_lookup(vhash, source_edge->v2);
	
	/*create a new edge*/
	target_edge = BM_edge_create(target_mesh, target_vert1, target_vert2, NULL, FALSE);

	/*insert new edge into the edge hash*/
	BLI_ghash_insert(ehash, source_edge, target_edge);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/	
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata, source_edge->data, &target_edge->data);
	
	/*copy flags*/
	if(BM_Is_Selected((BMHeader*) source_edge)) BM_edge_select_set(target_mesh, target_edge, TRUE);
	if(BM_Is_Hidden((BMHeader*) source_edge)) BM_Mark_Hidden(target_mesh, target_edge, 1);
	if(BM_Is_Sharp((BMHeader*) source_edge)) BM_Mark_Sharp(target_edge, 1);
	if(BM_Is_Seam((BMHeader*) source_edge)) BM_Mark_Seam(target_edge, 1);
	if(BM_Is_Fgon((BMHeader*) source_edge)) BM_Mark_Fgon(target_edge, 1);

	BMO_elem_flag_enable(target_mesh, (BMHeader*)target_edge, DUPE_NEW);
	
	return target_edge;
}

/*
 * COPY FACE
 *
 *  Copy an existing face from one bmesh to another.
 *
*/

static BMFace *copy_face(BMMesh *source_mesh, BMFace *source_face, BMMesh *target_mesh, BMEdge **edar, GHash *verthash, GHash *ehash)
{
	BMEdge  *target_edge;
	BMVert *target_vert1, *target_vert2;
	BMLoop *source_loop, *target_loop;
	BMFace *target_face = NULL;
	int i;
	
	/*lookup the first and second verts*/
	target_vert1 = BLI_ghash_lookup(vhash, source_face->lbase->v);
	target_vert2 = BLI_ghash_lookup(vhash, source_face->lbase->next->v);
	
	/*lookup edges*/
	i = 0;
	source_loop = source_face->lbase;
	do{
		edar[i] = BLI_ghash_lookup(ehash, source_loop->e);
		i++;
		source_loop = source_loop->next;
	}while(source_loop != source_face->lbase);
	
	/*create new face*/
	target_face = BM_face_create_ngon(target_mesh, target_vert1, target_vert2, edar, source_face->len, 0);	
	
	/*we copy custom data by hand, we cannot assume that customdata byte layout will be exactly the same....*/
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata, source_face->data, &target_face->data);	

	/*copy flags*/
	if(BM_Is_Selected((BMHeader*)source_face)) BM_Select_face(target, target_face, TRUE);
	if(BM_Is_Hidden((BMHeader*)source_face)) BM_Mark_Hidden((BMHeader*)target_face, 1);

	/*mark the face for output*/
	BMO_elem_flag_enable(target_mesh, (BMHeader*)target_face, DUPE_NEW);
	
	/*copy per-loop custom data*/
	source_loop = source_face->lbase;
	target_loop = target_face->lbase;
	do{
		CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata, source_loop->data, &target_loop->data);		
		source_loop = source_loop->next;
		target_loop = target_loop->next;
	}while(source_loop != source_face->lbase);
	
	return target_face;
}
	
/*
 * COPY MESH
 *
 * Internal Copy function.
*/

/*local flag defines*/

#define DUPE_INPUT		1			/*input from operator*/
#define DUPE_NEW		2
#define DUPE_DONE		3

static void copy_mesh(BMMesh *source, BMMesh *target)
{

	BMVert *v = NULL;
	BMEdge *e = NULL, **edar = NULL;
	BMLoop *l = NULL;
	BMFace *f = NULL;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	BMIter loops;
	
	GHash *vhash;
	GHash *ehash;
	
	int maxlength = 0, flag;

	/*initialize pointer hashes*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp); 
	ehash = BLI_ghash_new(BLI_ghashutil_ptrrhash, BLI_ghashutil_ptrcmp);
	
	/*initialize edge pointer array*/
	for(f = BM_iter_new(&faces, source, BM_FACES,  source,  0, NULL); f; f = BM_iter_step(&faces)){
		if(f->len > maxlength) maxlength = f->len;
	}
	edar = MEM_callocN(sizeof(BMEdge*) * maxlength, "BM copy mesh edge pointer array");
	

	/*first we dupe all flagged faces and their elements from source*/
	for(f = BM_iter_new(&faces, source, BM_FACES, source, 0, NULL); f; f= BM_iter_step(&faces)){
		if(BMO_elem_flag_test(source, (BMHeader*)f, DUPE_INPUT)){
			/*vertex pass*/
			for(v = BM_iter_new(&verts, source, BM_VERT_OF_FACE, f, 0, NULL); v; v = BM_iter_step(&verts)){
				if(!BMO_elem_flag_test(source, (BMHeader*)v, DUPE_DONE)){ 
					copy_vertex(source,v, target, vhash);
					BMO_elem_flag_enable(source, (BMHeader*)v, DUPE_DONE);
				}
			}

			/*edge pass*/
			for(e = BM_iter_new(&edges, source, BM_EDGE_OF_FACE, f, 0, NULL); e; e = BMeshIter_step(&edges)){
				if(!BMO_elem_flag_test(source, (BMHeader*)e, DUPE_DONE)){
					copy_edge(source, e, target,  vhash,  ehash);
					BMO_elem_flag_enable(source, (BMHeader*)e, DUPE_DONE);
				}
			}
			copy_face(source, f, target, edar, vhash, ehash);
			BMO_elem_flag_enable(source, (BMHeader*)f, DUPE_DONE);
		}
	}
	
	/*now we dupe all the edges*/
	for(e = BM_iter_new(&edges, source, BM_EDGES, source, 0, NULL); e; e = BM_iter_step(&edges)){
		if(BMO_elem_flag_test(source, (BMHeader*)e, DUPE_INPUT) && (!BMO_elem_flag_test(source, (BMHeader*)e, DUPE_DONE))){
			/*make sure that verts are copied*/
			if(!BMO_elem_flag_test(source, (BMHeader*)e->v1, DUPE_DONE){
				copy_vertex(source, e->v1, target, vhash);
				BMO_elem_flag_enable(source, (BMHeader*)e->v1, DUPE_DONE);
			}
			if(!BMO_elem_flag_test(source, (BMHeader*)e->v2, DUPE_DONE){
				copy_vertex(source, e->v2, target, vhash);
				BMO_elem_flag_enable(source, (BMHeader*)e->v2, DUPE_DONE);
			}
			/*now copy the actual edge*/
			copy_edge(source, e, target,  vhash,  ehash);			
			BMO_elem_flag_enable(source, (BMHeader*)e, DUPE_DONE); 
		}
	}
	
	/*finally dupe all loose vertices*/
	for(v = BM_iter_new(&verts, source, BM_VERTS, source, 0, NULL); v; v = BM_iter_step(&verts)){
		if(BMO_elem_flag_test(source, (BMHeader*)v, DUPE_INPUT) && (!BMO_elem_flag_test(source, (BMHeader*)v, DUPE_DONE))){
			copy_vertex(source, v, target, vhash);
			BMO_elem_flag_enable(source, (BMHeader*)v, DUPE_DONE);
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
BMMesh *bmesh_make_mesh_from_mesh(BMMesh *bm, int allocsize[4])
{
	BMMesh *target = NULL;
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

void dupeop_exec(BMMesh *bm, BMOperator *op)
{
	BMOperator *dupeop = op;
	BMOpSlot *vinput, *einput, *finput, *vnew, *enew, *fnew;
	int i;
	
	vinput = BMO_Get_Slot(dupeop, BMOP_DUPE_VINPUT);
	einput = BMO_Get_Slot(dupeop, BMOP_DUPE_EINPUT);
	finput = BMO_Get_Slot(dupeop, BMOP_DUPE_FINPUT);

	/*go through vinput, einput, and finput and flag elements with private flags*/
	BMO_slot_buffer_flag_enable(bm, dupeop, BMOP_DUPE_VINPUT, DUPE_INPUT);
	BMO_slot_buffer_flag_enable(bm, dupeop, BMOP_DUPE_EINPUT, DUPE_INPUT);
	BMO_slot_buffer_flag_enable(bm, dupeop, BMOP_DUPE_FINPUT, DUPE_INPUT);

	/*use the internal copy function*/
	copy_mesh(bm, bm);
	
	/*Output*/
	/*First copy the input buffers to output buffers - original data*/
	BMO_Copy_Opslot_Buffer_Alloc(dupeop, vinput, BMO_Get_Slot(dupeop, BMOP_DUPE_VORIGINAL));
	BMO_Copy_Opslot_Buffer_Alloc(dupeop, einput, BMO_Get_Slot(dupeop, BMOP_DUPE_EORIGINAL));
	BMO_Copy_Opslot_Buffer_Alloc(dupeop, finput, BMO_Get_Slot(dupeop, BMOP_DUPE_FORIGINAL));

	/*Now alloc the new output buffers*/
	BMO_slot_from_flag(bm, dupeop, BMOP_DUPE_VNEW, DUPE_NEW, BMESH_VERT);
	BMO_slot_from_flag(bm, dupeop, BMOP_DUPE_ENEW, DUPE_NEW, BMESH_EDGE);
	BMO_slot_from_flag(bm, dupeop, BMOP_DUPE_FNEW, DUPE_NEW, BMESH_FACE);
}

void splitop_exec(BMMesh *bm, BMOperator *op)
{
	BMOperator *splitop = op;
	BMOperator dupeop;
	BMOperator delop;

	/*initialize our sub-operators*/
	BMO_op_init(&dupeop, BMOP_DUPE);
	BMO_op_init(&delop, BMOP_DEL);

	BMO_Connect(BMO_Get_Slot(splitop, BMOP_SPLIT_VINPUT),BMO_Get_Slot(&dupeop, BMOP_DUPE_VINPUT));
	BMO_Connect(BMO_Get_Slot(splitop, BMOP_SPLIT_EINPUT),BMO_Get_Slot(&dupeop, BMOP_DUPE_EINPUT));
	BMO_Connect(BMO_Get_Slot(splitop, BMOP_SPLIT_FINPUT),BMO_Get_Slot(&dupeop, BMOP_DUPE_FINPUT));

	BMO_op_exec(&dupeop);

	/*connect outputs of dupe to delete*/
	BMO_Connect(BMO_Get_Slot(&dupeop, BMOP_DUPE_VORIGINAL), BMO_Get_Slot(&delop, BMOP_DEL_VINPUT));
	BMO_Connect(BMO_Get_Slot(&dupeop, BMOP_DUPE_EORIGINAL), BMO_Get_Slot(&delop, BMOP_DEL_EINPUT));
	BMO_Connect(BMO_Get_Slot(&dupeop, BMOP_DUPE_FORIGINAL), BMO_Get_Slot(&delop, BMOP_DEL_FINPUT));

	BMO_op_exec(&delop);

	/*now we make our outputs by copying the dupe outputs*/
	BMO_Copy_Buffer_Alloc(BMO_Get_Slot(&dupeop, BMOP_DUPE_VNEW), BMO_Get_Slot(splitop, BMOP_SPLIT_VOUTPUT));
	BMO_Copy_Buffer_Alloc(BMO_Get_Slot(&dupeop, BMOP_DUPE_ENEW), BMO_Get_Slot(splitop, BMOP_SPLIT_EOUTPUT));
	BMO_Copy_Buffer_Alloc(BMO_Get_Slot(&dupeop, BMOP_DUPE_FNEW), BMO_Get_Slot(splitop, BMOP_SPLIT_FOUTPUT));

	/*cleanup*/
	BMO_op_finish(&dupeop);
	BMO_op_finish(&delop);
}

#endif
