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

#include "GHOST_Debug.h"
#include "GHOST_NDOFManager.h"
#include "GHOST_EventNDOF.h"
#include "GHOST_EventKey.h"
#include "GHOST_WindowManager.h"
#include <string.h> // for memory functions
#include <stdio.h> // for error/info reporting
#include <math.h>

#ifdef DEBUG_NDOF_MOTION
// printable version of each GHOST_TProgress value
static const char* progress_string[] =
	{"not started","starting","in progress","finishing","finished"};
#endif

#ifdef DEBUG_NDOF_BUTTONS
static const char* ndof_button_names[] = {
	// used internally, never sent
	"NDOF_BUTTON_NONE",
	// these two are available from any 3Dconnexion device
	"NDOF_BUTTON_MENU",
	"NDOF_BUTTON_FIT",
	// standard views
	"NDOF_BUTTON_TOP",
	"NDOF_BUTTON_BOTTOM",
	"NDOF_BUTTON_LEFT",
	"NDOF_BUTTON_RIGHT",
	"NDOF_BUTTON_FRONT",
	"NDOF_BUTTON_BACK",
	// more views
	"NDOF_BUTTON_ISO1",
	"NDOF_BUTTON_ISO2",
	// 90 degree rotations
	"NDOF_BUTTON_ROLL_CW",
	"NDOF_BUTTON_ROLL_CCW",
	"NDOF_BUTTON_SPIN_CW",
	"NDOF_BUTTON_SPIN_CCW",
	"NDOF_BUTTON_TILT_CW",
	"NDOF_BUTTON_TILT_CCW",
	// device control
	"NDOF_BUTTON_ROTATE",
	"NDOF_BUTTON_PANZOOM",
	"NDOF_BUTTON_DOMINANT",
	"NDOF_BUTTON_PLUS",
	"NDOF_BUTTON_MINUS",
	// general-purpose buttons
	"NDOF_BUTTON_1",
	"NDOF_BUTTON_2",
	"NDOF_BUTTON_3",
	"NDOF_BUTTON_4",
	"NDOF_BUTTON_5",
	"NDOF_BUTTON_6",
	"NDOF_BUTTON_7",
	"NDOF_BUTTON_8",
	"NDOF_BUTTON_9",
	"NDOF_BUTTON_10",
	// more general-purpose buttons
	"NDOF_BUTTON_A",
	"NDOF_BUTTON_B",
	"NDOF_BUTTON_C",
	// the end
	"NDOF_BUTTON_LAST"
};
#endif

static const NDOF_ButtonT SpaceNavigator_HID_map[] = {
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT
};

static const NDOF_ButtonT SpaceExplorer_HID_map[] = {
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_NONE, // esc key
	NDOF_BUTTON_NONE, // alt key
	NDOF_BUTTON_NONE, // shift key
	NDOF_BUTTON_NONE, // ctrl key
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	NDOF_BUTTON_ROTATE
};

static const NDOF_ButtonT SpacePilotPro_HID_map[] = {
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_BOTTOM,
	NDOF_BUTTON_BACK,
	NDOF_BUTTON_ROLL_CW,
	NDOF_BUTTON_ROLL_CCW,
	NDOF_BUTTON_ISO1,
	NDOF_BUTTON_ISO2,
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_3,
	NDOF_BUTTON_4,
	NDOF_BUTTON_5,
	NDOF_BUTTON_6,
	NDOF_BUTTON_7,
	NDOF_BUTTON_8,
	NDOF_BUTTON_9,
	NDOF_BUTTON_10,
	NDOF_BUTTON_NONE, // esc key
	NDOF_BUTTON_NONE, // alt key
	NDOF_BUTTON_NONE, // shift key
	NDOF_BUTTON_NONE, // ctrl key
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_PANZOOM,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS
};

// latest HW: button-compatible with SpacePilotPro, just fewer of them
static const NDOF_ButtonT SpaceMousePro_HID_map[] = {
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_NONE, // left
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_NONE, // bottom
	NDOF_BUTTON_NONE, // back
	NDOF_BUTTON_ROLL_CW,
	NDOF_BUTTON_NONE, // roll ccw
	NDOF_BUTTON_NONE, // iso 1
	NDOF_BUTTON_NONE, // iso 2
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_3,
	NDOF_BUTTON_4,
	NDOF_BUTTON_NONE, // 5
	NDOF_BUTTON_NONE, // 6
	NDOF_BUTTON_NONE, // 7
	NDOF_BUTTON_NONE, // 8
	NDOF_BUTTON_NONE, // 9
	NDOF_BUTTON_NONE, // 10
	NDOF_BUTTON_NONE, // esc key
	NDOF_BUTTON_NONE, // alt key
	NDOF_BUTTON_NONE, // shift key
	NDOF_BUTTON_NONE, // ctrl key
	NDOF_BUTTON_ROTATE,
};

/* this is the older SpacePilot (sans Pro)
 * thanks to polosson for info about this device */
static const NDOF_ButtonT SpacePilot_HID_map[] = {
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_3,
	NDOF_BUTTON_4,
	NDOF_BUTTON_5,
	NDOF_BUTTON_6,
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_NONE, // esc key
	NDOF_BUTTON_NONE, // alt key
	NDOF_BUTTON_NONE, // shift key
	NDOF_BUTTON_NONE, // ctrl key
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_NONE // the CONFIG button -- what does it do?
};

/* this is the older Spaceball 5000 USB
 * thanks to Tehrasha Darkon for info about this device */
static const NDOF_ButtonT Spaceball5000_HID_map[] = {
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_3,
	NDOF_BUTTON_4,
	NDOF_BUTTON_5,
	NDOF_BUTTON_6,
	NDOF_BUTTON_7,
	NDOF_BUTTON_8,
	NDOF_BUTTON_9,
	NDOF_BUTTON_A,
	NDOF_BUTTON_B,
	NDOF_BUTTON_C
};

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System& sys)
	: m_system(sys)
	, m_deviceType(NDOF_UnknownDevice) // each platform has its own device detection code
	, m_buttonCount(0)
	, m_buttonMask(0)
	, m_buttons(0)
	, m_motionTime(0)
	, m_prevMotionTime(0)
	, m_motionState(GHOST_kNotStarted)
	, m_motionEventPending(false)
	, m_deadZone(0.f)
{
	// to avoid the rare situation where one triple is updated and
	// the other is not, initialize them both here:
	memset(m_translation, 0, sizeof(m_translation));
	memset(m_rotation, 0, sizeof(m_rotation));
}

bool GHOST_NDOFManager::setDevice(unsigned short vendor_id, unsigned short product_id)
{
	// default to NDOF_UnknownDevice so rogue button events will get discarded
	// "mystery device" owners can help build a HID_map for their hardware

	switch (vendor_id) {
		case 0x046D: // Logitech (3Dconnexion)
			switch (product_id) {
				// -- current devices --
				case 0xC626:
					puts("ndof: using SpaceNavigator");
					m_deviceType = NDOF_SpaceNavigator;
					m_buttonCount = 2;
					break;
				case 0xC628:
					puts("ndof: using SpaceNavigator for Notebooks");
					m_deviceType = NDOF_SpaceNavigator; // for Notebooks
					m_buttonCount = 2;
					break;
				case 0xC627:
					puts("ndof: using SpaceExplorer");
					m_deviceType = NDOF_SpaceExplorer;
					m_buttonCount = 15;
					break;
				case 0xC629:
					puts("ndof: using SpacePilot Pro");
					m_deviceType = NDOF_SpacePilotPro;
					m_buttonCount = 31;
					break;
				case 0xC62B:
					puts("ndof: using SpaceMouse Pro");
					m_deviceType = NDOF_SpaceMousePro;
					m_buttonCount = 27;
					// ^^ actually has 15 buttons, but their HID codes range from 0 to 26
					break;

				// -- older devices --
				case 0xC625:
					puts("ndof: using SpacePilot");
					m_deviceType = NDOF_SpacePilot;
					m_buttonCount = 21;
					break;

				case 0xC621:
					puts("ndof: using Spaceball 5000");
					m_deviceType = NDOF_Spaceball5000;
					m_buttonCount = 12;
					break;

				case 0xC623:
					puts("ndof: SpaceTraveler not supported, please file a bug report");
					m_buttonCount = 8;
					break;

				default:
					printf("ndof: unknown Logitech product %04hx\n", product_id);
			}
			break;
		default:
			printf("ndof: unknown device %04hx:%04hx\n", vendor_id, product_id);
	}

	if (m_deviceType == NDOF_UnknownDevice) {
		return false;
	}
	else {
		m_buttonMask = ~(-1 << m_buttonCount);

		// special case for SpaceMousePro? maybe...

#ifdef DEBUG_NDOF_BUTTONS
		printf("ndof: %d buttons -> hex:%X\n", m_buttonCount, m_buttonMask);
#endif

		return true;
	}
}

void GHOST_NDOFManager::updateTranslation(short t[3], GHOST_TUns64 time)
{
	memcpy(m_translation, t, sizeof(m_translation));
	m_motionTime = time;
	m_motionEventPending = true;
}

void GHOST_NDOFManager::updateRotation(short r[3], GHOST_TUns64 time)
{
	memcpy(m_rotation, r, sizeof(m_rotation));
	m_motionTime = time;
	m_motionEventPending = true;
}

void GHOST_NDOFManager::sendButtonEvent(NDOF_ButtonT button, bool press, GHOST_TUns64 time, GHOST_IWindow* window)
{
	if (button == NDOF_BUTTON_NONE) {
		// just being exceptionally cautious...
		// air-tight button masking and proper function key emulation
		// should guarantee we never get to this point
#ifdef DEBUG_NDOF_BUTTONS
		printf("discarding NDOF_BUTTON_NONE (should not escape the NDOF manager)\n");
#endif
		return;
	}

	GHOST_EventNDOFButton* event = new GHOST_EventNDOFButton(time, window);
	GHOST_TEventNDOFButtonData* data = (GHOST_TEventNDOFButtonData*) event->getData();

	data->action = press ? GHOST_kPress : GHOST_kRelease;
	data->button = button;

#ifdef DEBUG_NDOF_BUTTONS
	printf("%s %s\n", ndof_button_names[button], press ? "pressed" : "released");
#endif

	m_system.pushEvent(event);
}

void GHOST_NDOFManager::sendKeyEvent(GHOST_TKey key, bool press, GHOST_TUns64 time, GHOST_IWindow* window)
{
	GHOST_TEventType type = press ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;
	GHOST_EventKey* event = new GHOST_EventKey(time, type, window, key);

#ifdef DEBUG_NDOF_BUTTONS
	printf("keyboard %s\n", press ? "down" : "up");
#endif

	m_system.pushEvent(event);
}

void GHOST_NDOFManager::updateButton(int button_number, bool press, GHOST_TUns64 time)
{
	GHOST_IWindow* window = m_system.getWindowManager()->getActiveWindow();

#ifdef DEBUG_NDOF_BUTTONS
	if (m_deviceType != NDOF_UnknownDevice)
		printf("ndof: button %d -> ", button_number);
#endif

	switch (m_deviceType) {
		case NDOF_SpaceNavigator:
			sendButtonEvent(SpaceNavigator_HID_map[button_number], press, time, window);
			break;
		case NDOF_SpaceExplorer:
			switch (button_number) {
				case 6: sendKeyEvent(GHOST_kKeyEsc, press, time, window); break;
				case 7: sendKeyEvent(GHOST_kKeyLeftAlt, press, time, window); break;
				case 8: sendKeyEvent(GHOST_kKeyLeftShift, press, time, window); break;
				case 9: sendKeyEvent(GHOST_kKeyLeftControl, press, time, window); break;
				default: sendButtonEvent(SpaceExplorer_HID_map[button_number], press, time, window);
			}
			break;
		case NDOF_SpacePilotPro:
			switch (button_number) {
				case 22: sendKeyEvent(GHOST_kKeyEsc, press, time, window); break;
				case 23: sendKeyEvent(GHOST_kKeyLeftAlt, press, time, window); break;
				case 24: sendKeyEvent(GHOST_kKeyLeftShift, press, time, window); break;
				case 25: sendKeyEvent(GHOST_kKeyLeftControl, press, time, window); break;
				default: sendButtonEvent(SpacePilotPro_HID_map[button_number], press, time, window);
			}
			break;
		case NDOF_SpaceMousePro:
			switch (button_number) {
				case 22: sendKeyEvent(GHOST_kKeyEsc, press, time, window); break;
				case 23: sendKeyEvent(GHOST_kKeyLeftAlt, press, time, window); break;
				case 24: sendKeyEvent(GHOST_kKeyLeftShift, press, time, window); break;
				case 25: sendKeyEvent(GHOST_kKeyLeftControl, press, time, window); break;
				default: sendButtonEvent(SpaceMousePro_HID_map[button_number], press, time, window);
			}
			break;
		case NDOF_SpacePilot:
			switch (button_number) {
				case 10: sendKeyEvent(GHOST_kKeyEsc, press, time, window); break;
				case 11: sendKeyEvent(GHOST_kKeyLeftAlt, press, time, window); break;
				case 12: sendKeyEvent(GHOST_kKeyLeftShift, press, time, window); break;
				case 13: sendKeyEvent(GHOST_kKeyLeftControl, press, time, window); break;
				case 20: puts("ndof: ignoring CONFIG button"); break;
				default: sendButtonEvent(SpacePilot_HID_map[button_number], press, time, window);
			}
			break;
		case NDOF_Spaceball5000:
			// has no special 'keyboard' buttons
			sendButtonEvent(Spaceball5000_HID_map[button_number], press, time, window);
			break;
		case NDOF_UnknownDevice:
			printf("ndof: button %d on unknown device (", button_number);
			// map to the 'general purpose' buttons
			// this is mainly for old serial devices
			if (button_number < NDOF_BUTTON_LAST - NDOF_BUTTON_1) {
				printf("sending)\n");
				sendButtonEvent((NDOF_ButtonT)(NDOF_BUTTON_1 + button_number), press, time, window);
			}
			else {
				printf("discarding)\n");
			}
	}

	int mask = 1 << button_number;
	if (press) {
		m_buttons |= mask; // set this button's bit
	}
	else {
		m_buttons &= ~mask; // clear this button's bit
	}
}

void GHOST_NDOFManager::updateButtons(int button_bits, GHOST_TUns64 time)
{
	button_bits &= m_buttonMask; // discard any "garbage" bits

	int diff = m_buttons ^ button_bits;

	for (int button_number = 0; button_number < m_buttonCount; ++button_number) {
		int mask = 1 << button_number;

		if (diff & mask) {
			bool press = button_bits & mask;
			updateButton(button_number, press, time);
		}
	}
}

void GHOST_NDOFManager::setDeadZone(float dz)
{
	if (dz < 0.f) {
		// negative values don't make sense, so clamp at zero
		dz = 0.f;
	}
	else if (dz > 0.5f) {
		// warn the rogue user/programmer, but allow it
		GHOST_PRINTF("ndof: dead zone of %.2f is rather high...\n", dz);
	}
	m_deadZone = dz;

	GHOST_PRINTF("ndof: dead zone set to %.2f\n", dz);
}

static bool atHomePosition(GHOST_TEventNDOFMotionData* ndof)
{
#define HOME(foo) (ndof->foo == 0.f)
	return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
}

static bool nearHomePosition(GHOST_TEventNDOFMotionData* ndof, float threshold)
{
	if (threshold == 0.f) {
		return atHomePosition(ndof);
	}
	else {
#define HOME(foo) (fabsf(ndof->foo) < threshold)
		return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
	}
}

bool GHOST_NDOFManager::sendMotionEvent()
{
	if (!m_motionEventPending)
		return false;

	m_motionEventPending = false; // any pending motion is handled right now

	GHOST_IWindow* window = m_system.getWindowManager()->getActiveWindow();

	if (window == NULL) {
		return false; // delivery will fail, so don't bother sending
	}

	GHOST_EventNDOFMotion* event = new GHOST_EventNDOFMotion(m_motionTime, window);
	GHOST_TEventNDOFMotionData* data = (GHOST_TEventNDOFMotionData*) event->getData();

	// scale axis values here to normalize them to around +/- 1
	// they are scaled again for overall sensitivity in the WM based on user prefs

	const float scale = 1.f / 350.f; // 3Dconnexion devices send +/- 350 usually

	data->tx = scale * m_translation[0];
	data->ty = scale * m_translation[1];
	data->tz = scale * m_translation[2];

	data->rx = scale * m_rotation[0];
	data->ry = scale * m_rotation[1];
	data->rz = scale * m_rotation[2];

	data->dt = 0.001f * (m_motionTime - m_prevMotionTime); // in seconds

	bool weHaveMotion = !nearHomePosition(data, m_deadZone);

	// determine what kind of motion event to send (Starting, InProgress, Finishing)
	// and where that leaves this NDOF manager (NotStarted, InProgress, Finished)
	switch (m_motionState) {
		case GHOST_kNotStarted:
		case GHOST_kFinished:
			if (weHaveMotion) {
				data->progress = GHOST_kStarting;
				m_motionState = GHOST_kInProgress;
				// prev motion time will be ancient, so just make up a reasonable time delta
				data->dt = 0.0125f;
			}
			else {
				// send no event and keep current state
				delete event;
				return false;
			}
			break;
		case GHOST_kInProgress:
			if (weHaveMotion) {
				data->progress = GHOST_kInProgress;
				// remain 'InProgress'
			}
			else {
				data->progress = GHOST_kFinishing;
				m_motionState = GHOST_kFinished;
			}
			break;
		default:
			; // will always be one of the above
	}

#ifdef DEBUG_NDOF_MOTION
	printf("ndof motion sent -- %s\n", progress_string[data->progress]);

	// show details about this motion event
	printf("    T=(%.2f,%.2f,%.2f) R=(%.2f,%.2f,%.2f) dt=%.3f\n",
	       data->tx, data->ty, data->tz,
	       data->rx, data->ry, data->rz,
	       data->dt);
#endif

	m_system.pushEvent(event);

	m_prevMotionTime = m_motionTime;

	return true;
}
