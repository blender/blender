#ifndef CSG_SIMPLEPROP_H
#define CSG_SIMPLEPROP_H

// Simple face prop contains a fixed size piece of memory
// initiated by the client and copied round through this
// value type by the CSG library.
#include <memory.h>

class SimpleProp
{
private :

	static int s_size;
	unsigned char * m_data;	

public :

	static void SetSize(int size) {
		s_size = size;
	}

	static int Size() {
		return s_size;
	}

	SimpleProp()
	: m_data(new unsigned char[s_size])
	{}

	SimpleProp(const SimpleProp& other)
	: m_data(new unsigned char[s_size])
	{
		memcpy(m_data,other.m_data,s_size);
	}	

	SimpleProp& operator = (const SimpleProp& other)
	{
		memcpy(m_data,other.m_data,s_size);
		return *this;
	}

	void SetData(const void * data)
	{
		memcpy(m_data,data,s_size);
	}

	const unsigned char * Data() const {
		return m_data;
	}


	~SimpleProp() 
	{
		delete[] m_data;
	}	
};

#endif