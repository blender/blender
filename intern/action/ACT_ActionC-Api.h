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
 * Interface for C access to actions.
 * @author	Maarten Gribnau
 * @date	April, 25, 2001
 */

#ifndef _H_ACT_ACTION_C_API
#define _H_ACT_ACTION_C_API

#include "MEM_RefCountedC-Api.h"

/** A pointer to an action object. */
typedef MEM_TRefCountedObjectPtr ACT_ActionPtr;
/** A pointer to an action stack object. */
typedef MEM_TObjectPtr ACT_ActionStackPtr;


/** A pointer to user data passed by the callbacks. */
typedef void* ACT_ActionUserDataPtr;

/**
 * An action apply callback routine.
 * @param action The action that should be applied.
 * @param userData The pointer to the user data provided when the action was created.
 */
typedef void (*ACT_ActionApplyProcPtr)(ACT_ActionPtr action, ACT_ActionUserDataPtr userData);

/**
 * An action undo callback routine.
 * @param action The action that should be undone.
 * @param userData The pointer to the user data provided when the action was created.
 */
typedef void (*ACT_ActionUndoProcPtr)(ACT_ActionPtr action, ACT_ActionUserDataPtr userData);

/**
 * An action dispose callback routine.
 * @param action The action that is disposed.
 * @param userData The pointer to the user data provided when the action was created.
 */
typedef void (*ACT_ActionDisposeProcPtr)(ACT_ActionPtr action, ACT_ActionUserDataPtr userData);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * An action is a shared object that can be applied or undone.
 */

/**
 * Creates a new action.
 * This is an action that calls the given callbacks when it needs to be applied or undone.
 * @param name The name of the action.
 * @param isApplied Indication as to whether the action is already applied (0 = not applied).
 * @param userData Pointer passed to the apply/undo callbacks.
 * @param applyProc Pointer to the callback invoked when the action needs to be applied.
 * @param undoProc Pointer to the callback invoked when the action needs to be undone.
 * @return	The new action (null in case of error).
 */
extern ACT_ActionPtr ACT_ActionCreate(
						char* name,
						int isApplied,
						ACT_ActionUserDataPtr userData,
						ACT_ActionApplyProcPtr applyProc,
						ACT_ActionUndoProcPtr undoProc,
						ACT_ActionDisposeProcPtr disposeProc);

/**
 * Returns the name of an action.
 * @return	The name of the action (null in case of error).
 */
extern char* ACT_ActionGetName(ACT_ActionPtr action);



/**
 * An action stack stores actions and implements undo/redo functionality.
 */

/**
 * Creates a new action stack.
 * @param stackSize The maximum number of actions on the stack.
 * @return The new stack (or NULL in case of error).
 */
extern ACT_ActionStackPtr ACT_ActionStackCreate(unsigned int stackSize);

/**
 * Disposes an action stack.
 * @param stack The appropriate stack.
 */
extern void	ACT_ActionStackDispose(ACT_ActionStackPtr stack);

/**
 * Returns the current depth of the stack.
 * @param stack The appropriate stack.
 * @return the current stack depth.
 */
extern unsigned int ACT_ActionStackGetStackDepth(ACT_ActionStackPtr stack);

/**
 * Returns the current maximum depth of the stack.
 * @param stack The appropriate stack.
 * @return the maximum stack depth.
 */
extern unsigned int ACT_ActionStackGetMaxStackDepth(ACT_ActionStackPtr stack);

/**
 * Sets new maximum depth of the stack.
 * @param stack The appropriate stack.
 * @param maxStackDepth	The new stack depth.
 */
extern void ACT_ActionStackSetMaxStackDepth(ACT_ActionStackPtr stack, unsigned int maxStackDepth);

/**
 * Pushes an action on the stack.
 * If the action has not been applied yet, it will be applied here.
 * This will increase the reference count of the action.
 * If there is not enough capacity, the action at the bottom of the stack is removed (and its reference count decreased).
 * @param stack The appropriate stack.
 * @param action	the action that is pushed onto the stack.
 */
extern void	ACT_ActionStackPush(ACT_ActionStackPtr stack, ACT_ActionPtr action);

/**
 * Returns pointer to the current undo item.
 * @param stack The appropriate stack.
 * @return The action scheduled for undo (0 if there is none).
 */
extern ACT_ActionStackPtr ACT_ActionStackPeekUndo(ACT_ActionStackPtr stack);

/**
 * Returns pointer to the current redo item.
 * @param stack The appropriate stack.
 * @return The action scheduled for redo (0 if there is none).
 */
extern ACT_ActionStackPtr ACT_ActionStackPeekRedo(ACT_ActionStackPtr stack);

/**
 * Undos the current action.
 * @param stack The appropriate stack.
 * This will move the current undo index down (if the stack depth allows it).
 */
extern void	ACT_ActionStackUndo(ACT_ActionStackPtr stack);

/**
 * Redos the current action.
 * @param stack The appropriate stack.
 * This will move the action index up (if the stack depth allows it).
 */
extern void	ACT_ActionStackRedo(ACT_ActionStackPtr stack);


#ifdef __cplusplus
}
#endif

#endif // _H_ACT_ACTION_C_API

