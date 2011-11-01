/**
 *          BMESH EXTRUDE TOOL
 *
 * A rewrite of the old editmesh extrude code with the 
 * redundant parts broken into multiple functions
 * in an effort to reduce code. This works with multiple 
 * selection modes, and is intended to build the
 * extrusion in steps, depending on what elements are selected. 
 * Also decoupled the calculation of transform normal
 * and put it in UI where it probably is more appropriate 
 * for the moment.
 *
 * TODO:
 *  -Fit this into the new 'easy' API.
*/

void BME_extrude_verts(BME_Mesh *bm, GHash *vhash){
	BMVert *v, *nv = NULL;
	BMEdge *ne = NULL;
	float vec[3];

	//extrude the vertices
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			VECCOPY(vec,v->co);
			nv = BME_MV(bm,vec);
			nv->tflag2 =1; //mark for select
			ne = BME_ME(bm,v,nv);
			ne->tflag1 = 2; //mark as part of skirt 'ring'
			BLI_ghash_insert(vhash,v,nv);
			BME_VISIT(v);
		}
	}
}

void BME_extrude_skirt(BME_Mesh *bm, GHash *ehash){
	
	BMFace *nf=NULL;
	BMEdge *e, *l=NULL, *r=NULL, *edar[4], *ne;
	BMVert *v, *v1, *v2, *lv, *rv, *nv;

	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)){
			/*find one face incident upon e and use it for winding of new face*/
			if(e->l){
				v1 = e->l->next->v;
				v2 = e->l->v;
			}
			else{
				v1 = e->v1;
				v2 = e->v2;
			}
			
			if(v1->e->tflag1 == 2) l = v1->e;
			else l = BME_disk_next_edgeflag(v1->e, v1, 0, 2);
			if(v2->e->tflag1 == 2) r = v2->e;
			else r = BME_disk_next_edgeflag(v2->e, v2, 0, 2);
			
			lv = BME_edge_getothervert(l,v1);
			rv = BME_edge_getothervert(r,v2);
			
			ne = BME_ME(bm,lv,rv);
			ne->tflag2 = 1; //mark for select
			BLI_ghash_insert(ehash,e,ne);
			BME_VISIT(e);
			
			edar[0] = e;
			edar[1] = l;
			edar[2] = ne;
			edar[3] = r;
			BME_MF(bm,v1,v2,edar,4);
		}
	}
}

void BME_cap_skirt(BME_Mesh *bm, GHash *vhash, GHash *ehash){
	BMVert *v, *nv, *v1, *v2;
	BMEdge *e, **edar, *ne;
	BME_Loop *l;
	BMFace *f, *nf;
	MemArena *edgearena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	float vec[3];
	int i, j, del_old =0;
	

	//loop through faces, then loop through their verts. If the verts havnt been visited yet, duplicate these.
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			l = f->loopbase;
			do{
				if(!(BME_ISVISITED(l->v))){ //interior vertex
					//dupe vert
					VECCOPY(vec,l->v->co);
					nv = BME_MV(bm,vec);
					BLI_ghash_insert(vhash,l->v,nv);
					//mark for delete
					l->v->tflag1 = 1;
					BME_VISIT(l->v); //we dont want to dupe it again.
				}
				l=l->next;
			}while(l!=f->lbase);
		}
	}
	
	//find out if we delete old faces or not. This needs to be improved a lot.....
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e) && e->l){
			i= BME_cycle_length(&(e->l->radial));
			if(i > 2){ 
				del_old = 1;
				break;
			}
		}
	}
	
	
	//build a new edge net, insert the new edges into the edge hash
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			l=f->loopbase;
			do{
				if(!(BME_ISVISITED(l->e))){ //interior edge
					//dupe edge
					ne = BME_ME(bm,BLI_ghash_lookup(vhash,l->e->v1),BLI_ghash_lookup(vhash,l->e->v2));
					BLI_ghash_insert(ehash,l->e,ne);
					//mark for delete
					l->e->tflag1 = 1;
					BME_VISIT(l->e); //we dont want to dupe it again.
				}
				l=l->next;
			}while(l!=f->lbase);
		}
	}
	
	//build new faces. grab edges from edge hash.
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			edar = MEM_callocN(sizeof(BMEdge*)*f->len,"Extrude array");
			 v1 = BLI_ghash_lookup(vhash,f->loopbase->v);
			v2 = BLI_ghash_lookup(vhash,f->loopbase->next->v);
			for(i=0,l=f->loopbase; i < f->len; i++,l=l->next){
				ne = BLI_ghash_lookup(ehash,l->e);
				edar[i] = ne;
			}
			nf=BME_MF(bm,v1,v2,edar,f->len);
			nf->tflag2 = 1; // mark for select
			if(del_old) f->tflag1 = 1; //mark for delete
			MEM_freeN(edar);
		}
	}
	BLI_memarena_free(edgearena);
}

/*unified extrude code*/
void BME_extrude_mesh(BME_Mesh *bm, int type)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BME_Loop *l;
	
	struct GHash *vhash, *ehash;
	/*Build a hash table of old pointers and new pointers.*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	BME_selectmode_flush(bm); //ensure consistent selection. contains hack to make sure faces get consistent select.
	if(type & BME_EXTRUDE_FACES){ //Find selected edges with more than one incident face that is also selected. deselect them.
		for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
			int totsel=0;
			if(e->l){
				l= e->l;
				do{
					if(BME_SELECTED(l->f)) totsel++;
					l=BME_radial_nextloop(l);
				}while(l!=e->l);
			}
			if(totsel > 1) BME_select_edge(bm,e,0);
		}
	}

	/*another hack to ensure consistent selection.....*/
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)) BME_select_edge(bm,e,1);
	}
	
	/*now we are ready to extrude*/
	if(type & BME_EXTRUDE_VERTS) BME_extrude_verts(bm,vhash);
	if(type & BME_EXTRUDE_EDGES) BME_extrude_skirt(bm,ehash);
	if(type & BME_EXTRUDE_FACES) BME_cap_skirt(bm,vhash,ehash);
	
	/*clear all selection flags*/
	BME_clear_flag_all(bm, SELECT|BME_VISITED);
	/*go through and fix up selection flags. Anything with BME_NEW should be selected*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(f->tflag2 == 1) BME_select_poly(bm,f,1);
		if(f->tflag1 == 1) BME_VISIT(f); //mark for delete
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(e->tflag2 == 1) BME_select_edge(bm,e,1);
		if(e->tflag1 == 1) BME_VISIT(e); // mark for delete
	}
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(v->tflag2 == 1) BME_select_vert(bm,v,1);
		if(v->tflag1 == 1) BME_VISIT(v); //mark for delete
	}
	/*go through and delete all of our old faces , edges and vertices.*/
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
	/*free our hash tables*/
	BLI_ghash_free(vhash,NULL, NULL); //check usage!
	BLI_ghash_free(ehash,NULL, NULL); //check usage!
	BME_selectmode_flush(bm);
}

