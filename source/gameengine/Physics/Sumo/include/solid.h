#ifndef SOLID_H
#define SOLID_H

#include "solid_types.h"

#ifdef __cplusplus
extern "C" { 
#endif
    
DT_DECLARE_HANDLE(DT_ObjectHandle);
DT_DECLARE_HANDLE(DT_SceneHandle);
DT_DECLARE_HANDLE(DT_ShapeHandle);
DT_DECLARE_HANDLE(DT_RespTableHandle);

typedef enum DT_ScalarType {
	DT_FLOAT,
	DT_DOUBLE
} DT_ScalarType;

typedef enum DT_ResponseType { 
	DT_NO_RESPONSE,              
	DT_SIMPLE_RESPONSE,
	DT_WITNESSED_RESPONSE,
	DT_DEPTH_RESPONSE,
} DT_ResponseType;
    
typedef struct DT_CollData {
	DT_Vector3 point1;
	DT_Vector3 point2;
	DT_Vector3 normal;
} DT_CollData;

typedef void (*DT_ResponseCallback)(
	void *client_data,
	void *client_object1,
	void *client_object2,
	const DT_CollData *coll_data);

/* Shape definition, similar to OpenGL  */

extern DT_ShapeHandle DT_Box(DT_Scalar x, DT_Scalar y, DT_Scalar z);
extern DT_ShapeHandle DT_Cone(DT_Scalar radius, DT_Scalar height);
extern DT_ShapeHandle DT_Cylinder(DT_Scalar radius, DT_Scalar height);
extern DT_ShapeHandle DT_Sphere(DT_Scalar radius);
extern DT_ShapeHandle DT_Ray(DT_Scalar x, DT_Scalar y, DT_Scalar z);

extern DT_ShapeHandle DT_NewComplexShape();
extern void           DT_EndComplexShape();

extern DT_ShapeHandle DT_NewPolyhedron();
extern void           DT_EndPolyhedron();

extern void DT_Begin();
extern void DT_End();

extern void DT_Vertex(DT_Scalar x, DT_Scalar y, DT_Scalar z);

/* Vertex arrays maintained by the client application can be accessed directly
   by SUMO. For instance, you have a vertex struct in the client of the form:
   
   struct Vertex {
   float xyz[3];
   float uv[2];
   float normal[3];
   };
   
   And maintain vertex arrays e.g. as

   struct Vertex vertices[328];

   Within a Polyhedron or a ComplexShape you can use this data by specifying

   DT_VertexBase(vertices, DT_FLOAT, sizeof(struct Vertex));

   and refer to vertices in the array using   

   DT_VertexIndex(10);

   or 

   DT_Index indices[5] = { 6, 4, 8, 1, 3 };
   DT_VertexIndices(5, indices);

   or even

   DT_VertexRange(8, 4);

   for the range 8, 9, 10, 11.
*/


extern void DT_SetVertexBase(const void *base, DT_ScalarType type,
							 DT_Size stride);
extern void DT_VertexIndex(DT_Index index);
extern void DT_VertexIndices(DT_Count count, const DT_Index *indices);
extern void DT_VertexRange(DT_Index first, DT_Count count); 


/* currently not implemented */
extern void DT_ChangeVertexBase(DT_ShapeHandle shape, const void *base);

extern void DT_DeleteShape(DT_ShapeHandle shape);

/* Scene */

extern DT_SceneHandle DT_CreateScene(); 
extern void           DT_DeleteScene(DT_SceneHandle scene);

extern void DT_AddObject(DT_SceneHandle scene, DT_ObjectHandle object);
extern void DT_RemoveObject(DT_SceneHandle scene, DT_ObjectHandle object);



/* Object  */

extern DT_ObjectHandle DT_CreateObject(
	void *client_object,      /* pointer to object in client memory */
	DT_ShapeHandle shape  /* the shape or geometry of the object */
	);

extern void DT_DeleteObject(DT_ObjectHandle object);

extern void DT_SetScaling(DT_ObjectHandle object, const DT_Vector3 scaling);
extern void DT_SetPosition(DT_ObjectHandle object, const DT_Vector3 position);
extern void DT_SetOrientation(DT_ObjectHandle object, const DT_Quaternion orientation);

extern void DT_SetMargin(DT_ObjectHandle object, DT_Scalar margin);

extern void DT_SetMatrixf(DT_ObjectHandle object, const float *m); 
extern void DT_GetMatrixf(DT_ObjectHandle object, float *m); 

extern void DT_SetMatrixd(DT_ObjectHandle object, const double *m); 
extern void DT_GetMatrixd(DT_ObjectHandle object, double *m); 

extern void DT_GetWorldCoord(DT_ObjectHandle object,
							 const DT_Vector3 local,
							 DT_Vector3 world);

extern DT_Scalar DT_GetClosestPair(DT_ObjectHandle object1, DT_ObjectHandle object2,
								   DT_Vector3 point1, DT_Vector3 point2);  


/* Response, see SOLID user manual */

extern DT_RespTableHandle DT_CreateRespTable(); 
extern void               DT_DeleteRespTable(DT_RespTableHandle respTable); 

extern void DT_CallResponse(DT_RespTableHandle respTable,
							DT_ObjectHandle object1,
							DT_ObjectHandle object2,
							const DT_CollData *coll_data);

extern void DT_SetDefaultResponse(DT_RespTableHandle respTable,
								  DT_ResponseCallback response, 
								  DT_ResponseType type, 
								  void *client_data);

extern void DT_ClearDefaultResponse(DT_RespTableHandle respTable);

extern void DT_SetObjectResponse(DT_RespTableHandle respTable,
								 DT_ObjectHandle object,
								 DT_ResponseCallback response,
								 DT_ResponseType type, void *client_data);
extern void DT_ClearObjectResponse(DT_RespTableHandle respTable,
								   DT_ObjectHandle object);

extern void DT_SetPairResponse(DT_RespTableHandle respTable,
							   DT_ObjectHandle object1,
							   DT_ObjectHandle object2, 
							   DT_ResponseCallback  response,
							   DT_ResponseType type, 
							   void *client_data);
extern void DT_ClearPairResponse(DT_RespTableHandle respTable,
								 DT_ObjectHandle object1, 
								 DT_ObjectHandle object2);



/* Perform a collision test for a given scene, using a response table */
 
extern DT_Count DT_Test(DT_SceneHandle scene, DT_RespTableHandle respTable);

extern void *DT_RayTest(DT_SceneHandle scene, void *ignore_client,
						const DT_Vector3 from, const DT_Vector3 to,
						DT_Vector3 spot, DT_Vector3 normal);

extern int DT_ObjectRayTest(DT_ObjectHandle object,
							const DT_Vector3 from, const DT_Vector3 to,
							DT_Vector3 spot, DT_Vector3 normal);

#ifdef __cplusplus
}
#endif

#endif
