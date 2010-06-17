#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include "SpuFakeDma.h"
#include "LinearMath/btScalar.h"


///DoubleBuffer
template<class T, int size>
class DoubleBuffer
{
#if defined(__SPU__) || defined(USE_LIBSPE2)
	ATTRIBUTE_ALIGNED128( T m_buffer0[size] ) ;
	ATTRIBUTE_ALIGNED128( T m_buffer1[size] ) ;
#else
	T m_buffer0[size];
	T m_buffer1[size];
#endif
	
	T *m_frontBuffer;
	T *m_backBuffer;

	unsigned int m_dmaTag;
	bool m_dmaPending;
public:
	bool	isPending() const { return m_dmaPending;}
	DoubleBuffer();

	void init ();

	// dma get and put commands
	void backBufferDmaGet(uint64_t ea, unsigned int numBytes, unsigned int tag);
	void backBufferDmaPut(uint64_t ea, unsigned int numBytes, unsigned int tag);

	// gets pointer to a buffer
	T *getFront();
	T *getBack();

	// if back buffer dma was started, wait for it to complete
	// then move back to front and vice versa
	T *swapBuffers();
};

template<class T, int size>
DoubleBuffer<T,size>::DoubleBuffer()
{
	init ();
}

template<class T, int size>
void DoubleBuffer<T,size>::init()
{
	this->m_dmaPending = false;
	this->m_frontBuffer = &this->m_buffer0[0];
	this->m_backBuffer = &this->m_buffer1[0];
}

template<class T, int size>
void
DoubleBuffer<T,size>::backBufferDmaGet(uint64_t ea, unsigned int numBytes, unsigned int tag)
{
	m_dmaPending = true;
	m_dmaTag = tag;
	if (numBytes)
	{
		m_backBuffer = (T*)cellDmaLargeGetReadOnly(m_backBuffer, ea, numBytes, tag, 0, 0);
	}
}

template<class T, int size>
void
DoubleBuffer<T,size>::backBufferDmaPut(uint64_t ea, unsigned int numBytes, unsigned int tag)
{
	m_dmaPending = true;
	m_dmaTag = tag;
	cellDmaLargePut(m_backBuffer, ea, numBytes, tag, 0, 0);
}

template<class T, int size>
T *
DoubleBuffer<T,size>::getFront()
{
	return m_frontBuffer;
}

template<class T, int size>
T *
DoubleBuffer<T,size>::getBack()
{
	return m_backBuffer;
}

template<class T, int size>
T *
DoubleBuffer<T,size>::swapBuffers()
{
	if (m_dmaPending)
	{
		cellDmaWaitTagStatusAll(1<<m_dmaTag);
		m_dmaPending = false;
	}

	T *tmp = m_backBuffer;
	m_backBuffer = m_frontBuffer;
	m_frontBuffer = tmp;

	return m_frontBuffer;
}

#endif
