#ifndef __SM_CLIENTOBJECT_INFO_H
#define __SM_CLIENTOBJECT_INFO_H

/**
 * Client Type and Additional Info. This structure can be use instead of a bare void* pointer, for safeness, and additional info for callbacks
 */

struct SM_ClientObjectInfo
{
	int			m_type;
	void*		m_clientobject;
	void*		m_auxilary_info;
};

#endif //__SM_CLIENTOBJECT_INFO_H
