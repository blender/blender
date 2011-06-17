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

// static functions need to talk to these objects:
static GHOST_SystemCocoa* ghost_system = NULL;
static GHOST_NDOFManager* ndof_manager = NULL;

static void NDOF_DeviceAdded(io_connect_t connection)
	{
	printf("ndof: device added\n"); // change these: printf --> informational reports

#if 0 // device preferences will be useful soon, but not for hardware model detection
	ConnexionDevicePrefs p;
	ConnexionGetCurrentDevicePrefs(kDevID_AnyDevice, &p);
	printf("device type %d: %s\n", p.deviceID,
		p.deviceID == kDevID_SpaceNavigator ? "SpaceNavigator" :
		p.deviceID == kDevID_SpaceNavigatorNB ? "SpaceNavigator for Notebooks" :
		p.deviceID == kDevID_SpaceExplorer ? "SpaceExplorer" :
		"unknown");
#endif

	// determine exactly which device is plugged in
	int result = 0;
	ConnexionControl(kConnexionCtlGetDeviceID, 0, &result);
	unsigned short vendorID = result >> 16;
	unsigned short productID = result & 0xffff;

	ndof_manager->setDevice(vendorID, productID);
	}

static void NDOF_DeviceRemoved(io_connect_t connection)
	{
	printf("ndof: device removed\n");
	}

static void NDOF_DeviceEvent(io_connect_t connection, natural_t messageType, void* messageArgument)
	{
	switch (messageType)
		{
		case kConnexionMsgDeviceState:
			{
			ConnexionDeviceState* s = (ConnexionDeviceState*)messageArgument;

			GHOST_TUns64 now = ghost_system->getMilliSeconds();

			switch (s->command)
				{
				case kConnexionCmdHandleAxis:
					ndof_manager->updateTranslation(s->axis, now);
					ndof_manager->updateRotation(s->axis + 3, now);
					ghost_system->notifyExternalEventProcessed();
					break;

				case kConnexionCmdHandleButtons:

					// s->buttons field has only 16 bits, not enough for SpacePilotPro
					// look at raw USB report for more button bits
					printf("ndof: button bits = [");
					for (int i = 0; i < 8; ++i)
						printf("%02x", s->report[i]);
					printf("]\n");

					ndof_manager->updateButtons(s->buttons, now);
					ghost_system->notifyExternalEventProcessed();
					break;

				case kConnexionCmdAppSpecific:
					printf("ndof: app-specific command, param = %hd, value = %d\n", s->param, s->value);
					break;

				default:
					printf("ndof: mystery device command %d\n", s->command);
				}
			break;
			}
		case kConnexionMsgPrefsChanged:
			printf("ndof: prefs changed\n"); // this includes app switches
			break;
		case kConnexionMsgDoAction:
			printf("ndof: do action\n"); // no idea what this means
			// 'calibrate device' in System Prefs sends this
			// 3Dx header file says to ignore these
			break;
		default:
			printf("ndof: mystery event %d\n", messageType);
		}
	}

GHOST_NDOFManagerCocoa::GHOST_NDOFManagerCocoa(GHOST_System& sys)
	: GHOST_NDOFManager(sys)
	{
	if (available())
		{
		// give static functions something to talk to:
		ghost_system = dynamic_cast<GHOST_SystemCocoa*>(&sys);
		ndof_manager = this;

		OSErr error = InstallConnexionHandlers(NDOF_DeviceEvent, NDOF_DeviceAdded, NDOF_DeviceRemoved);
		if (error)
			{
			printf("ndof: error %d while installing handlers\n", error);
			return;
			}

		// Pascal string *and* a four-letter constant. How old-skool.
		m_clientID = RegisterConnexionClient('blnd', (UInt8*) "\pblender",
			kConnexionClientModeTakeOver, kConnexionMaskAll);

		printf("ndof: client id = %d\n", m_clientID);
		}
	else
		{
		printf("ndof: 3Dx driver not found\n");
		// This isn't a hard error, just means the user doesn't have a 3D mouse.
		}
	}

GHOST_NDOFManagerCocoa::~GHOST_NDOFManagerCocoa()
	{
	UnregisterConnexionClient(m_clientID);
	CleanupConnexionHandlers();
	ghost_system = NULL;
	ndof_manager = NULL;
	}

bool GHOST_NDOFManagerCocoa::available()
	{
//	extern OSErr InstallConnexionHandlers() __attribute__((weak_import));
// ^-- not needed since the entire framework is weak-linked
	return InstallConnexionHandlers != NULL;
	}
