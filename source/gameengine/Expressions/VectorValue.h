/*
 * VectorValue.h: interface for the CVectorValue class.
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

/** \file VectorValue.h
 *  \ingroup expressions
 */

#ifndef __VECTORVALUE_H__
#define __VECTORVALUE_H__

#include "Value.h"

#define KX_X 0
#define KX_Y 1
#define KX_Z 2


class CVectorValue : public CPropValue  
{
	//PLUGIN_DECLARE_SERIAL(CVectorValue,CValue)

public:
	//void Transform(rcMatrix4x4 mat);
	virtual void SetValue(CValue* newval);
	void SetVector(double newvec[]);
	void Configure(CValue* menuvalue);
	virtual double* GetVector3(bool bGetTransformedVec=false);
	virtual double		GetNumber();
	
	CValue*		Calc(VALUE_OPERATOR op, CValue *val) {
		return val->CalcFinal(VALUE_VECTOR_TYPE, op, this);
	}
	
	CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	
	
	CVectorValue(double vec[],const char *name,AllocationTYPE alloctype=CValue::HEAPVALUE);
	CVectorValue() {};

	CVectorValue(double vec[],AllocationTYPE alloctype=CValue::HEAPVALUE);
	CVectorValue(float x,float y,float z, AllocationTYPE alloctype = CValue::HEAPVALUE);
	virtual ~CVectorValue();
	//virtual bool ExportT3D(File *txtfile,bool bNoName=false);
	void AddConfigurationData(CValue* menuvalue);
	


	virtual CValue* GetReplica();
	virtual const STR_String & GetText();

#if 0
	void			SnapPoint(float num,int snap)
	{
		if (num > 0) num += ((float)snap / 2);
		else num -= ((float)snap / 2);
		num = (long)(((long)(num / snap)) * snap);
	};

	void SnapPosition(const double snapvec[])
	{
		
		if (snapvec[KX_X] >= 1)
			SnapPoint(m_vec[KX_X],snapvec[KX_X]);
		if (snapvec[KX_Y] >= 1)
			SnapPoint(m_vec[KX_Y],snapvec[KX_Y]);
		if (snapvec[KX_Z] >= 1)
			SnapPoint(m_vec[KX_Z],snapvec[KX_Z]);
			
	}
#endif

protected:
	double				m_vec[3];
	double				m_transformedvec[3];


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CVectorValue"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined __VECTORVALUE_H__

