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
 * @date	March 31, 2001
 */

#ifndef _H_ACT_ACTIONSTACK
#define _H_ACT_ACTIONSTACK

#include "ACT_Action.h"
#include <deque>


/**
 * A stack with actions that implements undo/redo capabilities.
 * A stack can grow to a maximum number of actions by pushing actions on the stack.
 * By calling undo and redo the apply and undo members of the actions on the stack are called.
 * In addition, this will move the stackIndex up and down the stack.
 * When a new action is pushed onto the stack, the actions above the current action are removed from the stack.
 * Actions pushed onto the stack are applied if they are not applied already.
 * @todo	implement error handling (e.g. memory errors)
 * @author	Maarten Gribnau
 * @date	March 31, 2001
 */

class ACT_ActionStack {
public:
	/**
	 * Constructs an action stack.
	 */
	ACT_ActionStack(unsigned int maxStackDepth = 1);

	/**
	 * Destructs an action stack.
	 */
	virtual ~ACT_ActionStack();

	/**
	 * Returns the current depth of the stack.
	 * @return the current stack depth.
	 */
	virtual unsigned int getStackDepth() const;

	/**
	 * Returns the current maximum depth of the stack.
	 * @return the maximum stack depth.
	 */
	virtual unsigned int getMaxStackDepth() const;

	/**
	 * Sets new maximum depth of the stack.
	 * @param maxStackDepth	The new stack depth.
	 */
	virtual void setMaxStackDepth(unsigned int maxStackDepth);

	/**
	 * Pushes an action on the stack.
	 * If the action has not been applied yet, it will be applied here.
	 * This will increase the reference count of the action.
	 * If there is not enough capacity, the action at the bottom of the stack is removed (and its reference count decreased).
	 * @param action	the action that is pushed onto the stack.
	 */
	virtual void push(ACT_Action& action);

	/**
	 * Returns pointer to the current undo item.
	 * @return The action scheduled for undo (0 if there is none).
	 */
	virtual ACT_Action* peekUndo();

	/**
	 * Returns pointer to the current redo item.
	 * @return The action scheduled for redo (0 if there is none).
	 */
	virtual ACT_Action* peekRedo();

	/**
	 * Flushes the action stack.
	 * All actions are removed from the stack and their reference counts decreased.
	 */
	virtual void flush();

	/**
	 * Returns whether we can undo the current action.
	 * @return Indication of the possibility to undo.
	 */
	virtual bool canUndo() const;

	/**
	 * Undos the current action.
	 * This will move the current undo index down (if the stack depth allows it).
	 */
	virtual void undo();

	/**
	 * Returns whether we can redo the current action.
	 * @return Indication of the possibility to redo.
	 */
	virtual bool canRedo() const;

	/**
	 * Redos the current action.
	 * This will move the action index up (if the stack depth allows it).
	 */
	virtual void redo();

protected:
	/**
	 * Removes <i>numActions</i> actions from the back of the stack.
	 * @param numActions	number of items to remove.
	 * @return the number of actions removed.
	 */
	virtual unsigned int popBack(unsigned int numActions = 1);

	/**
	 * Removes <i>numActions</i> actions from the front of the stack.
	 * @param numActions	number of items to remove.
	 * @return the number of actions removed.
	 */
	virtual unsigned int popFront(unsigned int numActions = 1);

	/**
	 * Returns the index of the current undo action.
	 * @param index	The index of the action.
	 * @return Indication as to whether the index is valid (==true).
	 */
	virtual bool getUndoIndex(unsigned int& index) const;

	/**
	 * Returns the index of the current redo action.
	 * @param index	The index of the action.
	 * @return Indication as to whether the index is valid (==true).
	 */
	virtual bool getRedoIndex(unsigned int& index) const;

	/** The maximum depth of this stack. */
	unsigned int m_maxStackDepth;
	/** The index of the current undo action in the stack. */
	unsigned int m_undoIndex;
	/** Is the index of the current undo action in the stack valid? */
	bool m_undoIndexValid;
	/** The index of the current redo action in the stack. */
	unsigned int m_redoIndex;
	/** Is the index of the current redo action in the stack valid? */
	bool m_redoIndexValid;
	/** The stack with actions. */
	deque<ACT_Action*> m_stack;
};


#endif // _H_ACT_ACTIONSTACK