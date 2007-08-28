/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef SOLID_H
#define SOLID_H

#include "SOLID_types.h"

#ifdef __cplusplus
extern "C" { 
#endif
    
	DT_DECLARE_HANDLE(DT_ObjectHandle);
	DT_DECLARE_HANDLE(DT_SceneHandle);
	DT_DECLARE_HANDLE(DT_ShapeHandle);
	DT_DECLARE_HANDLE(DT_VertexBaseHandle);
	DT_DECLARE_HANDLE(DT_RespTableHandle);
	DT_DECLARE_HANDLE(DT_ArchiveHandle);

	typedef unsigned int DT_ResponseClass;

	typedef enum DT_ResponseType { 
		DT_NO_RESPONSE,                  /* No response (obsolete) */
		DT_BROAD_RESPONSE,               /* Broad phase response is returned. */
		DT_SIMPLE_RESPONSE,              /* No collision data */
		DT_WITNESSED_RESPONSE,           /* A point common to both objects
											is returned as collision data
										 */
		DT_DEPTH_RESPONSE                /* The penetration depth is returned
											as collision data. The penetration depth
											is the shortest vector over which one 
											object needs to be translated in order
											to bring the objects in touching contact. 
										 */ 
	} DT_ResponseType;
    
/* For witnessed response, the following structure represents a common point. The world 
   coordinates of 'point1' and 'point2' coincide. 'normal' is the zero vector.
   
   For depth response, the following structure represents the penetration depth. 
   'point1' en 'point2' are the witness points of the penetration depth in world coordinates.
   The penetration depth vector in world coordinates is represented by 'normal'.
*/

	typedef struct DT_CollData {
		DT_Vector3 point1;               /* Point in object1 in world coordinates */ 
		DT_Vector3 point2;               /* Point in object2 in world coordinates */
		DT_Vector3 normal;               /* point2 - point1 */ 
	} DT_CollData;

/* A response callback is called by SOLID for each pair of collding objects. 'client-data'
   is a pointer to an arbitrary structure in the client application. The client objects are
   pointers to structures in the client application associated with the coliding objects.
   'coll_data' is the collision data computed by SOLID.
*/

	typedef DT_Bool (*DT_ResponseCallback)(void *client_data,
										   void *client_object1,
										   void *client_object2,
										   const DT_CollData *coll_data);
										
/* Shape definition */


	extern DECLSPEC DT_ShapeHandle DT_NewBox(DT_Scalar x, DT_Scalar y, DT_Scalar z);
	extern DECLSPEC DT_ShapeHandle DT_NewCone(DT_Scalar radius, DT_Scalar height);
	extern DECLSPEC DT_ShapeHandle DT_NewCylinder(DT_Scalar radius, DT_Scalar height);
	extern DECLSPEC DT_ShapeHandle DT_NewSphere(DT_Scalar radius);
	extern DECLSPEC DT_ShapeHandle DT_NewPoint(const DT_Vector3 point);
	extern DECLSPEC DT_ShapeHandle DT_NewLineSegment(const DT_Vector3 source, const DT_Vector3 target);
	extern DECLSPEC DT_ShapeHandle DT_NewMinkowski(DT_ShapeHandle shape1, DT_ShapeHandle shape2);
	extern DECLSPEC DT_ShapeHandle DT_NewHull(DT_ShapeHandle shape1, DT_ShapeHandle shape2);

	extern DECLSPEC DT_VertexBaseHandle DT_NewVertexBase(const void *pointer, DT_Size stride);
	extern DECLSPEC void DT_DeleteVertexBase(DT_VertexBaseHandle vertexBase);	
	extern DECLSPEC void DT_ChangeVertexBase(DT_VertexBaseHandle vertexBase, const void *pointer);

	extern DECLSPEC DT_ShapeHandle DT_NewComplexShape(DT_VertexBaseHandle vertexBase);
	extern DECLSPEC void           DT_EndComplexShape();

	extern DECLSPEC DT_ShapeHandle DT_NewPolytope(DT_VertexBaseHandle vertexBase);
	extern DECLSPEC void           DT_EndPolytope();

	extern DECLSPEC void DT_Begin();
	extern DECLSPEC void DT_End();

	extern DECLSPEC void DT_Vertex(const DT_Vector3 vertex);
	extern DECLSPEC void DT_VertexIndex(DT_Index index);

	extern DECLSPEC void DT_VertexIndices(DT_Count count, const DT_Index *indices);
	extern DECLSPEC void DT_VertexRange(DT_Index first, DT_Count count); 

	extern DECLSPEC void DT_DeleteShape(DT_ShapeHandle shape);

/* Object  */

	extern DECLSPEC DT_ObjectHandle DT_CreateObject(
		void *client_object,      /* pointer to object in client memory */
		DT_ShapeHandle shape  /* the shape or geometry of the object */
		);

	extern DECLSPEC void DT_DestroyObject(DT_ObjectHandle object);



	extern DECLSPEC void DT_SetPosition(DT_ObjectHandle object, const DT_Vector3 position);
	extern DECLSPEC void DT_SetOrientation(DT_ObjectHandle object, const DT_Quaternion orientation);
	extern DECLSPEC void DT_SetScaling(DT_ObjectHandle object, const DT_Vector3 scaling);

/* The margin is an offset from the actual shape. The actual geometry of an
   object is the set of points whose distance to the transformed shape is at 
   most the  margin. During the lifetime of an object the margin can be 
   modified. 
*/
   
	extern DECLSPEC void DT_SetMargin(DT_ObjectHandle object, DT_Scalar margin);


/* These commands assume a column-major 4x4 OpenGL matrix representation */

	extern DECLSPEC void DT_SetMatrixf(DT_ObjectHandle object, const float *m); 
	extern DECLSPEC void DT_GetMatrixf(DT_ObjectHandle object, float *m); 

	extern DECLSPEC void DT_SetMatrixd(DT_ObjectHandle object, const double *m); 
	extern DECLSPEC void DT_GetMatrixd(DT_ObjectHandle object, double *m); 

	extern DECLSPEC void DT_GetBBox(DT_ObjectHandle object, DT_Vector3 min, DT_Vector3 max);

	
	extern DECLSPEC DT_Bool  DT_GetIntersect(DT_ObjectHandle object1, DT_ObjectHandle object2,
												DT_Vector3 v);
/* This next command returns the distance between the objects. De returned
   closest points are given in world coordinates.
*/
	extern DECLSPEC DT_Scalar DT_GetClosestPair(DT_ObjectHandle object1, DT_ObjectHandle object2,
												DT_Vector3 point1, DT_Vector3 point2);  

	extern DECLSPEC DT_Bool   DT_GetCommonPoint(DT_ObjectHandle object1, DT_ObjectHandle object2,
												DT_Vector3 point);

	extern DECLSPEC DT_Bool   DT_GetPenDepth(DT_ObjectHandle object1, DT_ObjectHandle object2,
											 DT_Vector3 point1, DT_Vector3 point2);  

/* Scene */

	extern DECLSPEC DT_SceneHandle DT_CreateScene(); 
	extern DECLSPEC void           DT_DestroyScene(DT_SceneHandle scene);

	extern DECLSPEC void DT_AddObject(DT_SceneHandle scene, DT_ObjectHandle object);
	extern DECLSPEC void DT_RemoveObject(DT_SceneHandle scene, DT_ObjectHandle object);

/* Note that objects can be assigned to multiple scenes! */

/* Response */

/* Response tables are defined independent of the scenes in which they are used.
   Multiple response tables can be used in one scene, and a response table
   can be shared among scenes.
*/
	extern DECLSPEC DT_RespTableHandle DT_CreateRespTable(); 
	extern DECLSPEC void               DT_DestroyRespTable(DT_RespTableHandle respTable); 

/* Responses are defined on (pairs of) response classes. Each response table 
   maintains its set of response classes.
*/
	extern DECLSPEC DT_ResponseClass DT_GenResponseClass(DT_RespTableHandle respTable);

/* To each object for which a response is defined in the response table a
   response class needs to be assigned. 
*/

	extern DECLSPEC void DT_SetResponseClass(DT_RespTableHandle respTable,
											 DT_ObjectHandle object,
											 DT_ResponseClass responseClass);

	extern DECLSPEC void DT_ClearResponseClass(DT_RespTableHandle respTable, 
											   DT_ObjectHandle object);

	extern DECLSPEC void DT_CallResponse(DT_RespTableHandle respTable,
										 DT_ObjectHandle object1,
										 DT_ObjectHandle object2,
										 const DT_CollData *coll_data);

/* For each pair of objects multiple responses can be defined. A response is a callback
   together with its response type and client data. */
    
/* Responses can be defined for all pairs of response classes... */
	extern DECLSPEC void DT_AddDefaultResponse(DT_RespTableHandle respTable,
											   DT_ResponseCallback response, 
											   DT_ResponseType type, void *client_data);

	extern DECLSPEC void DT_RemoveDefaultResponse(DT_RespTableHandle respTable,
												  DT_ResponseCallback response);
/* ...per response class... */
	extern DECLSPEC void DT_AddClassResponse(DT_RespTableHandle respTable,
											 DT_ResponseClass responseClass,
											 DT_ResponseCallback response,
											 DT_ResponseType type, void *client_data);

	extern DECLSPEC void DT_RemoveClassResponse(DT_RespTableHandle respTable,
												DT_ResponseClass responseClass,
												DT_ResponseCallback response);

/* ... and per pair of response classes...*/
	extern DECLSPEC void DT_AddPairResponse(DT_RespTableHandle respTable,
											DT_ResponseClass responseClass1,
											DT_ResponseClass responseClass2, 
											DT_ResponseCallback response,
											DT_ResponseType type, void *client_data);
	extern DECLSPEC void DT_RemovePairResponse(DT_RespTableHandle respTable,
											   DT_ResponseClass responseClass1,
											   DT_ResponseClass responseClass2,
											   DT_ResponseCallback response);

/* The next command calls the response callbacks for all intersecting pairs of objects in a scene. 
   'DT_Test' returns the number of pairs of objects for which callbacks have been called. 
*/
 
	extern DECLSPEC DT_Count DT_Test(DT_SceneHandle scene, DT_RespTableHandle respTable);

/* Set the maximum relative error in the closest points and penetration depth
   computation. The default for `max_error' is 1.0e-3. Larger errors result
   in better performance. Non-positive error tolerances are ignored.
*/ 

	extern DECLSPEC void DT_SetAccuracy(DT_Scalar max_error);

/* Set the maximum tolerance on relative errors due to rounding.  The default for `tol_error' 
   is the machine epsilon. Very large tolerances result in false collisions. Setting tol_error too small 
   results in missed collisions. Non-positive error tolerances are ignored. 
*/ 
    
	extern DECLSPEC void DT_SetTolerance(DT_Scalar tol_error);


/* This function returns the client pointer to the first object in a scene hit by the ray 
   (actually a line segment) defined by the points 'from' en 'to'. The spot is the hit point 
   on the object in local coordinates. 'normal' is the normal to the surface of the object in
   world coordinates. The ignore_client pointer is used to make one of the objects transparent.

   NB: Currently ray tests are implemented for spheres, boxes, and meshes only!!
*/   

	extern DECLSPEC void *DT_RayCast(DT_SceneHandle scene, void *ignore_client,
									 const DT_Vector3 source, const DT_Vector3 target,
									 DT_Scalar max_param, DT_Scalar *param, DT_Vector3 normal);

/* Similar, only here a single object is tested and a boolean is returned */

	extern DECLSPEC DT_Bool DT_ObjectRayCast(DT_ObjectHandle object,
											 const DT_Vector3 source, const DT_Vector3 target,
											 DT_Scalar max_param, DT_Scalar *param, DT_Vector3 normal);


#ifdef __cplusplus
}
#endif

#endif
