#if

/*
 * BME_WELD.C
 *
 * This file contains functions for welding
 * elements in a mesh togather (remove doubles,
 * collapse, ect).
 *
 * TODO:
 *  -Rewrite this to fit into the new API
 *  -Seperate out find doubles code and put it in 
 *   BME_queries.c
 *
*/


/********* qsort routines *********/


typedef struct xvertsort {
	float x;
	BMVert *v1;
} xvertsort;

static int vergxco(const void *v1, const void *v2)
{
	const xvertsort *x1=v1, *x2=v2;

	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}

struct facesort {
	unsigned long x;
	struct BMFace *f;
};


static int vergface(const void *v1, const void *v2)
{
	const struct facesort *x1=v1, *x2=v2;
	
	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}



/*break this into two functions.... 'find doubles' and 'remove doubles'?*/

static void BME_remove_doubles__splitface(BME_Mesh *bm,BMFace *f,GHash *vhash)
{
	BMVert *doub=NULL, *target=NULL;
	BME_Loop *l;
	BMFace *f2=NULL;
	int split=0;
	
	l=f->loopbase;
	do{
		if(l->v->tflag1 == 2){
			target = BLI_ghash_lookup(vhash,l->v);
			if((BME_vert_in_face(target,f)) && (target != l->next->v) && (target != l->prev->v)){
				doub = l->v;
				split = 1;
				break;
			}
		}
		
		l= l->next;
	}while(l!= f->loopbase);

	if(split){
		f2 = BME_SFME(bm,f,doub,target,NULL);
		BME_remove_doubles__splitface(bm,f,vhash);
		BME_remove_doubles__splitface(bm,f2,vhash);
	}
}

int BME_remove_doubles(BME_Mesh *bm, float limit)		
{

	/* all verts with (flag & 'flag') are being evaluated */
	BMVert *v, *v2, *target;
	BMEdge *e, **edar, *ne;
	BME_Loop *l;
	BMFace *f, *nf;
	xvertsort *sortblock, *sb, *sb1;
	struct GHash *vhash;
	struct facesort *fsortblock, *vsb, *vsb1;
	int a, b, test, amount=0, found;
	float dist;

	/*Build a hash table of doubles to thier target vert/edge.*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);	
	
	/*count amount of selected vertices*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v))amount++;
	}
	
	/*qsort vertices based upon average of coordinate. We test this way first.*/
	sb= sortblock= MEM_mallocN(sizeof(xvertsort)*amount,"sortremovedoub");

	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			sb->x = v->co[0]+v->co[1]+v->co[2];
			sb->v1 = v;
			sb++;
		}
	}
	qsort(sortblock, amount, sizeof(xvertsort), vergxco);

	/* test for doubles */
	sb= sortblock;
	for(a=0; a<amount; a++) {
		v= sb->v1;
		if(!(v->tflag1)) { //have we tested yet?
			sb1= sb+1;
			for(b=a+1; b<amount; b++) {
				/* first test: simple distance. Simple way to discard*/
				dist= sb1->x - sb->x;
				if(dist > limit) break;
				
				/* second test: have we already done this vertex?
				(eh this should be swapped, simple equality test should be cheaper than math above... small savings 
				though) */
				v2= sb1->v1;
				if(!(v2->tflag1)) {
					dist= fabsf(v2->co[0]-v->co[0]);
					if(dist<=limit) {
						dist= fabsf(v2->co[1]-v->co[1]);
						if(dist<=limit) {
							dist= fabsf(v2->co[2]-v->co[2]);
							if(dist<=limit) {
								/*v2 is a double of v. We want to throw out v1 and relink everything to v*/
								BLI_ghash_insert(vhash,v2, v);
								v->tflag1 = 1; //mark this vertex as a target
								v->tflag2++; //increase user count for this vert.
								v2->tflag1 = 2; //mark this vertex as a double.
								BME_VISIT(v2); //mark for delete
							}
						}
					}
				}
				sb1++;
			}
		}
		sb++;
	}
	MEM_freeN(sortblock);

	
	/*todo... figure out what this is for...
	for(eve = em->verts.first; eve; eve=eve->next)
		if((eve->f & flag) && (eve->f & 128))
			EM_data_interp_from_verts(eve, eve->tmp.v, eve->tmp.v, 0.5f);
	*/
	
	/*We cannot collapse a vertex onto another vertex if they share a face and are not connected via a collapsable edge.
		so to deal with this we simply find these offending vertices and split the faces. Note that this is not optimal, but works.
	*/
	
	
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(!(BME_NEWELEM(f))){
			BME_remove_doubles__splitface(bm,f,vhash);
		}
	}
	
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		/*If either vertices of this edge are a double, we must mark it for removal and we create a new one.*/
		if(e->v1->tflag1 == 2 || e->v2->tflag1 == 2){ 
			v = v2 = NULL;
			/*For each vertex in the edge, test to find out what it should equal now.*/
			if(e->v1->tflag1 == 2) v= BLI_ghash_lookup(vhash,e->v1);
			else v = e->v1;
			if(e->v2->tflag1 == 2) v2 = BLI_ghash_lookup(vhash,e->v2);
			else v2 = e->v2;
			
			/*small optimization, test to see if the edge needs to be rebuilt at all*/
			if((e->v1 != v) || (e->v2 != v2)){ /*will this always be true of collapsed edges?*/
				if(v == v2) e->tflag1 = 2; /*mark as a collapsed edge*/
				else if(!BME_disk_existedge(v,v2)) ne = BME_ME(bm,v,v2);
				BME_VISIT(e); /*mark for delete*/
			}
		}
	}


	/* need to remove double edges as well. To do this we decide on one edge to keep,
	 * and if its inserted into hash then we need to remove all other
	 * edges incident upon and relink.*/
	/*
	 *	REBUILD FACES
	 *
	 * Loop through double faces and if they have vertices that have been flagged, they need to be rebuilt.
	 * We do this by looking up the face rebuild faces.
	 * loop through original face, for each loop, if the edge it is attached to is marked for delete and has no
	 * other edge in the hash edge, then we know to skip that loop on face recreation. Simple.
	 */
	
	/*1st loop through, just marking elements*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){ //insert bit here about double edges, mark with a flag (e->tflag2) so that we can nuke it later.
		l = f->loopbase;
		do{
			if(l->v->tflag1 == 2) f->tflag1 = 1; //double, mark for rebuild
			if(l->e->tflag1 != 2) f->tflag2++; //count number of edges in the new face.
			l=l->next;
		}while(l!=f->loopbase);
	}
	
	/*now go through and create new faces*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(f->tflag1 && f->tflag2 < 3) BME_VISIT(f); //mark for delete
		else if (f->tflag1 == 1){ /*is the face marked for rebuild*/
			edar = MEM_callocN(sizeof(BMEdge *)*f->tflag2,"Remove doubles face creation array.");
			a=0;
			l = f->loopbase;
			do{
				v = l->v;
				v2 = l->next->v;
				if(l->v->tflag1 == 2) v = BLI_ghash_lookup(vhash,l->v);
				if(l->next->v->tflag1 == 2) v2 = BLI_ghash_lookup(vhash,l->next->v);
				ne = BME_disk_existedge(v,v2); //use BME_disk_next_edgeflag here or something to find the edge that is marked as 'target'.
				//add in call here to edge doubles hash array... then bobs your uncle.
				if(ne){
					edar[a] = ne;
					a++;
				}
				l=l->next;
			}while(l!=f->loopbase);
			
			if(BME_vert_in_edge(edar[1],edar[0]->v2)){ 
				v = edar[0]->v1; 
				v2 = edar[0]->v2;
			}
			else{ 
				v = edar[0]->v2; 
				v2 = edar[0]->v1;
			}
			
			nf = BME_MF(bm,v,v2,edar,f->tflag2);
	
			/*copy per loop data here*/
			if(nf){
				BME_VISIT(f); //mark for delete
			}
			MEM_freeN(edar);
		}
	}
	
	/*count amount of removed vert doubles*/
	a = 0;
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(v->tflag1 == 2) a++;
	}
	
	/*free memory and return amount removed*/
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
	BLI_ghash_free(vhash,NULL, NULL);
	BME_selectmode_flush(bm);
	return a;
}

static void BME_MeshWalk__collapsefunc(void *userData, BMEdge *applyedge)
{
	int index;
	GHash *collected = userData;
	index = BLI_ghash_size(collected);
	if(!BLI_ghash_lookup(collected,applyedge->v1)){ 
		BLI_ghash_insert(collected,index,applyedge->v1);
		index++;
	}
	if(!BLI_ghash_lookup(collected,applyedge->v2)){
		BLI_ghash_insert(collected,index,applyedge->v2);
	}
}

void BME_collapse_edges(BME_Mesh *bm)
{

	BMVert *v, *cvert;
	GHash *collected;
	float min[3], max[3], cent[3];
	int size, i=0, j, num=0;
	
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(!(BME_ISVISITED(v)) && v->edge){
			/*initiate hash table*/
			collected = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
			/*do the walking.*/
			BME_MeshWalk(bm,v,BME_MeshWalk__collapsefunc,collected,BME_RESTRICTSELECT);
			/*now loop through the hash table twice, once to calculate bounding box, second time to do the actual collapse*/
			size = BLI_ghash_size(collected);
			/*initial values*/
			copy_v3_v3(min,v->co);
			copy_v3_v3(max,v->co);
			cent[0] = cent[1] = cent[2]=0;
			for(i=0; i<size; i++){
				cvert = BLI_ghash_lookup(collected,i);
				cent[0] = cent[0] + cvert->co[0];
				cent[1] = cent[1] + cvert->co[1];
				cent[2] = cent[2] + cvert->co[2];
			}
			
			cent[0] = cent[0] / size;
			cent[1] = cent[1] / size;
			cent[2] = cent[2] / size;
			
			for(i=0; i<size; i++){
				cvert = BLI_ghash_lookup(collected,i);
				copy_v3_v3(cvert->co,cent);
				num++;
			}
			/*free the hash table*/
			BLI_ghash_free(collected,NULL, NULL);
		}
	}
	/*if any collapsed, call remove doubles*/
	if(num){
		//need to change selection mode here, OR do something else? Or does tool change selection mode?
		//selectgrep
		//first clear flags
		BMEdge *e;
		BMFace *f;
		BME_clear_flag_all(bm,BME_VISITED);
		for(v=BME_first(bm,BME_VERT); v; v=BME_next(bm,BME_VERT,v)) v->tflag1 = v->tflag2 = 0;
		for(e=BME_first(bm,BME_EDGE); e; e=BME_next(bm,BME_EDGE,e)) e->tflag1 = e->tflag2 = 0;
		for(f=BME_first(bm,BME_POLY); f; f=BME_next(bm,BME_POLY,f)) f->tflag1 = f->tflag2 = 0;
		/*now call remove doubles*/
		BME_remove_doubles(bm,0.0000001);
	}
	BME_selectmode_flush(bm);
}

#endif
