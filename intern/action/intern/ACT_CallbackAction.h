/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	June 17, 2001
 */

#ifndef _H_ACT_CALLBACK_ACTION_H
#define _H_ACT_CALLBACK_ACTION_H

#include "ACT_Action.h"
#include "ACT_ActionC-Api.h"


/**
 * An action implementation that uses callbacks for undo and redo.
 * @author	Maarten Gribnau
 * @date	June 17, 2001
 */

class ACT_CallbackAction : public ACT_Action {
public:
	/**
	 * Constructs an action with the given name .
	 * @param	name		The name of the action.
	 * @param	applied		State of the action after creation.
	 * @param	data		Pointer to user data passed to the callbacks.
	 * @param	applyProc	Callback invoked for apply action.
	 * @param	undoProc	Callback invoked for undo action.
	 */
	ACT_CallbackAction(
		STR_String& name,
		bool applied,
		ACT_ActionUserDataPtr data,
		ACT_ActionApplyProcPtr applyProc,
		ACT_ActionUndoProcPtr undoProc,
		ACT_ActionDisposeProcPtr disposeProc)
		: ACT_Action(name, applied), m_userData(data), m_applyProc(applyProc), m_undoProc(undoProc), m_disposeProc(disposeProc)
	{
	}

	/**
	 * Default destructor.
	 * Will call the dispose callback.
	 */
	virtual ~ACT_CallbackAction()
	{
		// Should assert
		m_disposeProc((ACT_ActionPtr)this, m_userData);
	}


protected:
	/**
	 * Calls the appropriate callback.
	 */
	inline virtual void doApply();

	/**
	 * Calls the appropriate callback.
	 */
	inline virtual void doUndo();

	/** User data supplied at creation and passed to the callbacks. */
	ACT_ActionUserDataPtr m_userData;

	/** Callback invoked for apply action. */
	ACT_ActionApplyProcPtr m_applyProc;

	/** Callback invoked for undo action. */
	ACT_ActionApplyProcPtr m_undoProc;

	/** Callback invoked when the action is disposed. */
	ACT_ActionDisposeProcPtr m_disposeProc;
};


inline void ACT_CallbackAction::doApply()
{
	// Should assert
	m_applyProc((ACT_ActionPtr)this, m_userData);
}


inline void ACT_CallbackAction::doUndo()
{
	// Should assert
	m_undoProc((ACT_ActionPtr)this, m_userData);
}


#endif // _H_ACT_CALLBACK_ACTION_H

