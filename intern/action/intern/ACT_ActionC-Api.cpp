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
 * @date	April, 25, 2001
 */

#include "ACT_ActionC-Api.h"

#include "ACT_ActionStack.h"
#include "ACT_CallbackAction.h"


ACT_ActionPtr ACT_ActionCreate(
	char* name,
	int isApplied,
	ACT_ActionUserDataPtr data,
	ACT_ActionApplyProcPtr applyProc,
	ACT_ActionUndoProcPtr undoProc,
	ACT_ActionDisposeProcPtr disposeProc)
{
	STR_String tmp (name);
	ACT_CallbackAction* action = new ACT_CallbackAction(tmp, isApplied != 0, data, applyProc, undoProc, disposeProc);
	return (ACT_ActionPtr) action;
}


char* ACT_ActionGetName(ACT_ActionPtr action)
{
	return action ? ((ACT_Action*)action)->getName() : 0;
}


ACT_ActionStackPtr ACT_ActionStackCreate(unsigned int stackSize)
{
	return ((ACT_ActionStackPtr) (new ACT_ActionStack (stackSize)));
}


void ACT_ActionStackDispose(ACT_ActionStackPtr stack)
{
	if (stack) {
		delete (ACT_ActionStack*) stack;
	}
}


unsigned int ACT_ActionStackGetStackDepth(ACT_ActionStackPtr stack)
{
	return stack ? ((ACT_ActionStack*)stack)->getStackDepth() : 0;
}

unsigned int ACT_ActionStackGetMaxStackDepth(ACT_ActionStackPtr stack)
{
	return stack ? ((ACT_ActionStack*)stack)->getMaxStackDepth() : 0;
}

void ACT_ActionStackSetMaxStackDepth(ACT_ActionStackPtr stack, unsigned int maxStackDepth)
{
	if (stack) {
		((ACT_ActionStack*)stack)->setMaxStackDepth(maxStackDepth);
	}
}

void ACT_ActionStackPush(ACT_ActionStackPtr stack, ACT_ActionPtr action)
{
	if (stack && action) {
		((ACT_ActionStack*)stack)->push(*((ACT_Action*)action));
	}
}


ACT_ActionStackPtr ACT_ActionStackPeekUndo(ACT_ActionStackPtr stack)
{
	return (ACT_ActionStackPtr) (stack ? ((ACT_ActionStack*)stack)->peekUndo() : 0);
}


ACT_ActionStackPtr ACT_ActionStackPeekRedo(ACT_ActionStackPtr stack)
{
	return (ACT_ActionStackPtr) (stack ? ((ACT_ActionStack*)stack)->peekRedo() : 0);
}


void ACT_ActionStackUndo(ACT_ActionStackPtr stack)
{
	if (stack) {
		((ACT_ActionStack*)stack)->undo();
	}
}


void ACT_ActionStackRedo(ACT_ActionStackPtr stack)
{
	if (stack) {
		((ACT_ActionStack*)stack)->redo();
	}
}
