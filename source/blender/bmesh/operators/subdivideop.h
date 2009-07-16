#ifndef _SUBDIVIDEOP_H
#define _SUBDIVIDEOP_H

typedef struct subdparams {
	int numcuts;
	float smooth;
	float fractal;
	int beauty;
	BMOperator *op;
} subdparams;

typedef void (*subd_pattern_fill_fp)(BMesh *bm, BMFace *face, BMVert **verts, 
		                     subdparams *params);

/*
note: this is a pattern-based edge subdivider.
it tries to match a pattern to edge selections on faces,
then executes functions to cut them.
*/
typedef struct subdpattern {
	int seledges[20]; //selected edges mask, for splitting

	/*verts starts at the first new vert cut, not the first vert in the
	  face*/
	subd_pattern_fill_fp connectexec;
	int len; /*total number of verts, before any subdivision*/
} subdpattern;

/*generic subdivision rules:
  
  * two selected edges in a face should make a link
    between them.

  * one edge should do, what? make pretty topology, or just
    split the edge only?
*/

#endif /* _SUBDIVIDEOP_H */