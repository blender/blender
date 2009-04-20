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
 */
#ifndef __KX_ICONTROLLER
#define __KX_ICONTROLLER

#include "SCA_ILogicBrick.h"

class SCA_IController : public SCA_ILogicBrick
{
protected:
	std::vector<class SCA_ISensor*>		m_linkedsensors;
	std::vector<class SCA_IActuator*>	m_linkedactuators;
	unsigned int						m_statemask;
public:
	SCA_IController(SCA_IObject* gameobj,PyTypeObject* T);
	virtual ~SCA_IController();
	virtual void Trigger(class SCA_LogicManager* logicmgr)=0;
	void	LinkToSensor(SCA_ISensor* sensor);
	void	LinkToActuator(SCA_IActuator*);
	const std::vector<class SCA_ISensor*>&		GetLinkedSensors();
	const std::vector<class SCA_IActuator*>&	GetLinkedActuators();
	void	UnlinkAllSensors();
	void	UnlinkAllActuators();
	void	UnlinkActuator(class SCA_IActuator* actua);
	void	UnlinkSensor(class SCA_ISensor* sensor);
	void    SetState(unsigned int state) { m_statemask = state; }
	void    ApplyState(unsigned int state);


};

#endif

