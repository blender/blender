struct BMEditMesh;
struct BMFace;
struct BMEdge;
struct BMVert;
struct RegionView3D;

struct BMBVHTree;
typedef struct BMBVHTree BMBVHTree;

BMBVHTree *BMBVH_NewBVH(struct BMEditMesh *em);
void BMBVH_FreeBVH(BMBVHTree *tree);

struct BMFace *BMBVH_RayCast(BMBVHTree *tree, float *co, float *dir, float *hitout);
int BMBVH_EdgeVisible(BMBVHTree *tree, struct BMEdge *e, 
                      struct RegionView3D *r3d, struct Object *obedit);
