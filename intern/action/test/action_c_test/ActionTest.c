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

#include "ACT_ActionC-Api.h"
#include "TestAction.h"

int main(int argc, char *argv[])
{
	ACT_ActionStackPtr stack = ACT_ActionStackCreate (3);
	ACT_ActionPtr action = ACT_ActionCreate("action1", 0, 0, printApplied, printUndone, printDisposed);
	ACT_ActionStackPush(stack, action);
	MEM_RefCountedDecRef(action);
	action = ACT_ActionCreate("action2", 0, 0, printApplied, printUndone, printDisposed);
	ACT_ActionStackPush(stack, action);
	MEM_RefCountedDecRef(action);
	action = ACT_ActionCreate("action3", 0, 0, printApplied, printUndone, printDisposed);
	ACT_ActionStackPush(stack, action);
	MEM_RefCountedDecRef(action);

	ACT_ActionStackUndo(stack);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackRedo(stack);
	ACT_ActionStackRedo(stack);
	ACT_ActionStackRedo(stack);

	ACT_ActionStackSetMaxStackDepth(stack, 1);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackRedo(stack);
	ACT_ActionStackSetMaxStackDepth(stack, 5);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackRedo(stack);

	action = ACT_ActionCreate("action4", 0, 0, printApplied, printUndone, printDisposed);
	ACT_ActionStackPush(stack, action);
	MEM_RefCountedDecRef(action);
	ACT_ActionStackUndo(stack);
	action = ACT_ActionCreate("action5", 0, 0, printApplied, printUndone, printDisposed);
	ACT_ActionStackPush(stack, action);
	MEM_RefCountedDecRef(action);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackUndo(stack);
	ACT_ActionStackRedo(stack);
	ACT_ActionStackRedo(stack);

	return 0;
}
