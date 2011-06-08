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
 * Contributor(s):
 *   Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include "GHOST_NDOFManagerCocoa.h"
#include "GHOST_SystemCocoa.h"

extern "C" {
	#include <3DconnexionClient/ConnexionClientAPI.h>
	#include <stdio.h>
	}

static void SpaceNavAdded(io_connect_t connection)
	{
	printf("SpaceNav added\n"); // change these: printf --> informational reports
	}

static void SpaceNavRemoved(io_connect_t connection)
	{
	printf("SpaceNav removed\n");
	}

static void SpaceNavEvent(io_connect_t connection, natural_t messageType, void* messageArgument)
	{
	GHOST_SystemCocoa* system = (GHOST_SystemCocoa*) GHOST_ISystem::getSystem();
	GHOST_NDOFManager* manager = system->getNDOFManager();
	switch (messageType)
		{
		case kConnexionMsgDeviceState:
			{
			ConnexionDeviceState* s = (ConnexionDeviceState*)messageArgument;

			GHOST_TUns64 now = system->getMilliSeconds();

			switch (s->command)
				{
				case kConnexionCmdHandleAxis:
					manager->updateTranslation(s->axis, now);
					manager->updateRotation(s->axis + 3, now);
					system->notifyExternalEventProcessed();
					break;

				case kConnexionCmdHandleButtons:
					manager->updateButtons(s->buttons, now);
					system->notifyExternalEventProcessed();
					break;

				default:
					printf("device state command %d\n", s->command);
				}
			break;
			}
		case kConnexionMsgPrefsChanged:
			printf("prefs changed\n"); // this includes app switches
			break;
		case kConnexionMsgDoAction:
			printf("do action\n"); // no idea what this means
			// 'calibrate device' in System Prefs sends this
			// 3Dx header file says to ignore these
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

		// Pascal string *and* a four-letter constant. How old-skool.
		m_clientID = RegisterConnexionClient('blnd', (UInt8*) "\pblender",
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
//	extern OSErr InstallConnexionHandlers() __attribute__((weak_import));
// ^-- not needed since the entire framework is weak-linked
	return InstallConnexionHandlers != NULL;
	}
