
/** \file KX_CharacterWrapper.h
 *  \ingroup ketsji
 */

#ifndef __KX_CHARACTERWRAPPER_H__
#define __KX_CHARACTERWRAPPER_H__

#include "Value.h"
#include "PHY_DynamicTypes.h"
class PHY_ICharacter;


///Python interface to character physics
class	KX_CharacterWrapper : public PyObjectPlus
{
	Py_Header

public:
	KX_CharacterWrapper(PHY_ICharacter* character);
	virtual ~KX_CharacterWrapper();
#ifdef WITH_PYTHON
	KX_PYMETHOD_DOC_NOARGS(KX_CharacterWrapper, jump);

	static PyObject* pyattr_get_onground(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	
	static PyObject*	pyattr_get_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_max_jumps(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_max_jumps(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_jump_count(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_walk_dir(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_walk_dir(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif // WITH_PYTHON

private:
	PHY_ICharacter*			 m_character;
};

#endif  /* __KX_CHARACTERWRAPPER_H__ */
