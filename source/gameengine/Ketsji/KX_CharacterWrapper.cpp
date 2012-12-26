/** \file gameengine/Ketsji/KX_CharacterWrapper.cpp
 *  \ingroup ketsji
 */


#include "KX_CharacterWrapper.h"
#include "PHY_ICharacter.h"

KX_CharacterWrapper::KX_CharacterWrapper(PHY_ICharacter* character) :
		PyObjectPlus(),
		m_character(character)
{
}

KX_CharacterWrapper::~KX_CharacterWrapper()
{
	if (m_character)
		delete m_character; // We're responsible for the character object!
}

#ifdef WITH_PYTHON

PyTypeObject KX_CharacterWrapper::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_CharacterWrapper",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyAttributeDef KX_CharacterWrapper::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("onGround", KX_CharacterWrapper, pyattr_get_onground),
	KX_PYATTRIBUTE_RW_FUNCTION("gravity", KX_CharacterWrapper, pyattr_get_gravity, pyattr_set_gravity),
	KX_PYATTRIBUTE_RW_FUNCTION("maxJumps", KX_CharacterWrapper, pyattr_get_max_jumps, pyattr_set_max_jumps),
	{ NULL }	//Sentinel
};

PyObject *KX_CharacterWrapper::pyattr_get_onground(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CharacterWrapper* self = static_cast<KX_CharacterWrapper*>(self_v);

	return PyBool_FromLong(self->m_character->OnGround());
}

PyObject *KX_CharacterWrapper::pyattr_get_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CharacterWrapper* self = static_cast<KX_CharacterWrapper*>(self_v);

	return PyFloat_FromDouble(self->m_character->GetGravity());
}

int KX_CharacterWrapper::pyattr_set_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_CharacterWrapper* self = static_cast<KX_CharacterWrapper*>(self_v);
	double param = PyFloat_AsDouble(value);

	if (param == -1)
	{
		PyErr_SetString(PyExc_ValueError, "KX_CharacterWrapper.gravity: expected a float");
		return PY_SET_ATTR_FAIL;
	}

	self->m_character->SetGravity((float)param);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_CharacterWrapper::pyattr_get_max_jumps(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CharacterWrapper* self = static_cast<KX_CharacterWrapper*>(self_v);

	return PyLong_FromLong(self->m_character->GetMaxJumps());
}

int KX_CharacterWrapper::pyattr_set_max_jumps(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_CharacterWrapper* self = static_cast<KX_CharacterWrapper*>(self_v);
	long param = PyLong_AsLong(value);

	if (param == -1)
	{
		PyErr_SetString(PyExc_ValueError, "KX_CharacterWrapper.maxJumps: expected an integer");
		return PY_SET_ATTR_FAIL;
	}

	self->m_character->SetMaxJumps((int)param);
	return PY_SET_ATTR_SUCCESS;
}
PyMethodDef KX_CharacterWrapper::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_CharacterWrapper, jump),
	{NULL,NULL} //Sentinel
};

KX_PYMETHODDEF_DOC_NOARGS(KX_CharacterWrapper, jump,
	"jump()\n"
	"makes the character jump.\n")
{
	m_character->Jump();

	Py_RETURN_NONE;
}

#endif // WITH_PYTHON
