/**
 * $Id$
 *
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
#ifndef KX_CONVERTPHYSICSOBJECTS
#define KX_CONVERTPHYSICSOBJECTS




//#define USE_SUMO_SOLID
//solid is not available yet

#define USE_ODE


class RAS_MeshObject;
class KX_Scene;


struct KX_Bounds
{
	float m_center[3];
	float m_extends[3];
};

struct KX_ObjectProperties
{
	bool	m_dyna;
	double m_radius;
	bool	m_angular_rigidbody;
	bool	m_in_active_layer;
	bool	m_ghost;
	class KX_GameObject*	m_dynamic_parent;
	bool	m_isactor;
	bool	m_concave;
	bool	m_isdeformable;
	bool	m_implicitsphere ;
	bool	m_implicitbox;
	KX_Bounds	m_boundingbox;
};

#ifdef USE_ODE


void	KX_ConvertODEEngineObject(KX_GameObject* gameobj,
							 RAS_MeshObject* meshobj,
							 KX_Scene* kxscene,
							struct	PHY_ShapeProps* shapeprops,
							struct	PHY_MaterialProps*	smmaterial,
							struct	KX_ObjectProperties*	objprop);


#endif //USE_ODE


void	KX_ConvertDynamoObject(KX_GameObject* gameobj,
							 RAS_MeshObject* meshobj,
							 KX_Scene* kxscene,
							struct	PHY_ShapeProps* shapeprops,
							struct	PHY_MaterialProps*	smmaterial,
							struct	KX_ObjectProperties*	objprop);



#ifdef USE_SUMO_SOLID





void	KX_ConvertSumoObject(	class	KX_GameObject* gameobj,
							class	RAS_MeshObject* meshobj,
							class	KX_Scene* kxscene,
							struct	PHY_ShapeProps* shapeprops,
							struct	PHY_MaterialProps*	smmaterial,
							struct	KX_ObjectProperties*	objprop);
#endif



#endif //KX_CONVERTPHYSICSOBJECTS
