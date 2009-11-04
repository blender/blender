struct BMEditMesh;
struct BMFace;
struct BMEdge;
struct BMVert;
struct RegionView3D;
struct BMBVHTree;

#ifndef IN_EDITMESHBVH
typedef struct BMBVHTree BMBVHTree;
#endif

struct BMBVHTree *BMBVH_NewBVH(struct BMEditMesh *em);
void BMBVH_FreeBVH(struct BMBVHTree *tree);

struct BMFace *BMBVH_RayCast(struct BMBVHTree *tree, float *co, float *dir, float *hitout);
int BMBVH_EdgeVisible(struct BMBVHTree *tree, struct BMEdge *e, 
                      struct RegionView3D *r3d, struct Object *obedit);
