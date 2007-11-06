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


#include "SOLID.h"

#include "DT_Box.h"
#include "DT_Cone.h"
#include "DT_Cylinder.h"
#include "DT_Sphere.h"
#include "DT_Complex.h"
#include "DT_Polytope.h"
#include "DT_Polyhedron.h"
#include "DT_Point.h"
#include "DT_LineSegment.h"
#include "DT_Triangle.h"
#include "DT_Minkowski.h"
#include "DT_Hull.h"

#include "DT_Response.h"
#include "DT_RespTable.h"

#include "DT_Scene.h"
#include "DT_Object.h"

#include "DT_VertexBase.h"

#include "DT_Accuracy.h"

typedef MT::Tuple3<DT_Scalar> T_Vertex;
typedef std::vector<T_Vertex> T_VertexBuf;
typedef std::vector<DT_Index> T_IndexBuf;
typedef std::vector<const DT_Convex *> T_PolyList;

static T_VertexBuf vertexBuf;
static T_IndexBuf indexBuf;
static T_PolyList polyList; 

static DT_Complex       *currentComplex    = 0;
static DT_Polyhedron    *currentPolyhedron = 0;
static DT_VertexBase    *currentBase = 0;





		
DT_VertexBaseHandle DT_NewVertexBase(const void *pointer, DT_Size stride) 
{
    return (DT_VertexBaseHandle)new DT_VertexBase(pointer, stride);
}

void DT_DeleteVertexBase(DT_VertexBaseHandle vertexBase) 
{ 
    delete (DT_VertexBase *)vertexBase; 
}

void DT_ChangeVertexBase(DT_VertexBaseHandle vertexBase, const void *pointer) 
{ 
	DT_VertexBase *base = (DT_VertexBase *)vertexBase;
	base->setPointer(pointer);
	const DT_ComplexList& complexList = base->getComplexList();
	DT_ComplexList::const_iterator it;
	for (it = complexList.begin(); it != complexList.end(); ++it)
	{
		(*it)->refit();
	}
}


DT_ShapeHandle DT_NewBox(DT_Scalar x, DT_Scalar y, DT_Scalar z) 
{
    return (DT_ShapeHandle)new DT_Box(MT_Scalar(x) * MT_Scalar(0.5), 
									  MT_Scalar(y) * MT_Scalar(0.5), 
									  MT_Scalar(z) * MT_Scalar(0.5));
}

DT_ShapeHandle DT_NewCone(DT_Scalar radius, DT_Scalar height)
{
    return (DT_ShapeHandle)new DT_Cone(MT_Scalar(radius), MT_Scalar(height));
}

DT_ShapeHandle DT_NewCylinder(DT_Scalar radius, DT_Scalar height) 
{
    return (DT_ShapeHandle)new DT_Cylinder(MT_Scalar(radius), MT_Scalar(height));
}

DT_ShapeHandle DT_NewSphere(DT_Scalar radius) 
{
    return (DT_ShapeHandle)new DT_Sphere(MT_Scalar(radius));
}

DT_ShapeHandle DT_NewPoint(const DT_Vector3 point) 
{
	return (DT_ShapeHandle)new DT_Point(MT_Point3(point));
}

DT_ShapeHandle DT_NewLineSegment(const DT_Vector3 source, const DT_Vector3 target) 
{
	return (DT_ShapeHandle)new DT_LineSegment(MT_Point3(source), MT_Point3(target));
}

DT_ShapeHandle DT_NewMinkowski(DT_ShapeHandle shape1, DT_ShapeHandle shape2) 
{
	if (((DT_Shape *)shape1)->getType() != CONVEX ||
		((DT_Shape *)shape2)->getType() != CONVEX) 
	{
		return 0;
	}

	return (DT_ShapeHandle)new DT_Minkowski(*(DT_Convex *)shape1, *(DT_Convex *)shape2);
}
	
DT_ShapeHandle DT_NewHull(DT_ShapeHandle shape1, DT_ShapeHandle shape2)
{
	if (((DT_Shape *)shape1)->getType() != CONVEX ||
		((DT_Shape *)shape2)->getType() != CONVEX) 
	{
		return 0;
	}

	return (DT_ShapeHandle)new DT_Hull(*(DT_Convex *)shape1, *(DT_Convex *)shape2);
}

DT_ShapeHandle DT_NewComplexShape(const DT_VertexBaseHandle vertexBase) 
{
    if (!currentComplex) 
	{
		currentBase = vertexBase ? (DT_VertexBase *)vertexBase : new DT_VertexBase;
		currentComplex = new DT_Complex(currentBase);
	}
    return (DT_ShapeHandle)currentComplex;
}

void DT_EndComplexShape() 
{
    if (currentComplex) 
	{
        if (currentBase->getPointer() == 0) 
		{
            T_Vertex *vertexArray = new T_Vertex[vertexBuf.size()];   
			assert(vertexArray);	
            std::copy(vertexBuf.begin(), vertexBuf.end(), &vertexArray[0]);
            currentBase->setPointer(vertexArray, true);		
        }
		
		vertexBuf.clear();
        
        currentComplex->finish(polyList.size(), &polyList[0]);
        polyList.clear();
        currentComplex = 0;
        currentBase = 0; 
    }
}

DT_ShapeHandle DT_NewPolytope(const DT_VertexBaseHandle vertexBase) 
{
    if (!currentPolyhedron) 
	{
		currentBase = vertexBase ? (DT_VertexBase *)vertexBase : new DT_VertexBase;
        currentPolyhedron = new DT_Polyhedron;
		
    }
    return (DT_ShapeHandle)currentPolyhedron;
}

void DT_EndPolytope() 
{
    if (currentPolyhedron) 
	{
        if (currentBase->getPointer() == 0) 
		{
			currentBase->setPointer(&vertexBuf[0]);		
			new (currentPolyhedron) DT_Polyhedron(currentBase, indexBuf.size(), &indexBuf[0]);
			
			delete currentBase;
		}
		else
		{
			new (currentPolyhedron) DT_Polyhedron(currentBase, indexBuf.size(), &indexBuf[0]);
		}
		vertexBuf.clear();
        indexBuf.clear();
        currentPolyhedron = 0;
        currentBase = 0;
    }
}

void DT_Begin() 
{}

void DT_End() 
{ 
	if (currentComplex) 
	{
		DT_VertexIndices(indexBuf.size(), &indexBuf[0]);
		indexBuf.clear();
	}
}

void DT_Vertex(const DT_Vector3 vertex)
{
    MT::Vector3<DT_Scalar> p(vertex);
    int i = GEN_max((int)vertexBuf.size() - 20, 0);
	int n = static_cast<int>(vertexBuf.size());
	
    while (i != n  && !(vertexBuf[i] == p)) 
	{
		++i;
	}

    if (i == n) 
	{
		vertexBuf.push_back(p);
	}
    indexBuf.push_back(i);
}


void DT_VertexIndex(DT_Index index) { indexBuf.push_back(index); }

void DT_VertexIndices(DT_Count count, const DT_Index *indices) 
{
    if (currentComplex) 
	{
		DT_Convex *poly = count == 3 ? 
			              static_cast<DT_Convex *>(new DT_Triangle(currentBase, indices[0], indices[1], indices[2])) :
						  static_cast<DT_Convex *>(new DT_Polytope(currentBase, count, indices));  
		polyList.push_back(poly);
      
    }

    if (currentPolyhedron) 
	{
		int i;
		for (i = 0; i < count; ++i) 
		{
            indexBuf.push_back(indices[i]);
        }
    }   
}

void DT_VertexRange(DT_Index first, DT_Count count) 
{
    DT_Index *indices = new DT_Index[count];
    
	DT_Index i;
    for (i = 0; i != count; ++i) 
	{
        indices[i] = first + i;
    }
    DT_VertexIndices(count, indices);

    delete [] indices;	
}

void DT_DeleteShape(DT_ShapeHandle shape) 
{ 
    delete (DT_Shape *)shape; 
}




// Scene


DT_SceneHandle DT_CreateScene() 
{
    return (DT_SceneHandle)new DT_Scene; 
}

void DT_DestroyScene(DT_SceneHandle scene) 
{
    delete (DT_Scene *)scene;
}

void DT_AddObject(DT_SceneHandle scene, DT_ObjectHandle object) 
{
    assert(scene);
    assert(object);
    ((DT_Scene *)scene)->addObject(*(DT_Object *)object);
}

void DT_RemoveObject(DT_SceneHandle scene, DT_ObjectHandle object) 
{
    assert(scene);
    assert(object);
    ((DT_Scene *)scene)->removeObject(*(DT_Object *)object);
}


// Object instantiation


DT_ObjectHandle DT_CreateObject(void *client_object,
                                DT_ShapeHandle shape)
{
	return (DT_ObjectHandle)new DT_Object(client_object, *(DT_Shape *)shape);
}

void DT_DestroyObject(DT_ObjectHandle object) 
{
    delete (DT_Object *)object;
}

void DT_SetMargin(DT_ObjectHandle object, DT_Scalar margin) 
{
    ((DT_Object *)object)->setMargin(MT_Scalar(margin));
}


void DT_SetScaling(DT_ObjectHandle object, const DT_Vector3 scaling) 
{
    ((DT_Object *)object)->setScaling(MT_Vector3(scaling));
}

void DT_SetPosition(DT_ObjectHandle object, const DT_Vector3 position) 
{
    ((DT_Object *)object)->setPosition(MT_Point3(position));
}

void DT_SetOrientation(DT_ObjectHandle object, const DT_Quaternion orientation) 
{
    ((DT_Object *)object)->setOrientation(MT_Quaternion(orientation));   
}


void DT_SetMatrixf(DT_ObjectHandle object, const float *m) 
{
    ((DT_Object *)object)->setMatrix(m);
}

void DT_GetMatrixf(DT_ObjectHandle object, float *m) 
{
    ((DT_Object *)object)->getMatrix(m);
}

void DT_SetMatrixd(DT_ObjectHandle object, const double *m) 
{
    ((DT_Object *)object)->setMatrix(m);
}
void DT_GetMatrixd(DT_ObjectHandle object, double *m) 
{
    ((DT_Object *)object)->getMatrix(m);
}

void DT_GetBBox(DT_ObjectHandle object, DT_Vector3 min, DT_Vector3 max) 
{
	const MT_BBox& bbox = ((DT_Object *)object)->getBBox();
	bbox.getMin().getValue(min);
	bbox.getMax().getValue(max);
}

DT_Bool DT_GetIntersect(DT_ObjectHandle object1, DT_ObjectHandle object2, DT_Vector3 vec)
{
	MT_Vector3 v;
	DT_Bool result = intersect(*(DT_Object*)object1, *(DT_Object*)object2, v);
	v.getValue(vec);
	return result;
}

DT_Scalar DT_GetClosestPair(DT_ObjectHandle object1, DT_ObjectHandle object2,
							DT_Vector3 point1, DT_Vector3 point2) 
{
    MT_Point3 p1, p2;
    
    MT_Scalar result = closest_points(*(DT_Object *)object1, 
									  *(DT_Object *)object2,
									  p1, p2);
	p1.getValue(point1);
	p2.getValue(point2);

    return MT_sqrt(result);
}

DT_Bool DT_GetCommonPoint(DT_ObjectHandle object1, DT_ObjectHandle object2,
						  DT_Vector3 point) 
{
    MT_Point3   p1, p2;
	MT_Vector3  v(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0)); 
    
    bool result = common_point(*(DT_Object *)object1, *(DT_Object *)object2, v, p1, p2);

	if (result) 
	{
		p1.getValue(point);
	}

    return result;
}

DT_Bool DT_GetPenDepth(DT_ObjectHandle object1, DT_ObjectHandle object2,
				    DT_Vector3 point1, DT_Vector3 point2) 
{
    MT_Point3   p1, p2;
	MT_Vector3  v(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0)); 
    
    bool result = penetration_depth(*(DT_Object *)object1, *(DT_Object *)object2, v, p1, p2);

	if (result) 
	{
		p1.getValue(point1);
		p2.getValue(point2);
	}

    return result;
}

// Response

DT_RespTableHandle DT_CreateRespTable() 
{
    return (DT_RespTableHandle)new DT_RespTable;
}    

void DT_DestroyRespTable(DT_RespTableHandle respTable) 
{
    delete (DT_RespTable *)respTable;
}

DT_ResponseClass DT_GenResponseClass(DT_RespTableHandle respTable) 
{
	return ((DT_RespTable *)respTable)->genResponseClass();
}

void DT_SetResponseClass(DT_RespTableHandle respTable, DT_ObjectHandle object,
						 DT_ResponseClass responseClass)
{
	((DT_RespTable *)respTable)->setResponseClass(object, responseClass);
}

void DT_ClearResponseClass(DT_RespTableHandle respTable, 
						   DT_ObjectHandle object)
{
	((DT_RespTable *)respTable)->clearResponseClass(object);
}

void DT_CallResponse(DT_RespTableHandle respTable,
					 DT_ObjectHandle object1,
					 DT_ObjectHandle object2,
					 const DT_CollData *coll_data)
{
	const DT_ResponseList& responseList =
		((DT_RespTable *)respTable)->find(object1, object2);
	
	if (responseList.getType() != DT_NO_RESPONSE) 
	{
		responseList(((DT_Object *)object1)->getClientObject(), 
					 ((DT_Object *)object2)->getClientObject(),
					 coll_data);
	}
}


void DT_AddDefaultResponse(DT_RespTableHandle respTable,
                           DT_ResponseCallback response, 
						   DT_ResponseType type, void *client_data)
{
    ((DT_RespTable *)respTable)->addDefault(DT_Response(response, type, client_data));
}

void DT_RemoveDefaultResponse(DT_RespTableHandle respTable,
							  DT_ResponseCallback response)
{
      ((DT_RespTable *)respTable)->removeDefault(DT_Response(response));
}

void DT_AddClassResponse(DT_RespTableHandle respTable,
						 DT_ResponseClass responseClass, 
						 DT_ResponseCallback response, 
						 DT_ResponseType type, void *client_data)
{
    ((DT_RespTable *)respTable)->addSingle(responseClass, 
										   DT_Response(response, type, client_data));
}

void DT_RemoveClassResponse(DT_RespTableHandle respTable,
							DT_ResponseClass responseClass, 
							DT_ResponseCallback response) 
{
    ((DT_RespTable *)respTable)->removeSingle(responseClass, 
											  DT_Response(response));
}

void DT_AddPairResponse(DT_RespTableHandle respTable,
                        DT_ResponseClass responseClass1, 
						DT_ResponseClass responseClass2, 
                        DT_ResponseCallback response,
						DT_ResponseType type, void *client_data)
{
    ((DT_RespTable *)respTable)->addPair(responseClass1, responseClass2, 
										 DT_Response(response, type, client_data));
}

void DT_RemovePairResponse(DT_RespTableHandle respTable,
						   DT_ResponseClass responseClass1, 
						   DT_ResponseClass responseClass2, 
						   DT_ResponseCallback response)
{
    ((DT_RespTable *)respTable)->removePair(responseClass1, responseClass2, 
											DT_Response(response));
}


// Runtime

void DT_SetAccuracy(DT_Scalar max_error) 
{ 
	if (max_error > MT_Scalar(0.0)) 
	{
		DT_Accuracy::setAccuracy(MT_Scalar(max_error)); 
	}
}

void DT_SetTolerance(DT_Scalar tol_error) 
{ 
	if (tol_error > MT_Scalar(0.0)) 
	{
		DT_Accuracy::setTolerance(MT_Scalar(tol_error)); 
	}
}

DT_Count DT_Test(DT_SceneHandle scene, DT_RespTableHandle respTable) 
{ 
    return ((DT_Scene *)scene)->handleCollisions((DT_RespTable *)respTable);
}

void *DT_RayCast(DT_SceneHandle scene, void *ignore_client,
				 const DT_Vector3 source, const DT_Vector3 target,
				 DT_Scalar max_param, DT_Scalar *param, DT_Vector3 normal) 
{
	DT_Scalar  lambda = max_param;

	void *client_object = ((DT_Scene *)scene)->rayCast(ignore_client, source, target, 
													   lambda, normal);
   if (client_object)
   {
      *param = lambda;
   }
	return client_object;
}

DT_Bool DT_ObjectRayCast(DT_ObjectHandle object,
	   				     const DT_Vector3 source, const DT_Vector3 target,
					     DT_Scalar max_param, DT_Scalar *param, DT_Vector3 hit_normal) 
{
	MT_Scalar lambda = MT_Scalar(max_param);
	MT_Vector3 normal;  

	bool result = ((DT_Object *)object)->ray_cast(MT_Point3(source), MT_Point3(target), 
												  lambda, normal);

	if (result) 
	{
		*param = lambda;
		normal.getValue(hit_normal);
	}
	return result;
}

