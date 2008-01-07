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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_ExpressionController::SCA_ExpressionController(SCA_IObject* gameobj,
												   const STR_String& exprtext,
												   PyTypeObject* T)
	:SCA_IController(gameobj,T),
	m_exprText(exprtext)
{
}



SCA_ExpressionController::~SCA_ExpressionController()
{
}



CValue* SCA_ExpressionController::GetReplica()
{
	SCA_ExpressionController* replica = new SCA_ExpressionController(*this);
	replica->m_exprText = m_exprText;
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



void SCA_ExpressionController::Trigger(SCA_LogicManager* logicmgr)
{

	bool expressionresult = false;

	CParser parser;
	parser.SetContext(this->AddRef());
	CExpression* expr = parser.ProcessText(m_exprText);
	if (expr)
	{
		CValue* value = expr->Calculate();
		if (value)
		{
			if (value->IsError())
			{
				printf(value->GetText());
			} else
			{
				float num = value->GetNumber();
				expressionresult = !MT_fuzzyZero(num);
			}
			value->Release();

		}
		expr->Release();
	}

	/*

	for (vector<SCA_ISensor*>::const_iterator is=m_linkedsensors.begin();
	!(is==m_linkedsensors.end());is++)
	{
		SCA_ISensor* sensor = *is;
		if (!sensor->IsPositiveTrigger())
		{
			sensorresult = false;
			break;
		}
	}
	
	  */
	
	CValue* newevent = new CBoolValue(expressionresult);

	for (vector<SCA_IActuator*>::const_iterator i=m_linkedactuators.begin();
	!(i==m_linkedactuators.end());i++)
	{
		SCA_IActuator* actua = *i;
		logicmgr->AddActiveActuator(actua,newevent);
	}
	//printf("expr %d.",expressionresult);
	// every actuator that needs the event, has a it's own reference to it now so
	// release it (so to be clear: if there is no actuator, it's deleted right now)
	newevent->Release();
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
			identifierval = new CBoolValue(sensor->IsPositiveTrigger());
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
