#ifndef BM_OPERATORS_PRIVATE_H
#define BM_OPERATORS_PRIVATE_H

struct BMesh;
struct BMOperator;

void BMO_push(struct BMesh *bm, struct BMOperator *op);
void BMO_pop(struct BMesh *bm);

void splitop_exec(struct BMesh *bm, struct BMOperator *op);
void dupeop_exec(struct BMesh *bm, struct BMOperator *op);
void delop_exec(struct BMesh *bm, struct BMOperator *op);
void esubdivide_exec(BMesh *bmesh, BMOperator *op);
void edit2bmesh_exec(BMesh *bmesh, BMOperator *op);
void bmesh2edit_exec(BMesh *bmesh, BMOperator *op);
void triangulate_exec(BMesh *bmesh, BMOperator *op);
void dissolvefaces_exec(BMesh *bmesh, BMOperator *op);
void dissolveverts_exec(BMesh *bmesh, BMOperator *op);
void bmesh_make_fgons_exec(BMesh *bmesh, BMOperator *op);
void extrude_edge_context_exec(BMesh *bm, BMOperator *op);
void connectverts_exec(BMesh *bm, BMOperator *op);

#endif
