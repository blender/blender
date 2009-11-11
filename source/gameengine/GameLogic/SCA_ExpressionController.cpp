/**
 * 'Expression Controller enables to calculate an expression that wires inputs to output
 *
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

#include "SCA_ExpressionController.h"
#include "SCA_ISensor.h"
#include "SCA_LogicManager.h"
#include "BoolValue.h"
#include "InputParser.h"
#include "MT_Transform.h" // for fuzzyZero

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_ExpressionController::SCA_ExpressionController(SCA_IObject* gameobj,
												   const STR_String& exprtext)
	:SCA_IController(gameobj),
	m_exprText(exprtext),
	m_exprCache(NULL)
{
}



SCA_ExpressionController::~SCA_ExpressionController()
{
	if (m_exprCache)
		m_exprCache->Release();
}



CValue* SCA_ExpressionController::GetReplica()
{
	SCA_ExpressionController* replica = new SCA_ExpressionController(*this);
	replica->m_exprText = m_exprText;
	replica->m_exprCache = NULL;
	// this will copy properties and so on...
	replica->ProcessReplica();

	return replica;
}


// Forced deletion of precalculated expression to break reference loop
// Use this function when you know that you won't use the sensor anymore
void SCA_ExpressionController::Delete()
{
	if (m_exprCache)
	{
		m_exprCache->Release();
		m_exprCache = NULL;
	}
	Release();
}


void SCA_ExpressionController::Trigger(SCA_LogicManager* logicmgr)
{

	bool expressionresult = false;
	if (!m_exprCache)
	{
		CParser parser;
		parser.SetContext(this->AddRef());
		m_exprCache = parser.ProcessText(m_exprText);
	}
	if (m_exprCache)
	{
		CValue* value = m_exprCache->Calculate();
		if (value)
		{
			if (value->IsError())
			{
				printf("%s\n", value->GetText().ReadPtr());
			} else
			{
				float num = (float)value->GetNumber();
				expressionresult = !MT_fuzzyZero(num);
			}
			value->Release();

		}
	}

	for (vector<SCA_IActuator*>::const_iterator i=m_linkedactuators.begin();
	!(i==m_linkedactuators.end());i++)
	{
		SCA_IActuator* actua = *i;
		logicmgr->AddActiveActuator(actua,expressionresult);
	}
}



CValue* SCA_ExpressionController::FindIdentifier(const STR_String& identifiername)
{

	CValue* identifierval = NULL;

	for (vector<SCA_ISensor*>::const_iterator is=m_linkedsensors.begin();
	!(is==m_linkedsensors.end());is++)
	{
		SCA_ISensor* sensor = *is;
		if (sensor->GetName() == identifiername)
		{
			identifierval = new CBoolValue(sensor->GetState());
			//identifierval = sensor->AddRef();
		}

		//if (!sensor->IsPositiveTrigger())
		//{
		//	sensorresult = false;
		//	break;
		//}
	}

	if (identifierval)
		return identifierval;

	return  GetParent()->FindIdentifier(identifiername);

}
