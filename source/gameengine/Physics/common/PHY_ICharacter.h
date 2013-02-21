
/** \file PHY_ICharacter.h
 *  \ingroup phys
 */

#ifndef __PHY_ICHARACTER_H__
#define __PHY_ICHARACTER_H__

//PHY_ICharacter provides a generic interface for "character" controllers

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class PHY_ICharacter
{
public:	
	virtual ~PHY_ICharacter(){};

	virtual void Jump()= 0;
	virtual bool OnGround()= 0;

	virtual float GetGravity()= 0;
	virtual void SetGravity(float gravity)= 0;
	
	virtual int GetMaxJumps()= 0;
	virtual void SetMaxJumps(int maxJumps)= 0;

	virtual int GetJumpCount()= 0;

	virtual void SetWalkDirection(const class MT_Vector3& dir)=0;
	virtual MT_Vector3 GetWalkDirection()=0;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:PHY_ICharacter")
#endif
};

#endif //__PHY_ICHARACTER_H__
