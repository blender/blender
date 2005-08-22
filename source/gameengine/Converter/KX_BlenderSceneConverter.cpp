/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef WIN32
	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif

#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_IpoConvert.h"
#include "RAS_MeshObject.h"
#include "KX_PhysicsEngineEnums.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_KetsjiEngine.h"
#include "Object.h"
#include "KX_IPhysicsController.h"

#include "DummyPhysicsEnvironment.h"

//to decide to use sumo/ode or dummy physics - defines USE_ODE
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET
#include "CcdPhysicsEnvironment.h"
#endif

#ifdef USE_ODE
#include "OdePhysicsEnvironment.h"
#endif //USE_ODE

#ifdef USE_SUMO_SOLID
#include "SumoPhysicsEnvironment.h"
#endif

#include "KX_BlenderSceneConverter.h"
#include "KX_BlenderScalarInterpolator.h"
#include "BL_BlenderDataConversion.h"
#include "BlenderWorldInfo.h"
#include "KX_Scene.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "BKE_main.h"

#include "DNA_object_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"


KX_BlenderSceneConverter::KX_BlenderSceneConverter(
							struct Main* maggie,
							class KX_KetsjiEngine* engine
							)
							: m_maggie(maggie),
							m_ketsjiEngine(engine),
							m_alwaysUseExpandFraming(false)
{
	m_newfilename = "";
}


KX_BlenderSceneConverter::~KX_BlenderSceneConverter()
{
	// clears meshes, and hashmaps from blender to gameengine data
	int i;
	// delete sumoshapes
	

	int numipolists = m_map_blender_to_gameipolist.size();
	for (i=0; i<numipolists; i++) {
		BL_InterpolatorList *ipoList= *m_map_blender_to_gameipolist.at(i);

		delete (ipoList);
	}

	vector<KX_WorldInfo*>::iterator itw = m_worldinfos.begin();
	while (itw != m_worldinfos.end()) {
		delete (*itw);
		itw++;
	}

	vector<RAS_IPolyMaterial*>::iterator itp = m_polymaterials.begin();
	while (itp != m_polymaterials.end()) {
		delete (*itp);
		itp++;
	}
	
	vector<RAS_MeshObject*>::iterator itm = m_meshobjects.begin();
	while (itm != m_meshobjects.end()) {
		delete (*itm);
		itm++;
	}
	
#ifdef USE_SUMO_SOLID
	KX_ClearSumoSharedShapes();
#endif

#ifdef USE_BULLET
	KX_ClearBulletSharedShapes();
#endif

}



void KX_BlenderSceneConverter::SetNewFileName(const STR_String& filename)
{
	m_newfilename = filename;
}



bool KX_BlenderSceneConverter::TryAndLoadNewFile()
{
	bool result = false;

	// find the file
/*	if ()
	{
		result = true;
	}
	// if not, clear the newfilename
	else
	{
		m_newfilename = "";	
	}
*/
	return result;
}



	/**
	 * Find the specified scene by name, or the first
	 * scene if nothing matches (shouldn't happen).
	 */
static struct Scene *GetSceneForName2(struct Main *maggie, const STR_String& scenename) {
	Scene *sce;

	for (sce= (Scene*) maggie->scene.first; sce; sce= (Scene*) sce->id.next)
		if (scenename == (sce->id.name+2))
			return sce;

	return (Scene*) maggie->scene.first;
}
#include "KX_PythonInit.h"

#ifdef USE_BULLET

#include "IDebugDraw.h"


struct	BlenderDebugDraw : public IDebugDraw
{
	BlenderDebugDraw () :
		m_debugMode(0) 
	{
	}
	
	int m_debugMode;

	virtual void	DrawLine(const SimdVector3& from,const SimdVector3& to,const SimdVector3& color)
	{
		if (m_debugMode >0)
		{
			MT_Vector3 kxfrom(from[0],from[1],from[2]);
			MT_Vector3 kxto(to[0],to[1],to[2]);
			MT_Vector3 kxcolor(color[0],color[1],color[2]);

			KX_RasterizerDrawDebugLine(kxfrom,kxto,kxcolor);
		}
	}
	
	virtual void	DrawContactPoint(const SimdVector3& PointOnB,const SimdVector3& normalOnB,float distance,int lifeTime,const SimdVector3& color)
	{
		//not yet
	}

	virtual void	SetDebugMode(int debugMode)
	{
		m_debugMode = debugMode;
	}
	virtual int		GetDebugMode() const
	{
		return m_debugMode;
	}
		
};

#endif

void KX_BlenderSceneConverter::ConvertScene(const STR_String& scenename,
											class KX_Scene* destinationscene,
											PyObject* dictobj,
											class SCA_IInputDevice* keyinputdev,
											class RAS_IRenderTools* rendertools,
											class RAS_ICanvas* canvas)
{
	//find out which physics engine
	Scene *blenderscene = GetSceneForName2(m_maggie, scenename);

	e_PhysicsEngine physics_engine = UseSumo;

	if (blenderscene)
	{
	
		if (blenderscene->world)
		{
			switch (blenderscene->world->physicsEngine)
			{
			case WOPHY_BULLET:
				{
					physics_engine = UseBullet;
					break;
				}
                                
				case WOPHY_ODE:
				{
					physics_engine = UseODE;
					break;
				}
				case WOPHY_DYNAMO:
				{
					physics_engine = UseDynamo;
					break;
				}
				case WOPHY_SUMO:
				{
					physics_engine = UseSumo;
					break;
				}
				case WOPHY_NONE:
				{
					physics_engine = UseNone;
				}
			}
		  
		}
	}

	switch (physics_engine)
	{
#ifdef USE_BULLET
		case UseBullet:
			{
				CcdPhysicsEnvironment* ccdPhysEnv = new CcdPhysicsEnvironment();
				ccdPhysEnv->setDebugDrawer(new BlenderDebugDraw());
				//todo: get a button in blender ?
				//disable / enable debug drawing (contact points, aabb's etc)	
				//ccdPhysEnv->setDebugMode(1);
				destinationscene->SetPhysicsEnvironment(ccdPhysEnv);
				break;
			}
#endif

#ifdef USE_SUMO_SOLID
		case UseSumo:
			destinationscene ->SetPhysicsEnvironment(new SumoPhysicsEnvironment());
			break;
#endif
#ifdef USE_ODE

		case UseODE:
			destinationscene ->SetPhysicsEnvironment(new ODEPhysicsEnvironment());
			break;
#endif //USE_ODE
	
		case UseDynamo:
		{
		}
		
		default:
		case UseNone:
			physics_engine = UseNone;
			destinationscene ->SetPhysicsEnvironment(new DummyPhysicsEnvironment());
			break;
	}

	BL_ConvertBlenderObjects(m_maggie,
		scenename,
		destinationscene,
		m_ketsjiEngine,
		physics_engine,
		dictobj,
		keyinputdev,
		rendertools,
		canvas,
		this,
		m_alwaysUseExpandFraming
		);

	m_map_blender_to_gameactuator.clear();
	m_map_blender_to_gamecontroller.clear();
	
	m_map_blender_to_gameobject.clear();
	m_map_mesh_to_gamemesh.clear();

	//don't clear it yet, it is needed for the baking physics into ipo animation
	//m_map_gameobject_to_blender.clear();
}



void KX_BlenderSceneConverter::SetAlwaysUseExpandFraming(
	bool to_what)
{
	m_alwaysUseExpandFraming= to_what;
}

	

void KX_BlenderSceneConverter::RegisterGameObject(
									KX_GameObject *gameobject, 
									struct Object *for_blenderobject) 
{
	m_map_gameobject_to_blender.insert(CHashedPtr(gameobject),for_blenderobject);
	m_map_blender_to_gameobject.insert(CHashedPtr(for_blenderobject),gameobject);
}



KX_GameObject *KX_BlenderSceneConverter::FindGameObject(
									struct Object *for_blenderobject) 
{
	KX_GameObject **obp= m_map_blender_to_gameobject[CHashedPtr(for_blenderobject)];
	
	return obp?*obp:NULL;
}



struct Object *KX_BlenderSceneConverter::FindBlenderObject(
									KX_GameObject *for_gameobject) 
{
	struct Object **obp= m_map_gameobject_to_blender[CHashedPtr(for_gameobject)];
	
	return obp?*obp:NULL;
}

	

void KX_BlenderSceneConverter::RegisterGameMesh(
									RAS_MeshObject *gamemesh,
									struct Mesh *for_blendermesh)
{
	m_map_mesh_to_gamemesh.insert(CHashedPtr(for_blendermesh),gamemesh);
	m_meshobjects.push_back(gamemesh);
}



RAS_MeshObject *KX_BlenderSceneConverter::FindGameMesh(
									struct Mesh *for_blendermesh,
									unsigned int onlayer)
{
	RAS_MeshObject** meshp = m_map_mesh_to_gamemesh[CHashedPtr(for_blendermesh)];
	
	if (meshp && onlayer==(*meshp)->GetLightLayer()) {
		return *meshp;
	} else {
		return NULL;
	}
}

	


	

void KX_BlenderSceneConverter::RegisterPolyMaterial(RAS_IPolyMaterial *polymat)
{
	m_polymaterials.push_back(polymat);
}



void KX_BlenderSceneConverter::RegisterInterpolatorList(
									BL_InterpolatorList *ipoList,
									struct Ipo *for_ipo)
{
	m_map_blender_to_gameipolist.insert(CHashedPtr(for_ipo), ipoList);
}



BL_InterpolatorList *KX_BlenderSceneConverter::FindInterpolatorList(
									struct Ipo *for_ipo)
{
	BL_InterpolatorList **listp = m_map_blender_to_gameipolist[CHashedPtr(for_ipo)];
		
	return listp?*listp:NULL;
}



void KX_BlenderSceneConverter::RegisterGameActuator(
									SCA_IActuator *act,
									struct bActuator *for_actuator)
{
	m_map_blender_to_gameactuator.insert(CHashedPtr(for_actuator), act);
}



SCA_IActuator *KX_BlenderSceneConverter::FindGameActuator(
									struct bActuator *for_actuator)
{
	SCA_IActuator **actp = m_map_blender_to_gameactuator[CHashedPtr(for_actuator)];
	
	return actp?*actp:NULL;
}



void KX_BlenderSceneConverter::RegisterGameController(
									SCA_IController *cont,
									struct bController *for_controller)
{
	m_map_blender_to_gamecontroller.insert(CHashedPtr(for_controller), cont);
}



SCA_IController *KX_BlenderSceneConverter::FindGameController(
									struct bController *for_controller)
{
	SCA_IController **contp = m_map_blender_to_gamecontroller[CHashedPtr(for_controller)];
	
	return contp?*contp:NULL;
}



void KX_BlenderSceneConverter::RegisterWorldInfo(
									KX_WorldInfo *worldinfo)
{
	m_worldinfos.push_back(worldinfo);
}


void	KX_BlenderSceneConverter::ResetPhysicsObjectsAnimationIpo()
{
	//todo,before 2.38/2.40 release, Erwin

	KX_SceneList* scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i=0;i<numScenes;i++)
	{
		KX_Scene* scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue* parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g=0;g<numObjects;g++)
		{
			KX_GameObject* gameObj = (KX_GameObject*)parentList->GetValue(g);
			if (gameObj->IsDynamic())
			{
				//KX_IPhysicsController* physCtrl = gameObj->GetPhysicsController();
				
				Object* blenderObject = FindBlenderObject(gameObj);
				if (blenderObject)
				{
					//erase existing ipo's
					Ipo* ipo = blenderObject->ipo;
					if (ipo)
					{

						IpoCurve *icu;
						int numCurves = 0;
						for( icu = (IpoCurve*)ipo->curve.first; icu; icu = icu->next ) {
							numCurves++;
							
						}

					}
				}
			}

		}
		
	
	}



}

	///this generates ipo curves for position, rotation, allowing to use game physics in animation
void	KX_BlenderSceneConverter::WritePhysicsObjectToAnimationIpo(int frameNumber)
{
	//todo, before 2.38/2.40 release, Erwin
#ifdef TURN_THIS_PYTHON_CODE_INTO_CPP

						//all stuff needed to bake keyframes into blender objects
						//this allows physics simulation of the game engine to be automatically turned into blender ipo curves
						//so bullet physics can be used for animations



						static PyObject *Ipo_getCurve( BPy_Ipo * self, PyObject * args )
						{
							char *str, *str1;
							IpoCurve *icu = 0;

							if( !PyArg_ParseTuple( args, "s", &str ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected string argument" ) );

							for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
								str1 = getIpoCurveName( icu );
								if( !strcmp( str1, str ) )
									return IpoCurve_CreatePyObject( icu );
							}

							Py_INCREF( Py_None );
							return Py_None;
						}

						static PyObject *Ipo_addCurve( BPy_Ipo * self, PyObject * args )
						{
							int param = 0;		/* numeric curve name constant */
							int ok = 0;
							int ipofound = 0;
							char *cur_name = 0;	/* input arg: curve name */
							Ipo *ipo = 0;
							IpoCurve *icu = 0;
							Link *link;

							if( !PyArg_ParseTuple( args, "s", &cur_name ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected string argument" ) );


							/* chase down the ipo list looking for ours */
							link = G.main->ipo.first;

							while( link ) {
								ipo = ( Ipo * ) link;
								if( ipo == self->ipo ) {
									ipofound = 1;
									break;
								}
								link = link->next;
							}

							if( ipo && ipofound ) {
								/* ok.  continue */
							} else {		/* runtime error here:  our ipo not found */
								return ( EXPP_ReturnPyObjError
									( PyExc_RuntimeError, "Ipo not found" ) );
							}


							/*
							depending on the block type, 
							check if the input arg curve name is valid 
							and set param to numeric value.
							*/
							switch ( ipo->blocktype ) {
							case ID_OB:
								ok = Ipo_obIcuName( cur_name, &param );
								break;
							case ID_CA:
								ok = Ipo_caIcuName( cur_name, &param );
								break;
							case ID_LA:
								ok = Ipo_laIcuName( cur_name, &param );
								break;
							case ID_TE:
								ok = Ipo_texIcuName( cur_name, &param );
								break;
							case ID_WO:
								ok = Ipo_woIcuName( cur_name, &param );
								break;
							case ID_MA:
								ok = Ipo_maIcuName( cur_name, &param );
								break;
							case ID_AC:
								ok = Ipo_acIcuName( cur_name, &param );
								break;
							case IPO_CO:
								ok = Ipo_coIcuName( cur_name, &param );
								break;
							case ID_CU:
								ok = Ipo_cuIcuName( cur_name, &param );
								break;
							case ID_KE:
								ok = Ipo_keIcuName( cur_name, &param );
								break;
							case ID_SEQ:
								ok = Ipo_seqIcuName( cur_name, &param );
								break;
							default:
								ok = 0;
							}

							if( !ok )		/* curve type was invalid */
								return EXPP_ReturnPyObjError
									( PyExc_NameError, "curve name was invalid" );

							/* ask blender to create the new ipo curve */
							icu = get_ipocurve( NULL, ipo->blocktype, param, self->ipo );

							if( icu == 0 )		/* could not create curve */
								return EXPP_ReturnPyObjError
									( PyExc_RuntimeError,
									"blender could not create ipo curve" );

							allspace( REMAKEIPO, 0 );
							EXPP_allqueue( REDRAWIPO, 0 );

							/* create a bpy wrapper for the new ipo curve */
							return IpoCurve_CreatePyObject( icu );
						}

						static PyObject *Ipo_getNcurves( BPy_Ipo * self )
						{
							int i = 0;

							IpoCurve *icu;
							for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
								i++;
							}

							return ( PyInt_FromLong( i ) );
						}


						static PyObject *Ipo_getNBezPoints( BPy_Ipo * self, PyObject * args )
						{
							int num = 0, i = 0;
							IpoCurve *icu = 0;
							if( !PyArg_ParseTuple( args, "i", &num ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected int argument" ) );
							icu = self->ipo->curve.first;
							if( !icu )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "No IPO curve" ) );
							for( i = 0; i < num; i++ ) {
								if( !icu )
									return ( EXPP_ReturnPyObjError
										( PyExc_TypeError, "Bad curve number" ) );
								icu = icu->next;

							}
							return ( PyInt_FromLong( icu->totvert ) );
						}

						static PyObject *Ipo_DeleteBezPoints( BPy_Ipo * self, PyObject * args )
						{
							int num = 0, i = 0;
							IpoCurve *icu = 0;
							if( !PyArg_ParseTuple( args, "i", &num ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected int argument" ) );
							icu = self->ipo->curve.first;
							if( !icu )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "No IPO curve" ) );
							for( i = 0; i < num; i++ ) {
								if( !icu )
									return ( EXPP_ReturnPyObjError
										( PyExc_TypeError, "Bad curve number" ) );
								icu = icu->next;

							}
							icu->totvert--;
							return ( PyInt_FromLong( icu->totvert ) );
						}

						/*****************************************************************************/
						/* Function:	  M_Object_Get						*/
						/* Python equivalent:	  Blender.Object.Get				*/
						/*****************************************************************************/
						PyObject *M_Object_Get( PyObject * self, PyObject * args )
						{
							struct Object *object;
							BPy_Object *blen_object;
							char *name = NULL;

							PyArg_ParseTuple( args, "|s", &name );

							if( name != NULL ) {
								object = GetObjectByName( name );

								if( object == NULL ) {
									/* No object exists with the name specified in the argument name. */
									return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
													"Unknown object specified." ) );
								}
								blen_object =
									( BPy_Object * ) PyObject_NEW( BPy_Object,
													&Object_Type );
								blen_object->object = object;

								return ( ( PyObject * ) blen_object );
							} else {
								/* No argument has been given. Return a list of all objects. */
								PyObject *obj_list;
								Link *link;
								int index;

								obj_list = PyList_New( BLI_countlist( &( G.main->object ) ) );

								if( obj_list == NULL ) {
									return ( EXPP_ReturnPyObjError( PyExc_SystemError,
													"List creation failed." ) );
								}

								link = G.main->object.first;
								index = 0;
								while( link ) {
									object = ( Object * ) link;
									blen_object =
										( BPy_Object * ) PyObject_NEW( BPy_Object,
														&Object_Type );
									blen_object->object = object;

									PyList_SetItem( obj_list, index,
											( PyObject * ) blen_object );
									index++;
									link = link->next;
								}
								return ( obj_list );
							}
						}


						static PyObject *M_Ipo_New( PyObject * self, PyObject * args )
						{
							Ipo *add_ipo( char *name, int idcode );
							char *name = NULL, *code = NULL;
							int idcode = -1;
							BPy_Ipo *pyipo;
							Ipo *blipo;

							if( !PyArg_ParseTuple( args, "ss", &code, &name ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError,
									"expected string string arguments" ) );

							if( !strcmp( code, "Object" ) )
								idcode = ID_OB;
							if( !strcmp( code, "Camera" ) )
								idcode = ID_CA;
							if( !strcmp( code, "World" ) )
								idcode = ID_WO;
							if( !strcmp( code, "Material" ) )
								idcode = ID_MA;
							if( !strcmp( code, "Texture" ) )
								idcode = ID_TE;
							if( !strcmp( code, "Lamp" ) )
								idcode = ID_LA;
							if( !strcmp( code, "Action" ) )
								idcode = ID_AC;
							if( !strcmp( code, "Constraint" ) )
								idcode = IPO_CO;
							if( !strcmp( code, "Sequence" ) )
								idcode = ID_SEQ;
							if( !strcmp( code, "Curve" ) )
								idcode = ID_CU;
							if( !strcmp( code, "Key" ) )
								idcode = ID_KE;

							if( idcode == -1 )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "Bad code" ) );


							blipo = add_ipo( name, idcode );

							if( blipo ) {
								/* return user count to zero because add_ipo() inc'd it */
								blipo->id.us = 0;
								/* create python wrapper obj */
								pyipo = ( BPy_Ipo * ) PyObject_NEW( BPy_Ipo, &Ipo_Type );
							} else
								return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
												"couldn't create Ipo Data in Blender" ) );

							if( pyipo == NULL )
								return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
												"couldn't create Ipo Data object" ) );

							pyipo->ipo = blipo;

							return ( PyObject * ) pyipo;
						}



						static PyObject *Object_setIpo( BPy_Object * self, PyObject * args )
						{
							PyObject *pyipo = 0;
							Ipo *ipo = NULL;
							Ipo *oldipo;

							if( !PyArg_ParseTuple( args, "O!", &Ipo_Type, &pyipo ) )
								return EXPP_ReturnPyObjError( PyExc_TypeError,
												"expected Ipo as argument" );

							ipo = Ipo_FromPyObject( pyipo );

							if( !ipo )
								return EXPP_ReturnPyObjError( PyExc_RuntimeError,
												"null ipo!" );

							if( ipo->blocktype != ID_OB )
								return EXPP_ReturnPyObjError( PyExc_TypeError,
												"this ipo is not an object ipo" );

							oldipo = self->object->ipo;
							if( oldipo ) {
								ID *id = &oldipo->id;
								if( id->us > 0 )
									id->us--;
							}

							( ( ID * ) & ipo->id )->us++;

							self->object->ipo = ipo;

							Py_INCREF( Py_None );
							return Py_None;
						}


						static PyObject *M_Ipo_Get( PyObject * self, PyObject * args )
						{
							char *name = NULL;
							Ipo *ipo_iter;
							PyObject *ipolist, *pyobj;
							BPy_Ipo *wanted_ipo = NULL;
							char error_msg[64];

							if( !PyArg_ParseTuple( args, "|s", &name ) )
								return ( EXPP_ReturnPyObjError( PyExc_TypeError,
												"expected string argument (or nothing)" ) );

							ipo_iter = G.main->ipo.first;

							if( name ) {		/* (name) - Search ipo by name */
								while( ( ipo_iter ) && ( wanted_ipo == NULL ) ) {
									if( strcmp( name, ipo_iter->id.name + 2 ) == 0 ) {
										wanted_ipo =
											( BPy_Ipo * ) PyObject_NEW( BPy_Ipo,
															&Ipo_Type );
										if( wanted_ipo )
											wanted_ipo->ipo = ipo_iter;
									}
									ipo_iter = ipo_iter->id.next;
								}

								if( wanted_ipo == NULL ) {	/* Requested ipo doesn't exist */
									PyOS_snprintf( error_msg, sizeof( error_msg ),
											"Ipo \"%s\" not found", name );
									return ( EXPP_ReturnPyObjError
										( PyExc_NameError, error_msg ) );
								}

								return ( PyObject * ) wanted_ipo;
							}

							else {			/* () - return a list with all ipos in the scene */
								int index = 0;

								ipolist = PyList_New( BLI_countlist( &( G.main->ipo ) ) );

								if( ipolist == NULL )
									return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
													"couldn't create PyList" ) );

								while( ipo_iter ) {
									pyobj = Ipo_CreatePyObject( ipo_iter );

									if( !pyobj )
										return ( EXPP_ReturnPyObjError
											( PyExc_MemoryError,
											"couldn't create PyString" ) );

									PyList_SET_ITEM( ipolist, index, pyobj );

									ipo_iter = ipo_iter->id.next;
									index++;
								}

								return ( ipolist );
							}
						}



						static PyObject *M_Ipo_Recalc( PyObject * self, PyObject * args )
						{
							void testhandles_ipocurve( IpoCurve * icu );
							PyObject *a;
							IpoCurve *icu;
							if( !PyArg_ParseTuple( args, "O", &a ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected ipo argument)" ) );
							icu = IpoCurve_FromPyObject( a );
							testhandles_ipocurve( icu );

							Py_INCREF( Py_None );
							return Py_None;

						}


						static PyObject *IpoCurve_addBezier( C_IpoCurve * self, PyObject * args )
						{
							short MEM_freeN( void *vmemh );
							void *MEM_mallocN( unsigned int len, char *str );
							float x, y;
							int npoints;
							IpoCurve *icu;
							BezTriple *bzt, *tmp;
							static char name[10] = "mlml";
							PyObject *popo = 0;
							if( !PyArg_ParseTuple( args, "O", &popo ) )
								return ( EXPP_ReturnPyObjError
									( PyExc_TypeError, "expected tuple argument" ) );

							x = (float)PyFloat_AsDouble( PyTuple_GetItem( popo, 0 ) );
							y = (float)PyFloat_AsDouble( PyTuple_GetItem( popo, 1 ) );
							icu = self->ipocurve;
							npoints = icu->totvert;
							tmp = icu->bezt;
							icu->bezt = MEM_mallocN( sizeof( BezTriple ) * ( npoints + 1 ), name );
							if( tmp ) {
								memmove( icu->bezt, tmp, sizeof( BezTriple ) * npoints );
								MEM_freeN( tmp );
							}
							memmove( icu->bezt + npoints, icu->bezt, sizeof( BezTriple ) );
							icu->totvert++;
							bzt = icu->bezt + npoints;
							bzt->vec[0][0] = x - 1;
							bzt->vec[1][0] = x;
							bzt->vec[2][0] = x + 1;
							bzt->vec[0][1] = y - 1;
							bzt->vec[1][1] = y;
							bzt->vec[2][1] = y + 1;
							/* set handle type to Auto */
							bzt->h1 = HD_AUTO;
							bzt->h2 = HD_AUTO;

							Py_INCREF( Py_None );
							return Py_None;
						}

#endif
}

