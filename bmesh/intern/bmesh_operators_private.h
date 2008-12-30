#ifndef BMESH_OPERATORS_PRIVATE_H
#define BMESH_OPERATORS_PRIVATE_H

struct BMesh;
struct BMOperator;

void BMO_push(struct BMesh *bm, struct BMOperator *op);
void BMO_pop(struct BMesh *bm);

void splitop_exec(struct BMesh *bm, struct BMOperator *op);
void dupeop_exec(struct BMesh *bm, struct BMOperator *op);
void delop_exec(struct BMesh *bm, struct BMOperator *op);

#endif
