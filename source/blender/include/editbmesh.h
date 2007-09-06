struct Mesh;
struct CustomData;
struct BME_Mesh;
struct BME_Vert;
struct BME_Edge;
struct BME_Loop;
struct BME_Face;

struct BME_Mesh *BME_FromMesh(Mesh *mesh);
void Mesh_FromBMesh(struct BME_Mesh *bmesh, struct Mesh *mesh);
void EditBME_remakeEditMesh(void);
void EditBME_makeEditMesh(void);
void EditBME_loadEditMesh(struct Mesh *mesh);
void EditBME_MouseClick();

void EditBME_FlushSelUpward(struct BME_Mesh *mesh);

void BME_data_interp_from_verts(struct BME_Vert *v1, struct BME_Vert *v2, struct BME_Vert *eve, float fac);
void BME_add_data_layer(struct CustomData *data, int type);
void BME_free_data_layer(struct CustomData *data, int type);

struct BME_Vert *EditBME_FindNearestVert(int *dis, short sel, short strict);
struct BME_Edge *EditBME_FindNearestEdge(int *dis);
struct BME_Poly *EditBME_FindNearestPoly(int *dis);
void mouse_bmesh(void);


/*editbmesh_tools.c*/
void EM_cut_edges(int numcuts);
void EM_dissolve_edges(void);
void EM_delete_context(void);