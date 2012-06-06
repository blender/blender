/*
 * Value.h: interface for the CValue class.
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

/** \file Value.h
 *  \ingroup expressions
 */

#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning (disable:4786)
#endif //WIN32

#ifndef __VALUE_H__
#define __VALUE_H__

#include <map>		// array functionality for the propertylist
#include "STR_String.h"	// STR_String class

using namespace std;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#ifndef GEN_NO_ASSERT
#undef  assert
#define	assert(exp)			((void)NULL)
#endif


#ifndef GEN_NO_TRACE
#undef  trace
#define	trace(exp)			((void)NULL)
#endif

#ifndef GEN_NO_DEBUG
#undef  debug
#define	debug(exp)			((void)NULL)
#endif

#ifndef GEN_NO_ASSERTD
#undef  assertd
#define	assertd(exp)			((void)NULL)
#endif


#ifndef USE_PRAGMA_ONCE
#ifdef WIN32
	#pragma once

#endif //WIN32
#endif

enum VALUE_OPERATOR {
	
	VALUE_MOD_OPERATOR,			// %
	VALUE_ADD_OPERATOR,			// +
	VALUE_SUB_OPERATOR,			// -
	VALUE_MUL_OPERATOR,			// *
	VALUE_DIV_OPERATOR,			// /
	VALUE_NEG_OPERATOR,			// -
	VALUE_POS_OPERATOR,			// +
	VALUE_AND_OPERATOR,			// &&
	VALUE_OR_OPERATOR,			// ||
	VALUE_EQL_OPERATOR,			// ==
	VALUE_NEQ_OPERATOR,			// !=
	VALUE_GRE_OPERATOR,			// >
	VALUE_LES_OPERATOR,			// <
	VALUE_GEQ_OPERATOR,			// >=
	VALUE_LEQ_OPERATOR,			// <=
	VALUE_NOT_OPERATOR,			// !
	VALUE_NO_OPERATOR			// no operation at all
};

enum VALUE_DATA_TYPE {
	VALUE_NO_TYPE,				// abstract baseclass
	VALUE_INT_TYPE,
	VALUE_FLOAT_TYPE,
	VALUE_STRING_TYPE,
	VALUE_BOOL_TYPE,
	VALUE_ERROR_TYPE,
	VALUE_EMPTY_TYPE,
	VALUE_SOLID_TYPE,
	VALUE_COMBISOLID_TYPE,
	VALUE_VECTOR_TYPE,
	VALUE_MENU_TYPE,
	VALUE_ACTOR_TYPE,
	VALUE_MAX_TYPE				//only here to provide number of types
};



#ifdef _DEBUG
//extern int gRefCountValue;		// debugonly variable to check if all CValue Refences are Dereferenced at programexit
#endif

struct HashableInt 
{
	HashableInt(int id)															: mData(id) { }

	unsigned long				Hash() const											{ return 0;} ////}gHash(&mData, sizeof(int));}
	
	bool				operator==(HashableInt rhs)								{ return mData == rhs.mData; }
	
	int					mData;
};


//
// Bitfield that stores the flags for each CValue derived class
//
struct ValueFlags {
	ValueFlags() :
		Modified(true),
		Selected(false),
		Affected(false),
		ReleaseRequested(false),
		Error(false),
		RefCountDisabled(false),
		HasProperties(false),
		HasName(false),
		Visible(true),
		CustomFlag1(false),
		CustomFlag2(false)
	{
	}

	unsigned short Modified : 1;
	unsigned short Selected : 1;
	unsigned short Affected : 1;
	unsigned short ReleaseRequested : 1;
	unsigned short Error : 1;
	unsigned short RefCountDisabled : 1;
	unsigned short HasProperties : 1;
	unsigned short HasName : 1;
	unsigned short Visible : 1;
	unsigned short CustomFlag1 : 1;
	unsigned short CustomFlag2 : 1;

	
};

/**
 *	Base Class for all Actions performed on CValue's. Can be extended for undo/redo system in future.
 */
class CAction
{
public:
	CAction() {
	};
	virtual ~CAction() {
	};
	virtual void Execute() const =0;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CAction"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


#include "PyObjectPlus.h"
#ifdef WITH_PYTHON
#include "object.h"
#endif

/**
 * Baseclass CValue
 *
 * Together with CExpression, CValue and it's derived classes can be used to
 * parse expressions into a parsetree with error detecting/correcting capabilities
 * also expandable by a CFactory pluginsystem 
 *
 * Base class for all editor functionality, flexible object type that allows
 * calculations and uses reference counting for memory management.
 * 
 * Features:
 * - Reference Counting (AddRef() / Release())
 * - Calculations (Calc() / CalcFinal())
 * - Configuration (Configure())
 * - Serialization (EdSerialize() / EdIdSerialize() / EdPtrSerialize() and macro PLUGIN_DECLARE_SERIAL
 * - Property system (SetProperty() / GetProperty() / FindIdentifier())
 * - Replication (GetReplica())
 * - Flags (IsSelected() / IsModified() / SetSelected()...)
 * 
 * - Some small editor-specific things added
 * - A helperclass CompressorArchive handles the serialization
 * 
 */
class CValue  : public PyObjectPlus

{
Py_Header
public:
	enum AllocationTYPE {
		STACKVALUE		= 0,
		HEAPVALUE		= 1
	};
	
	enum DrawTYPE {
		STARTFRAME		= 0,
		ENDFRAME		= 1,
		INTERFRAME		= 2
	};


	// Construction / Destruction
	CValue();

#ifdef WITH_PYTHON
	//static PyObject*	PyMake(PyObject*,PyObject*);
	virtual PyObject *py_repr(void)
	{
		return PyUnicode_From_STR_String(GetText());
	}

	virtual PyObject*	ConvertValueToPython() {
		return NULL;
	}

	virtual CValue*	ConvertPythonToValue(PyObject* pyobj, const char *error_prefix);
	
	static PyObject * pyattr_get_name(void * self, const KX_PYATTRIBUTE_DEF * attrdef);
	
	virtual PyObject* ConvertKeysToPython( void );
#endif // WITH_PYTHON

	
	
	// Expression Calculation
	virtual CValue*		Calc(VALUE_OPERATOR op, CValue *val) = 0;
	virtual CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val) = 0;
	virtual void		SetOwnerExpression(class CExpression* expr);

	

	void				Execute(const CAction& a)
	{
		a.Execute();
	};

	/// Reference Counting
	int					GetRefCount()											
	{ 
		return m_refcount; 
	}

	// Add a reference to this value
	CValue*				AddRef()												
	{
		// Increase global reference count, used to see at the end of the program
		// if all CValue-derived classes have been dereferenced to 0
		//debug(gRefCountValue++);
	#ifdef _DEBUG
		//gRefCountValue++;
	#endif
		m_refcount++; 
		return this;
	}

	// Release a reference to this value (when reference count reaches 0, the value is removed from the heap)
	int			Release()								
	{
		// Decrease global reference count, used to see at the end of the program
		// if all CValue-derived classes have been dereferenced to 0
		//debug(gRefCountValue--);
	#ifdef _DEBUG
		//gRefCountValue--;
	#endif
		// Decrease local reference count, if it reaches 0 the object should be freed
		if (--m_refcount > 0)
		{
			// Reference count normal, return new reference count
			return m_refcount;
		}
		else
		{
			// Reference count reached 0, delete ourselves and return 0
	//		MT_assert(m_refcount==0, "Reference count reached sub-zero, object released too much");
			
			delete this;
			return 0;
		}
	}


	/// Property Management
	virtual void		SetProperty(const STR_String& name,CValue* ioProperty);						// Set property <ioProperty>, overwrites and releases a previous property with the same name if needed
	virtual void		SetProperty(const char* name,CValue* ioProperty);
	virtual CValue*		GetProperty(const char* inName);							// Get pointer to a property with name <inName>, returns NULL if there is no property named <inName>
	virtual CValue*		GetProperty(const STR_String & inName);
	const STR_String&	GetPropertyText(const STR_String & inName);						// Get text description of property with name <inName>, returns an empty string if there is no property named <inName>
	float				GetPropertyNumber(const STR_String& inName,float defnumber);
	virtual bool		RemoveProperty(const char *inName);						// Remove the property named <inName>, returns true if the property was succesfully removed, false if property was not found or could not be removed
	virtual vector<STR_String>	GetPropertyNames();
	virtual void		ClearProperties();										// Clear all properties

	virtual void		SetPropertiesModified(bool inModified);					// Set all properties' modified flag to <inModified>
	virtual bool		IsAnyPropertyModified();								// Check if any of the properties in this value have been modified

	virtual CValue*		GetProperty(int inIndex);								// Get property number <inIndex>
	virtual int			GetPropertyCount();										// Get the amount of properties assiocated with this value

	virtual CValue*		FindIdentifier(const STR_String& identifiername);
	/** Set the wireframe color of this value depending on the CSG
	 * operator type <op>
	 * \attention: not implemented */
	virtual void		SetColorOperator(VALUE_OPERATOR op);

	virtual const STR_String &	GetText() = 0;
	virtual double		GetNumber() = 0;
	double*				ZeroVector() { return m_sZeroVec; }
	virtual double*		GetVector3(bool bGetTransformedVec = false);

	virtual STR_String&	GetName() = 0;											// Retrieve the name of the value
	virtual void		SetName(const char *name) = 0;								// Set the name of the value
	/** Sets the value to this cvalue.
	 * \attention this particular function should never be called. Why not abstract? */
	virtual void		SetValue(CValue* newval);
	virtual CValue*		GetReplica() =0;
	virtual void			ProcessReplica();
	//virtual CValue*		Copy() = 0;
	
	
	STR_String				op2str(VALUE_OPERATOR op);
		
	// setting / getting flags
	inline void			SetSelected(bool bSelected)								{ m_ValFlags.Selected = bSelected; }
	virtual void		SetModified(bool bModified)								{ m_ValFlags.Modified = bModified; }
	virtual void		SetAffected(bool bAffected=true)						{ m_ValFlags.Affected = bAffected; }
	inline void			SetReleaseRequested(bool bReleaseRequested)				{ m_ValFlags.ReleaseRequested=bReleaseRequested; }
	inline void			SetError(bool err)										{ m_ValFlags.Error=err; }
	inline void			SetVisible (bool vis)									{ m_ValFlags.Visible=vis; }
																				
	virtual bool		IsModified()											{ return m_ValFlags.Modified; }
	inline bool			IsError()												{ return m_ValFlags.Error; }
	virtual bool		IsAffected()											{ return m_ValFlags.Affected || m_ValFlags.Modified; }
	virtual bool		IsSelected()											{ return m_ValFlags.Selected; }
	inline bool			IsReleaseRequested()									{ return m_ValFlags.ReleaseRequested; }
	virtual bool		IsVisible()												{ return m_ValFlags.Visible;}
	virtual void		SetCustomFlag1(bool bCustomFlag)						{ m_ValFlags.CustomFlag1 = bCustomFlag;}
	virtual bool		IsCustomFlag1()											{ return m_ValFlags.CustomFlag1;}

	virtual void		SetCustomFlag2(bool bCustomFlag)						{ m_ValFlags.CustomFlag2 = bCustomFlag;}
	virtual bool		IsCustomFlag2()											{ return m_ValFlags.CustomFlag2;}

protected:																		
	virtual void		DisableRefCount();										// Disable reference counting for this value
	//virtual void		AddDataToReplica(CValue* replica);						
	virtual				~CValue();
private:
	// Member variables															
	std::map<STR_String,CValue*>*		m_pNamedPropertyArray;									// Properties for user/game etc
	ValueFlags			m_ValFlags;												// Frequently used flags in a bitfield (low memoryusage)
	int					m_refcount;												// Reference Counter	
	static	double m_sZeroVec[3];	

};



//
// Declare a CValue or CExpression or CWhatever to be serialized by the editor.
//
// This macro introduces the EdSerialize() function (which must be implemented by
// the client) and the EdIdSerialize() function (which is implemented by this macro).
//
// The generated Copy() function returns a pointer to <root_base_class_name> type
// of object. So, for *any* CValue-derived object this should be set to CValue,
// for *any* CExpression-derived object this should be set to CExpression.
//
#define PLUGIN_DECLARE_SERIAL(class_name, root_base_class_name)                \
public:                                                                        \
	virtual root_base_class_name *Copy() {                                     \
		return new class_name;                                                 \
	}                                                                          \
	virtual bool EdSerialize(CompressorArchive& arch,                          \
	                         class CFactoryManager* facmgr,                    \
	                         bool bIsStoring);                                 \
	virtual bool EdIdSerialize(CompressorArchive& arch,                        \
	                           class CFactoryManager* facmgr,                  \
	                           bool bIsStoring)                                \
	{                                                                          \
		if (bIsStoring)                                                        \
			arch.StoreString(#class_name);                                     \
		return false;                                                          \
	}                                                                          \


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// CPropValue is a CValue derived class, that implements the identification (String name)
// SetName() / GetName(), 
// normal classes should derive from CPropValue, real lightweight classes straight from CValue


class CPropValue : public CValue
{
public:
	CPropValue() :
	  CValue(),
		m_strNewName()

	{
	}
	
	virtual ~CPropValue()
	{
	}
	
	virtual void			SetName(const char *name) {
		m_strNewName = name;
	}
	
	virtual STR_String&			GetName() {
		//STR_String namefromprop = GetPropertyText("Name");
		//if (namefromprop.Length() > 0)
		//	return namefromprop;
		return m_strNewName;
	}						// name of Value
	
protected:
	STR_String					m_strNewName;				    // Identification


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CPropValue"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined _VALUEBASECLASS_H

