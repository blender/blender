/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 * Scenegraph controller for ipos.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(_WIN64)
typedef unsigned __int64 uint_ptr;
#else
typedef unsigned long uint_ptr;
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "KX_IPO_SGController.h"
#include "KX_ScalarInterpolator.h"
#include "KX_GameObject.h"
#include "KX_IPhysicsController.h"

// All objects should start on frame 1! Will we ever need an object to 
// start on another frame, the 1.0 should change.
KX_IpoSGController::KX_IpoSGController() 
: m_modify_position(false),
  m_modify_orientation(false),
  m_modify_scaling(false),
  m_ipo_as_force(false),
  m_force_ipo_acts_local(false),
  m_modified(true),
  m_ipotime(1.0)
{
	m_game_object = NULL;

}


void KX_IpoSGController::SetOption(
	int option,
	int value)
{
	switch (option) {
	case SG_CONTR_IPO_IPO_AS_FORCE:
		m_ipo_as_force = (value != 0);
		m_modified = true;
		break;
	case SG_CONTR_IPO_FORCES_ACT_LOCAL:
		m_force_ipo_acts_local = (value != 0);
		m_modified = true;
		break;
	default:
		; /* just ignore the rest */
	}
}

	void 
KX_IpoSGController::UpdateSumoReference(
	)
{
	if (m_game_object) {

	}
}

	void 
KX_IpoSGController::SetGameObject(
	KX_GameObject* go
	)
{
	m_game_object = go;
}



bool KX_IpoSGController::Update(double currentTime)
{
	if (m_modified)
	{
		T_InterpolatorList::iterator i;
		for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
			(*i)->Execute(m_ipotime);//currentTime);
		}
		
		SG_Spatial* ob = (SG_Spatial*)m_pObject;
		
		if (m_modify_position) {
			if (m_ipo_as_force) {
				
				if (m_game_object && ob) {
					m_game_object->GetPhysicsController()->ApplyForce(m_force_ipo_acts_local ?
						ob->GetWorldOrientation() * m_ipo_xform.GetPosition() :
						m_ipo_xform.GetPosition(), false);
				}

			} else {
				ob->SetLocalPosition(m_ipo_xform.GetPosition());
			}
		}
		if (m_modify_orientation) {
			if (m_ipo_as_force) {
				
				if (m_game_object && ob) {
					m_game_object->ApplyTorque(m_force_ipo_acts_local ?
						ob->GetWorldOrientation() * m_ipo_xform.GetEulerAngles() :
						m_ipo_xform.GetEulerAngles(), false);
				}

			} else {
				ob->SetLocalOrientation(MT_Matrix3x3(m_ipo_xform.GetEulerAngles()));
			}
		}
		if (m_modify_scaling)
			ob->SetLocalScale(m_ipo_xform.GetScaling());

		m_modified=false;
	}
	return false;
}


void KX_IpoSGController::AddInterpolator(KX_IInterpolator* interp)
{
	this->m_interpolators.push_back(interp);
}

SG_Controller*	KX_IpoSGController::GetReplica(class SG_Node* destnode)
{
	KX_IpoSGController* iporeplica = new KX_IpoSGController(*this);
	// clear object that ipo acts on in the replica.
	iporeplica->ClearObject();

	// dirty hack, ask Gino for a better solution in the ipo implementation
	// hacken en zagen, in what we call datahiding, not written for replication :(

	T_InterpolatorList oldlist = m_interpolators;
	iporeplica->m_interpolators.clear();

	T_InterpolatorList::iterator i;
	for (i = oldlist.begin(); !(i == oldlist.end()); ++i) {
		KX_ScalarInterpolator* copyipo = new KX_ScalarInterpolator(*((KX_ScalarInterpolator*)*i));
		iporeplica->AddInterpolator(copyipo);

		MT_Scalar* scaal = ((KX_ScalarInterpolator*)*i)->GetTarget();
		uint_ptr orgbase = (uint_ptr)&m_ipo_xform;
		uint_ptr orgloc = (uint_ptr)scaal;
		uint_ptr offset = orgloc-orgbase;
		uint_ptr newaddrbase = (uint_ptr)&iporeplica->m_ipo_xform;
		newaddrbase += offset;
		MT_Scalar* blaptr = (MT_Scalar*) newaddrbase;
		copyipo->SetNewTarget((MT_Scalar*)blaptr);
	}
	
	return iporeplica;
}

KX_IpoSGController::~KX_IpoSGController()
{

	T_InterpolatorList::iterator i;
	for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
		delete (*i);
	}
	
}
