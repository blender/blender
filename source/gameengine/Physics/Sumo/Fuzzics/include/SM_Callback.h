#ifndef SM_CALLBACK_H
#define SM_CALLBACK_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class SM_Callback {
public:
	virtual void do_me() = 0;
}; 

#endif

