#ifndef CSG_IndexProp_H
#define CSG_IndexProp_H

#include <vector>
#include <memory.h>
// Face and vertex props that are contained in a seperate array
// (PropArray) and indexed through a VProp compliant thing.

typedef int (*CSG_InterpFunc)(const void *d1, const void * d2, void *dnew, float epsilon);

class IndexProp
{
private :

	int m_vIndex;
	int m_size;
	unsigned char *m_data;
	mutable CSG_InterpFunc m_interpFunc;

public :

	IndexProp(const int& vIndex)
		: m_vIndex(vIndex), 
		  m_size(0),
		  m_data(0)
	{};
	
	IndexProp(
		const int& vIndex, 
		const IndexProp& p1, 
		const IndexProp& p2, 
		const MT_Scalar& epsilon
	): 
		m_vIndex(vIndex),
		m_data(0)
	{
		SetInterpFunc(p1.m_interpFunc);
		SetSize(p1.m_size);
		m_interpFunc(p1.m_data,p2.m_data,m_data,(float)epsilon);
	}

	IndexProp(const IndexProp& other)
	:	m_vIndex(other.m_vIndex),
		m_data(0),
		m_interpFunc(other.m_interpFunc)
	{
		SetInterpFunc(other.m_interpFunc);
		SetSize(other.m_size);
		memcpy(m_data,other.m_data,m_size);
	}

	IndexProp(
	):  m_vIndex(-1),
		m_size(0),
		m_data(0)
	{};

	// Support conversion to an integer
	///////////////////////////////////
	operator int(
	) const { 
		return m_vIndex;
	}

	// and assignment from an integer.
	//////////////////////////////////
		IndexProp& 
	operator = (
		int i
	) { 
		m_vIndex = i; 
		return *this;
	}

		IndexProp&
	operator = (
		const IndexProp& other
	) {
		m_vIndex = other.m_vIndex;
		m_data = 0;
		SetSize(other.m_size);
		SetInterpFunc(other.m_interpFunc);
		memcpy(m_data,other.m_data,m_size);
		return *this;
	}

	// Our local functions
	//////////////////////

	void SetInterpFunc(CSG_InterpFunc interpFunc)
	{
		m_interpFunc = interpFunc;
	}
	
	void SetSize(int size)
	{
		delete[] m_data;
		m_data = new unsigned char[size];
		m_size = size;
	}

	int Size() const {
		return m_size;
	}

	void CopyData(const void * userData)
	{
		memcpy(m_data,userData,m_size);
	}

	void Create(int size, const void * userData, CSG_InterpFunc interpFunc)
	{
		SetInterpFunc(interpFunc);
		SetSize(size);
		CopyData(userData);
	}

	const unsigned char * GetData() const { return m_data;}

	~IndexProp() {
		delete[] m_data;
	};
};
	

#endif