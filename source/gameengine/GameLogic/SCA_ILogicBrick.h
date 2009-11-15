/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __KX_ILOGICBRICK
#define __KX_ILOGICBRICK

#include "Value.h"
#include "SCA_IObject.h"
#include "BoolValue.h"
#include "GEN_Map.h"
#include "GEN_HashedPtr.h"

class NG_NetworkScene;
class SCA_IScene;

class SCA_ILogicBrick : public CValue
{
	Py_Header;
protected:
	SCA_IObject*		m_gameobj;
	int					m_Execute_Priority;
	int					m_Execute_Ueber_Priority;

	bool				m_bActive;
	CValue*				m_eventval;
	STR_String			m_text;
	STR_String			m_name;
	//unsigned long		m_drawcolor;
	void RegisterEvent(CValue* eventval);
	void RemoveEvent();
	CValue* GetEvent();

public:
	SCA_ILogicBrick(SCA_IObject* gameobj);
	virtual ~SCA_ILogicBrick();

	void SetExecutePriority(int execute_Priority);
	void SetUeberExecutePriority(int execute_Priority);

	SCA_IObject*	GetParent() { return m_gameobj; }

	virtual void	ReParent(SCA_IObject* parent);
	virtual void	Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map);
	virtual void Delete() { Release(); }

	// act as a BoolValue (with value IsPositiveTrigger)
	virtual CValue*	Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue*	CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);

	virtual const STR_String &	GetText();
	virtual double		GetNumber();
	virtual STR_String&	GetName();
	virtual void		SetName(const char *);
		
	bool				IsActive()
	{
		return m_bActive;
	}

	void				SetActive(bool active)
	{
		m_bActive=active;
	}

	// insert in a QList at position corresponding to m_Execute_Priority
	void			    InsertActiveQList(SG_QList& head)
	{
		SG_QList::iterator<SCA_ILogicBrick> it(head);
		for(it.begin(); !it.end() && m_Execute_Priority > (*it)->m_Execute_Priority; ++it);
		it.add_back(this);
	}

	// insert in a QList at position corresponding to m_Execute_Priority
	// inside a longer list that contains elements of other objects. 
	// Sorting is done only between the elements of the same object.
	// head is the head of the combined list
	// current points to the first element of the object in the list, NULL if none yet
	void			    InsertSelfActiveQList(SG_QList& head, SG_QList** current)
	{
		if (!*current)
		{
			// first element can be put anywhere
			head.QAddBack(this);
			*current = this;
			return;
		}
		// note: we assume current points actually to one o our element, skip the tests
		SG_QList::iterator<SCA_ILogicBrick> it(head,*current);
		if (m_Execute_Priority <= (*it)->m_Execute_Priority)
		{
			// this element comes before the first
			*current = this;
		}
		else
		{
			for(++it; !it.end() && (*it)->m_gameobj == m_gameobj &&  m_Execute_Priority > (*it)->m_Execute_Priority; ++it);
		}
		it.add_back(this);
	}

	virtual	bool		LessComparedTo(SCA_ILogicBrick* other);

	/* runtime variable, set when Triggering the python controller */
	static class SCA_LogicManager*	m_sCurrentLogicManager;


	/* for moving logic bricks between scenes */
	virtual void		Replace_IScene(SCA_IScene *val) {};
	virtual void		Replace_NetworkScene(NG_NetworkScene *val) {};

#ifndef DISABLE_PYTHON
	// python methods
	
	static PyObject*	pyattr_get_owner(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);

	// check that attribute is a property
	static int CheckProperty(void *self, const PyAttributeDef *attrdef);

	enum KX_BOOL_TYPE {
		KX_BOOL_NODEF = 0,
		KX_TRUE,
		KX_FALSE,
		KX_BOOL_MAX
	};


protected: 
	/* Some conversions to go with the bool type. */
	/** Convert a KX_TRUE, KX_FALSE in Python to a c++ value. */
	bool PyArgToBool(int boolArg);

	/** Convert a a c++ value to KX_TRUE, KX_FALSE in Python. */
	PyObject* BoolToPyArg(bool);
	
#endif // DISABLE_PYTHON

};

#endif

