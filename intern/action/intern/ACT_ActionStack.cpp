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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ACT_ActionStack.h"


ACT_ActionStack::ACT_ActionStack(unsigned int maxStackDepth)
	: m_maxStackDepth(maxStackDepth),
	  m_undoIndex(0), m_undoIndexValid(false),
	  m_redoIndex(0), m_redoIndexValid(false)
{
}


ACT_ActionStack::~ACT_ActionStack()
{
	flush();
}


unsigned int ACT_ActionStack::getStackDepth() const
{
	return m_stack.size();
}


unsigned int ACT_ActionStack::getMaxStackDepth() const
{
	return m_maxStackDepth;
}


void ACT_ActionStack::setMaxStackDepth(unsigned int maxStackDepth)
{
	if (maxStackDepth != m_maxStackDepth) {
		if (maxStackDepth) {
			unsigned int size = m_stack.size();
			if (maxStackDepth < size) {
				// New max stack size is smaller than current stack size, need to shrink stack
				unsigned int numRemove = size - maxStackDepth;
				if (m_undoIndex >= maxStackDepth) {
					// Pop items from the front (throw away undo steps)
					popFront(numRemove);
					m_undoIndex -= numRemove;
					m_redoIndex = m_undoIndex + 1;
					m_redoIndexValid = m_redoIndexValid && (maxStackDepth > 1);
				}
				else {
					// Pop items from the back (throw away redo steps)
					popBack(numRemove);
					m_redoIndexValid = m_redoIndexValid && (m_redoIndex < maxStackDepth);
				}
			}
		}
		else {
			// New stack size is zero
			flush();
		}
		m_maxStackDepth = maxStackDepth;
	}
}


void ACT_ActionStack::push(ACT_Action& action)
{
	if (m_maxStackDepth) {
		unsigned int size = m_stack.size();
		if (m_redoIndexValid) {
			// Remove items after the current action (throw away redo steps)
			popBack(size - m_redoIndex);
		}
		else if (size >= m_maxStackDepth) {
			// Remove items from the front (throw away undo steps)
			popFront(m_maxStackDepth - size + 1);
		}

		// Store the action
		if (!action.getIsApplied()) {
			action.apply();
		}
		action.incRef();
		m_stack.push_back(&action);

		// Update action indices
		m_redoIndex = m_stack.size();
		m_redoIndexValid = false;
		m_undoIndex = m_redoIndex - 1;
		m_undoIndexValid = true;
	}
}


ACT_Action* ACT_ActionStack::peekUndo()
{
	unsigned int i;
	return getUndoIndex(i) ? m_stack[i] : 0;
}


ACT_Action* ACT_ActionStack::peekRedo()
{
	unsigned int i;
	return getRedoIndex(i) ? m_stack[i] : 0;
}


void ACT_ActionStack::flush()
{
	popBack(m_stack.size());
	m_undoIndex = 0;
	m_undoIndexValid = false;
	m_redoIndex = 0;
	m_redoIndexValid = false;
}


bool ACT_ActionStack::canUndo() const
{
	unsigned int i;
	return getUndoIndex(i);
}


void ACT_ActionStack::undo()
{
	ACT_Action* action = peekUndo();
	if (action) {
		action->undo();

		// Update action indices
		m_redoIndex = m_undoIndex;
		m_redoIndexValid = true;
		if (m_undoIndex) {
			m_undoIndex--;
		}
		else {
			m_undoIndexValid = false;
		}
	}
}


bool ACT_ActionStack::canRedo() const
{
	unsigned int i;
	return getRedoIndex(i);
}


void ACT_ActionStack::redo()
{
	ACT_Action* action = peekRedo();
	if (action) {
		action->apply();

		// Update action indices
		m_undoIndex = m_redoIndex;
		m_undoIndexValid = true;
		m_redoIndex++;
		m_redoIndexValid = m_redoIndex < m_stack.size();
	}
}


unsigned int ACT_ActionStack::popFront(unsigned int numActions)
{
	unsigned int numRemoved = 0;

	while (numActions-- && m_stack.size()) {
		ACT_Action* action = m_stack[0];
		action->decRef();
		m_stack.pop_front();
		numRemoved++;
	}
	return numRemoved;	
}


unsigned int ACT_ActionStack::popBack(unsigned int numActions)
{
	unsigned int numRemoved = 0;
	unsigned int size;

	while (numActions-- && (size = m_stack.size())) {
		ACT_Action* action = m_stack[size-1];
		action->decRef();
		m_stack.pop_back();
		numRemoved++;
	}
	return numRemoved;	
}


bool ACT_ActionStack::getUndoIndex(unsigned int& i) const
{
	i = m_undoIndex;
	return m_undoIndexValid;
}


bool ACT_ActionStack::getRedoIndex(unsigned int& i) const
{
	i = m_redoIndex;
	return m_redoIndexValid;
}
