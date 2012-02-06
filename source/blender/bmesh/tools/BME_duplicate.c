#if 0

/*
 *  BME_DUPLICATE.C
 *
 *  This file contains functions for duplicating, copying, and splitting
 *  elements from a bmesh.
 *
 */

/*
 *  BMESH COPY VERTEX
 *
 *   Copy an existing vertex from one bmesh to another.
 *
*/

static BMVert *bmesh_copy_vertex(BMMesh *source_mesh, BMVert *source_vertex, BMMesh *target_mesh, GHash *vhash)
{
	BMVert *target_vertex = NULL;

	/*create a new vertex*/
	target_vertex = bmesh_make_vert(target, source_vertex->co,  NULL);

	/*insert new vertex into the vert hash*/
	BLI_ghash_insert(vhash, source_vertex, target_vertex);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata, source_vertex->data, &target_vertex->data);	

	/*copy flags*/
	if(bmesh_test_flag(source_vertex, BMESH_SELECT)) bmesh_set_flag(target_vertex, BMESH_SELECT);
	if(bmesh_test_flag(source_vertex, BMESH_HIDDEN)) bmesh_set_flag(target_vertex, BMESH_HIDDEN);

	return target_vertex;
}

/*
 * BMESH COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 *
*/

static BMEdge *bmesh_copy_edge(BMMesh *source_mesh, BMEdge *source_edge, BMMesh *target_mesh, GHash *vhash, GHash *ehash)
{
	BMEdge *target_edge = NULL;
	BMVert *target_vert1, *target_vert2;
	
	/*lookup v1 and v2*/
	target_vert1 = BLI_ghash_lookup(vhash, source_edge->v1);
	target_vert2 = BLI_ghash_lookup(vhash, source_edge->v2);
	
	/*create a new edge*/
	target_edge = bmesh_make_edge(target_mesh, target_vert1, target_vert2, NULL, 0);

	/*insert new edge into the edge hash*/
	BLI_ghash_insert(ehash, source_edge, target_edge);	
	
	/*copy custom data in this function since we cannot be assured that byte layout is same between meshes*/	
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata, source_edge->data, &target_edge->data);
	
	/*copy flags*/
	if(bmesh_test_flag(source_edge, BMESH_SELECT)) bmesh_set_flag(target_edge, BMESH_SELECT);
	if(bmesh_test_flag(source_edge, BMESH_HIDDEN)) bmesh_set_flag(target_edge, BMESH_SELECT);
	if(bmesh_test_flag(source_edge, BMESH_SHARP)) bmesh_set_flag(target_edge, BMESH_SHARP);
	if(bmesh_test_flag(source_edge, BMESH_SEAM)) bmesh_set_flag(target_edge, BMESH_SEAM);
	if(bmesh_test_flag(source_edge, BMESH_FGON)) bmesh_set_flag(target_edge, BMESH_FGON);
	
	return target_edge;
}

/*
 * BMESH COPY FACE
 *
 *  Copy an existing face from one bmesh to another.
 *
*/

static BMFace *bmesh_copy_face(BMMesh *source_mesh, BMFace *source_face, BMMesh *target_mesh, BMEdge **edar, GHash *verthash, GHash *ehash)
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
	target_face = bmesh_make_ngon(target_mesh, target_vert1, target_vert2, edar, source_face->len, 0);	
	
	/*we copy custom data by hand, we cannot assume that customdata byte layout will be exactly the same....*/
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata, source_face->data, &target_face->data);	

	/*copy flags*/
	if(bmesh_test_flag(source_face, BMESH_SELECT)) bmesh_set_flag(target_face, BMESH_SELECT);
	if(bmesh_test_flag(source_face, BMESH_HIDDEN)) bmesh_set_flag(target_face, BMESH_HIDDEN);
	
	/*mark the face as dirty for normal and tesselation calcs*/
	bmesh_set_flag(target_face, BMESH_DIRTY);
	
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
 * BMESH COPY MESH
 *
 * Internal Copy function. copies flagged elements from 
 * source to target, which may in fact be the same mesh.
 * Note that if __flag is 0, all elements will be copied.
 *
*/

static void bmesh_copy_mesh(BMMesh *source, BMMesh *target, int __flag)
{

	BMVert *v;
	BMEdge *e, **edar;
	BMLoop *l;
	BMFace *f;
	
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
	for(f = BMeshIter_init(faces, BM_FACES,  source,  0); f; f = BMeshIter_step(faces)){
		if(f->len > maxlength) maxlength = f->len;
	}
	edar = MEM_callocN(sizeof(BMEdge*) * maxlength, "BM copy mesh edge pointer array");
	
	/*begin modelling loop for target*/
	bmesh_begin_edit(target);

	/*we make special exception for __flag == 0... we copy all*/
	if(!__flag){
		flag = BMESH_DUPE; 
		for(v = BMeshIter_init(verts, BM_VERTS, source, 0); v; v = BMeshIter_step(verts)) bmesh_set_flag(v, BMESH_DUPE);
		for(e = BMeshIter_init(verts, BM_EDGES, source, 0); e; e = BMeshIter_step(edges)) bmesh_set_flag(e, BMESH_DUPE);
		for(f = BMeshIter_init(faces, BM_FACES, source, 0); f; f = BMeshIter_step(faces)) bmesh_set_flag(f, BMESH_DUPE);
	} else{
		flag = __flag;
	}
	
	/*first we dupe all flagged faces and their elements from source*/
	for(f = BMeshIter_init(faces, BM_FACES, source, 0); f; f= BMeshIter_step(faces)){
		if(bmesh_test_flag(f, flag)){
			/*vertex pass*/
			for(l = BMeshIter_init(loops, BMESH_LOOP_OF_MESH, f, 0); l; l = BMeshIter_step(loops)){
				if(!bmesh_test_flag(l->v, BMESH_DUPED)){ 
					bmesh_copy_vertex(source,l->v, target, vhash);
					bmesh_set_flag(l->v, BMESH_DUPED);
				}
			}

			/*edge pass*/
			for(l = BMeshIter_init(loops, BMESH_LOOP_OF_MESH, f, 0); l; l = BMeshIter_step(loops)){
				if(!bmesh_test_flag(l->e, BMESH_DUPED)){ 
					bmesh_copy_edge(source, l->e, target,  vhash,  ehash);
					bmesh_set_flag(l->e, BMESH_DUPED);
				}
			}
			bmesh_copy_face(source, f, target, edar, vhash, ehash);
			bmesh_set_flag(f, BMESH_DUPED);		
		}
	}
	
	/*now we dupe all the edges*/
	for(e = BMeshIter_init(edges, BM_EDGES, source, 0); e; e = BMeshIter_step(edges)){
		if(bmesh_test_flag(e, flag) && (!bmesh_test_flag(e, BMESH_DUPED))){
			/*make sure that verts are copied*/
			if(!bmesh_test_flag(e->v1, BMESH_DUPED)){
				bmesh_copy_vertex(source, e->v1, target, vhash);
				bmesh_set_flag(e->v1, BMESH_DUPED);
			}
			if(!bmesh_test_flag(e->v2, BMESH_DUPED)){
				bmesh_copy_vertex(source, e->v2, target, vhash);
				bmesh_set_flag(e->v2, BMESH_DUPED);
			}
			/*now copy the actual edge*/
			bmesh_copy_edge(source, e, target,  vhash,  ehash);			
			bmesh_set_flag(e, BMESH_DUPED);
		}
	}
	
	/*finally dupe all loose vertices*/
	for(v = BMeshIter_init(verts, BM_VERTS, bm, 0); v; v = BMeshIter_step(verts)){
		if(bmesh_test_flag(v, flag) && (!bmesh_test_flag(v, BMESH_DUPED))){
			bmesh_copy_vertex(source, v, target, vhash);
			bmesh_set_flag(v, BMESH_DUPED);
		}
	}
	
	/*finish*/
	bmesh_end_edit(target, BMESH_CALC_NORM | BMESH_CALC_TESS);
	
	/*free pointer hashes*/
	BLI_ghash_free(vhash, NULL, NULL);
	BLI_ghash_free(ehash, NULL, NULL);	

	/*free edge pointer array*/
	MEM_freeN(edar);
}

/*
 * BMESH MAKE MESH FROM MESH
 *
 * Creates a new mesh by duplicating an existing one.
 *
*/

BMMesh *bmesh_make_mesh_from_mesh(BMMesh *bm, int allocsize[4])
{
	BMMesh *target = NULL;
	target = bmesh_make_mesh(allocsize);
	
	/*copy custom data layout*/
	CustomData_copy(&bm->vdata, &target->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->edata, &target->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->ldata, &target->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->pdata, &target->pdata, CD_MASK_BMESH, CD_CALLOC, 0);
	
	/*initialize memory pools*/
	CustomData_bmesh_init_pool(&target->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&target->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&target->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&target->pdata, allocsize[3]);
	
	bmesh_begin_edit(bm);
	bmesh_begin_edit(target);
	
	bmesh_copy_mesh(bm, target, 0);	/* copy all elements */

	bmesh_end_edit(bm);
	bmesh_end_edit(target);
	
	return target;

}

/*
 * BMESH SPLIT MESH
 *
 * Copies flagged elements then deletes them.
 *
*/

void bmesh_split_mesh(BMMesh *bm, int flag)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	bmesh_begin_edit(bm);
	bmesh_copy_mesh(bm, bm, flag);

	/*mark verts for deletion*/
	for(v = BMeshIter_init(verts, BM_VERTS, bm, 0); v; v = BMeshIter_step(verts)){
		if(bmesh_test_flag(v, flag)) bmesh_delete_vert(bm, v);
	}
	/*mark edges for deletion*/
	for(e = BMeshIter_init(edges, BM_EDGES, bm, 0); e; e = BMeshIter_step(edges)){
		if(bmesh_test_flag(e, flag)) bmesh_delete_edge(bm, e);
		
	}
	/*mark faces for deletion*/
	for(f = BMeshIter_init(faces, BM_FACES, bm, 0); f; f= BMeshIter_step(faces)){
		if(bmesh_tes t_flag(f, flag)) bmesh_delete_face(bm, f);
		
	}
	bmesh_end_edit(bm);	
}

#endif
