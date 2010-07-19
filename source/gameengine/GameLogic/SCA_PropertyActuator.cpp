/**
 * Assign, change, copy properties
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "SCA_PropertyActuator.h"
#include "InputParser.h"
#include "Operator2Expr.h"
#include "ConstExpr.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_PropertyActuator::SCA_PropertyActuator(SCA_IObject* gameobj,SCA_IObject* sourceObj,const STR_String& propname,const STR_String& expr,int acttype)
   :	SCA_IActuator(gameobj, KX_ACT_PROPERTY),
	m_type(acttype),
	m_propname(propname),
	m_exprtxt(expr),
	m_sourceObj(sourceObj)
{
	// protect ourselves against someone else deleting the source object
	// don't protect against ourselves: it would create a dead lock
	if (m_sourceObj)
		m_sourceObj->RegisterActuator(this);
}

SCA_PropertyActuator::~SCA_PropertyActuator()
{
	if (m_sourceObj)
		m_sourceObj->UnregisterActuator(this);
}

bool SCA_PropertyActuator::Update()
{
	bool result = false;

	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();


	if (bNegativeEvent)
		return false; // do nothing on negative events


	CValue* propowner = GetParent();
	CParser parser;
	parser.SetContext( propowner->AddRef());
	
	CExpression* userexpr= NULL;
	
	if (m_type==KX_ACT_PROP_TOGGLE)
	{
		/* dont use */
		CValue* newval;
		CValue* oldprop = propowner->GetProperty(m_propname);
		if (oldprop)
		{
			newval = new CBoolValue((oldprop->GetNumber()==0.0) ? true:false);
			oldprop->SetValue(newval);
		} else
		{	/* as not been assigned, evaluate as false, so assign true */
			newval = new CBoolValue(true);
			propowner->SetProperty(m_propname,newval);
		}
		newval->Release();
	}
	else if ((userexpr = parser.ProcessText(m_exprtxt))) {
		switch (m_type)
		{

		case KX_ACT_PROP_ASSIGN:
			{
				
				CValue* newval = userexpr->Calculate();
				CValue* oldprop = propowner->GetProperty(m_propname);
				if (oldprop)
				{
					oldprop->SetValue(newval);
				} else
				{
					propowner->SetProperty(m_propname,newval);
				}
				newval->Release();
				break;
			}
		case KX_ACT_PROP_ADD:
			{
				CValue* oldprop = propowner->GetProperty(m_propname);
				if (oldprop)
				{
					// int waarde = (int)oldprop->GetNumber();  /*unused*/
					CExpression* expr = new COperator2Expr(VALUE_ADD_OPERATOR,new CConstExpr(oldprop->AddRef()),
															userexpr->AddRef());

					CValue* newprop = expr->Calculate();
					oldprop->SetValue(newprop);
					newprop->Release();
					expr->Release();

				}

				break;
			}
		case KX_ACT_PROP_COPY:
			{
				if (m_sourceObj)
				{
					CValue* copyprop = m_sourceObj->GetProperty(m_exprtxt);
					if (copyprop)
					{
						CValue *val = copyprop->GetReplica();
						GetParent()->SetProperty(
							 m_propname,
							 val);
						val->Release();

					}
				}
				break;
			}
		/* case KX_ACT_PROP_TOGGLE: */ /* accounted for above, no need for userexpr */
		default:
			{

			}
		}

		userexpr->Release();
	}
	
	return result;
}

	bool 

SCA_PropertyActuator::

isValid(

	SCA_PropertyActuator::KX_ACT_PROP_MODE mode

){
	bool res = false;	
	res = ((mode > KX_ACT_PROP_NODEF) && (mode < KX_ACT_PROP_MAX));
	return res;
}


	CValue* 

SCA_PropertyActuator::

GetReplica() {

	SCA_PropertyActuator* replica = new SCA_PropertyActuator(*this);

	replica->ProcessReplica();
	return replica;

};

void SCA_PropertyActuator::ProcessReplica()
{
	// no need to check for self reference like in the constructor:
	// the replica will always have a different parent
	if (m_sourceObj)
		m_sourceObj->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

bool SCA_PropertyActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_sourceObj)
	{
		// this object is being deleted, we cannot continue to track it.
		m_sourceObj = NULL;
		return true;
	}
	return false;
}

void SCA_PropertyActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_sourceObj];
	if (h_obj) {
		if (m_sourceObj)
			m_sourceObj->UnregisterActuator(this);
		m_sourceObj = (SCA_IObject*)(*h_obj);
		m_sourceObj->RegisterActuator(this);
	}
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_PropertyActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_PropertyActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_PropertyActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_PropertyActuator::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW_CHECK("propName",0,100,false,SCA_PropertyActuator,m_propname,CheckProperty),
	KX_PYATTRIBUTE_STRING_RW("value",0,100,false,SCA_PropertyActuator,m_exprtxt),
	KX_PYATTRIBUTE_INT_RW("mode", KX_ACT_PROP_NODEF+1, KX_ACT_PROP_MAX-1, false, SCA_PropertyActuator, m_type), /* ATTR_TODO add constents to game logic dict */
	{ NULL }	//Sentinel
};

#endif

/* eof */
