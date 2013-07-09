/** \file gameengine/Expressions/VectorValue.cpp
 *  \ingroup expressions
 */
// VectorValue.cpp: implementation of the CVectorValue class.
/*
 * Copyright (c) 1996-2000 Erwin Coumans <coockie@acm.org>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include "Value.h"
#include "VectorValue.h"
#include "ErrorValue.h"
//#include "MatrixValue.h"
#include "VoidValue.h"
#include "StringValue.h"
//#include "FactoryManager.h"



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVectorValue::CVectorValue(float x,float y,float z, AllocationTYPE alloctype)
{
	SetCustomFlag1(false);//FancyOutput=false;
	
	if (alloctype == STACKVALUE)
	{
		CValue::DisableRefCount();
	};
	
	m_vec[KX_X] = m_transformedvec[KX_X] = x;
	m_vec[KX_Y] = m_transformedvec[KX_Y] = y;
	m_vec[KX_Z] = m_transformedvec[KX_Z] = z;
	
}
CVectorValue::CVectorValue(double vec[3], const char *name,AllocationTYPE alloctype)
{
	
	SetCustomFlag1(false);//FancyOutput=false;
	
	m_vec[KX_X] = m_transformedvec[KX_X] = vec[KX_X];
	m_vec[KX_Y] = m_transformedvec[KX_Y] = vec[KX_Y];
	m_vec[KX_Z] = m_transformedvec[KX_Z] = vec[KX_Z];
		
	if (alloctype == STACKVALUE)
	{
		CValue::DisableRefCount();
		
	}
	
	SetName(name);
}

CVectorValue::CVectorValue(double vec[3], AllocationTYPE alloctype)
{
	
	SetCustomFlag1(false);//FancyOutput=false;
	
	m_vec[KX_X] = m_transformedvec[KX_X] = vec[KX_X];
	m_vec[KX_Y] = m_transformedvec[KX_Y] = vec[KX_Y];
	m_vec[KX_Z] = m_transformedvec[KX_Z] = vec[KX_Z];
	
	if (alloctype == STACKVALUE)
	{
		CValue::DisableRefCount();
		
	}
	
	
}
CVectorValue::~CVectorValue()
{

}

/**
 * pre: the type of val is dtype
 * ret: a new object containing the result of applying operator op to val and
 * this object
 */
CValue* CVectorValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
{
	CValue *ret = NULL;
	
	switch (op) {
		case VALUE_ADD_OPERATOR:
		{
			switch (dtype)
			{
				case VALUE_EMPTY_TYPE:
				case VALUE_VECTOR_TYPE:
				{
					ret = new CVectorValue(
					            val->GetVector3()[KX_X] + GetVector3()[KX_X],
					            val->GetVector3()[KX_Y] + GetVector3()[KX_Y],
					            val->GetVector3()[KX_Z] + GetVector3()[KX_Z],
					            CValue::HEAPVALUE);
					ret->SetName(GetName());
					break;
				}

				default:
					ret = new CErrorValue(val->GetText() + op2str(op) +	GetText());
			}
			break;
		}
		case VALUE_MUL_OPERATOR:
		{
			switch (dtype)
			{
				
				case VALUE_EMPTY_TYPE:
				case VALUE_VECTOR_TYPE:
				{
					//MT_Vector3 supports 'scaling' by another vector, instead of using general transform, Gino?
					//ret = new CVectorValue(val->GetVector3().Scaled(GetVector3()),GetName());
					break;
				}
				case VALUE_FLOAT_TYPE:
				{
					ret = new CVectorValue(
					            val->GetVector3()[KX_X] * GetVector3()[KX_X],
					            val->GetVector3()[KX_Y] * GetVector3()[KX_Y],
					            val->GetVector3()[KX_Z] * GetVector3()[KX_Z],
					            CValue::HEAPVALUE);
					ret->SetName(GetName());
					break;
				}

				default:
					ret = new CErrorValue(val->GetText() + op2str(op) +	GetText());
			}
			break;

		}

		default:
			ret = new CErrorValue(val->GetText() + op2str(op) +	GetText());
	}

	
	return ret;
}

double CVectorValue::GetNumber()
{
	return m_vec[KX_X];
}


double* CVectorValue::GetVector3(bool bGetTransformedVec)
{
	if (bGetTransformedVec)
		return m_transformedvec;
	// else 
	return m_vec;
}





void CVectorValue::SetVector(double newvec[])
{
	m_vec[KX_X] = m_transformedvec[KX_X] = newvec[KX_X];
	m_vec[KX_Y] = m_transformedvec[KX_Y] = newvec[KX_Y];
	m_vec[KX_Z] = m_transformedvec[KX_Z] = newvec[KX_Z];
	
	SetModified(true);
}


void CVectorValue::SetValue(CValue *newval)
{
	
	double* newvec = ((CVectorValue*)newval)->GetVector3();
	m_vec[KX_X] = m_transformedvec[KX_X] = newvec[KX_X];
	m_vec[KX_Y] = m_transformedvec[KX_Y] = newvec[KX_Y];
	m_vec[KX_Z] = m_transformedvec[KX_Z] = newvec[KX_Z];
	
	SetModified(true);
}

static const STR_String gstrVectorStr=STR_String();
const STR_String & CVectorValue::GetText()
{
	assertd(false);
	return gstrVectorStr;
}

CValue* CVectorValue::GetReplica()
{
	CVectorValue* replica = new CVectorValue(*this);
	replica->ProcessReplica();
	return replica;
};

#if 0
void CVectorValue::Transform(rcMatrix4x4 mat)
{
	m_transformedvec = mat*m_vec;
}
#endif
