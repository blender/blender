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
void makeprim_exec(BMesh *bm, BMOperator *op);
void extrude_vert_indiv_exec(BMesh *bm, BMOperator *op);
void mesh_to_bmesh_exec(BMesh *bm, BMOperator *op);
void bmesh_to_mesh_exec(BMesh *bm, BMOperator *op);
void bmesh_translate_exec(BMesh *bm, BMOperator *op);
void bmesh_transform_exec(BMesh *bm, BMOperator *op);
void bmesh_contextual_create_exec(BMesh *bm, BMOperator *op);
void bmesh_edgenet_fill_exec(BMesh *bm, BMOperator *op);
void bmesh_rotate_exec(BMesh *bm, BMOperator *op);
void bmesh_makevert_exec(BMesh *bm, BMOperator *op);


#endif
