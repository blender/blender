/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_MaterialIpoController.cpp
 *  \ingroup ketsji
 */

#include "KX_MaterialIpoController.h"
#include "KX_ScalarInterpolator.h"
#include "KX_GameObject.h"

#include "BLI_sys_types.h" // for intptr_t support

bool KX_MaterialIpoController::Update(double currentTime)
{
	if (m_modified)
	{
		T_InterpolatorList::iterator i;
		for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
			(*i)->Execute(m_ipotime);
		}
		

		SG_Spatial* ob = (SG_Spatial*)m_pObject;
		KX_GameObject* kxgameobj= (KX_GameObject*) ob->GetSGClientObject();

		//kxgameobj->SetObjectColor(m_rgba);
		kxgameobj->UpdateMaterialData( 
			m_matname_hash,
			m_rgba, 
			m_specrgb, 
			m_hard, 
			m_spec, 
			m_ref, 
			m_emit,
			m_alpha
		);

		m_modified=false;
	}
	return false;
}


void KX_MaterialIpoController::AddInterpolator(KX_IInterpolator* interp)
{
	this->m_interpolators.push_back(interp);
}

SG_Controller*	KX_MaterialIpoController::GetReplica(class SG_Node* destnode)
{
	KX_MaterialIpoController* iporeplica = new KX_MaterialIpoController(*this);
	// clear object that ipo acts on
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
		intptr_t orgbase = (intptr_t)this;
		intptr_t orgloc = (intptr_t)scaal;
		intptr_t offset = orgloc-orgbase;
		intptr_t newaddrbase = (intptr_t)iporeplica + offset;
		MT_Scalar* blaptr = (MT_Scalar*) newaddrbase;
		copyipo->SetNewTarget((MT_Scalar*)blaptr);
	}
	
	return iporeplica;
}

KX_MaterialIpoController::~KX_MaterialIpoController()
{

	T_InterpolatorList::iterator i;
	for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
		delete (*i);
	}
	
}

