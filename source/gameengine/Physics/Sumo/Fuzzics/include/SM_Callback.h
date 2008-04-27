#ifndef SM_CALLBACK_H
#define SM_CALLBACK_H

class SM_Callback {
public:
	virtual void do_me() = 0;
	virtual ~SM_Callback() {}
}; 

#endif

