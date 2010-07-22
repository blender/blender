/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include "GHOST_NDOFManagerCocoa.h"
#include <3DconnexionClient/ConnexionClientAPI.h>
#include <stdio.h>

static void SpaceNavAdded(io_connect_t connection)
	{
	printf("SpaceNav added\n");
	}

static void SpaceNavRemoved(io_connect_t connection)
	{
	printf("SpaceNav removed\n");
	}

static void SpaceNavEvent(io_connect_t connection, natural_t messageType, void *messageArgument)
	{
	GHOST_System* system = (GHOST_System*) GHOST_ISystem::getSystem();
	GHOST_NDOFManager* manager = system->getNDOFManager();
	switch (messageType)
		{
		case kConnexionMsgDeviceState:
			{
			ConnexionDeviceState* s = (ConnexionDeviceState*)messageArgument;
			switch (s->command)
				{
				case kConnexionCmdHandleAxis:
					manager->updateTranslation(s->axis, s->time);
					manager->updateRotation(s->axis + 3, s->time);
					break;

				case kConnexionCmdHandleButtons:
					manager->updateButtons(s->buttons, s->time);
					break;
				}
			break;
			}
		case kConnexionMsgPrefsChanged:
			printf("prefs changed\n"); // this includes app switches
			break;
		case kConnexionMsgDoAction:
			printf("do action\n"); // no idea what this means
			// 'calibrate device' in System Prefs sends this
			break;
		default:
			printf("<!> mystery event\n");
		}
	}

GHOST_NDOFManagerCocoa::GHOST_NDOFManagerCocoa(GHOST_System& sys)
	: GHOST_NDOFManager(sys)
	{
	if (available())
		{
		OSErr error = InstallConnexionHandlers(SpaceNavEvent, SpaceNavAdded, SpaceNavRemoved);
		if (error)
			{
			printf("<!> error = %d\n", error);
			return;
			}

//		char* name = "\pBlender";
//		name[0] = 7; // convert to Pascal string

		m_clientID = RegisterConnexionClient('blnd', (UInt8*) "\pBlender"/*name*/,
			kConnexionClientModeTakeOver, kConnexionMaskAll);

		printf("client id = %d\n", m_clientID);
		}
	else
		{
		printf("<!> SpaceNav driver not found\n");
		// This isn't a hard error, just means the user doesn't have a SpaceNavigator.
		}
	}

GHOST_NDOFManagerCocoa::~GHOST_NDOFManagerCocoa()
	{
	UnregisterConnexionClient(m_clientID);
	CleanupConnexionHandlers();
	}

bool GHOST_NDOFManagerCocoa::available()
	{
	extern OSErr InstallConnexionHandlers() __attribute__((weak_import));
// [mce] C likes the above line, but Obj-C++ does not. Make sure it works for
//       machines without the driver installed! Try it on the QuickSilver.

	return InstallConnexionHandlers != NULL;
	}
