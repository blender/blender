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
#include "TestAction.h"

int main()
{
    ACT_ActionStack testStack (3);
    TestAction* testAction = new TestAction (STR_String("action1"));
    testStack.push(*testAction);
    testAction->decRef();
    testAction = new TestAction (STR_String("action2"));
    testStack.push(*testAction);
    testAction->decRef();
    testAction = new TestAction (STR_String("action3"));
    testStack.push(*testAction);
    testAction->decRef();

    testStack.undo();
    testStack.undo();
    testStack.undo();
    testStack.redo();
    testStack.redo();
    testStack.redo();

    testStack.setMaxStackDepth(1);
    testStack.undo();
    testStack.redo();
    testStack.setMaxStackDepth(5);
    testStack.undo();
    testStack.redo();

    testAction = new TestAction (STR_String("action4"));
    testStack.push(*testAction);
    testAction->decRef();
    testStack.undo();
    testAction = new TestAction (STR_String("action5"));
    testStack.push(*testAction);
    testAction->decRef();
    testStack.undo();
    testStack.undo();
    testStack.redo();
    testStack.redo();

	return 0;
}
