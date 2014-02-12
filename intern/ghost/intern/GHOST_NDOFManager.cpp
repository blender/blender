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
static const char *progress_string[] =
{"not started", "starting", "in progress", "finishing", "finished"};
#endif

#ifdef DEBUG_NDOF_BUTTONS
static const char *ndof_button_names[] = {
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
	// keyboard emulation
	"NDOF_BUTTON_ESC",
	"NDOF_BUTTON_ALT",
	"NDOF_BUTTON_SHIFT",
	"NDOF_BUTTON_CTRL",
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

// shared by the latest 3Dconnexion hardware
// SpacePilotPro uses all of these
// smaller devices use only some, based on button mask
static const NDOF_ButtonT Modern3Dx_HID_map[] = {
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
	NDOF_BUTTON_ESC,
	NDOF_BUTTON_ALT,
	NDOF_BUTTON_SHIFT,
	NDOF_BUTTON_CTRL,
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_PANZOOM,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS
};

static const NDOF_ButtonT SpaceExplorer_HID_map[] = {
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_ESC,
	NDOF_BUTTON_ALT,
	NDOF_BUTTON_SHIFT,
	NDOF_BUTTON_CTRL,
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	NDOF_BUTTON_ROTATE
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
	NDOF_BUTTON_ESC,
	NDOF_BUTTON_ALT,
	NDOF_BUTTON_SHIFT,
	NDOF_BUTTON_CTRL,
	NDOF_BUTTON_FIT,
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_NONE // the CONFIG button -- what does it do?
};

static const NDOF_ButtonT Generic_HID_map[] = {
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

static const int genericButtonCount = sizeof(Generic_HID_map) / sizeof(NDOF_ButtonT);

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System& sys)
	: m_system(sys)
	, m_deviceType(NDOF_UnknownDevice) // each platform has its own device detection code
	, m_buttonCount(genericButtonCount)
	, m_buttonMask(0)
	, m_hidMap(Generic_HID_map)
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
	// call this function until it returns true
	// it's a good idea to stop calling it after that, as it will "forget"
	// whichever device it already found

	// default to safe generic behavior for "unknown" devices
	// unidentified devices will emit motion events like normal
	// rogue buttons do nothing by default, but can be customized by the user

	m_deviceType = NDOF_UnknownDevice;
	m_hidMap = Generic_HID_map;
	m_buttonCount = genericButtonCount;
	m_buttonMask = 0;

	// "mystery device" owners can help build a HID_map for their hardware
	// A few users have already contributed information about several older devices
	// that I don't have access to. Thanks!

	switch (vendor_id) {
		case 0x046D: // Logitech (3Dconnexion)
			switch (product_id) {
				// -- current devices --
				case 0xC626: // full-size SpaceNavigator
				case 0xC628: // the "for Notebooks" one
					puts("ndof: using SpaceNavigator");
					m_deviceType = NDOF_SpaceNavigator;
					m_buttonCount = 2;
					m_hidMap = Modern3Dx_HID_map;
					break;
				case 0xC627:
					puts("ndof: using SpaceExplorer");
					m_deviceType = NDOF_SpaceExplorer;
					m_buttonCount = 15;
					m_hidMap = SpaceExplorer_HID_map;
					break;
				case 0xC629:
					puts("ndof: using SpacePilot Pro");
					m_deviceType = NDOF_SpacePilotPro;
					m_buttonCount = 31;
					m_hidMap = Modern3Dx_HID_map;
					break;
				case 0xC62B:
					puts("ndof: using SpaceMouse Pro");
					m_deviceType = NDOF_SpaceMousePro;
					m_buttonCount = 27;
					// ^^ actually has 15 buttons, but their HID codes range from 0 to 26
					m_buttonMask = 0x07C0F137;
					m_hidMap = Modern3Dx_HID_map;
					break;

				// -- older devices --
				case 0xC625:
					puts("ndof: using SpacePilot");
					m_deviceType = NDOF_SpacePilot;
					m_buttonCount = 21;
					m_hidMap = SpacePilot_HID_map;
					break;
				case 0xC621:
					puts("ndof: using Spaceball 5000");
					m_deviceType = NDOF_Spaceball5000;
					m_buttonCount = 12;
					break;
				case 0xC623:
					puts("ndof: using SpaceTraveler");
					m_deviceType = NDOF_SpaceTraveler;
					m_buttonCount = 8;
					break;

				default:
					printf("ndof: unknown Logitech product %04hx\n", product_id);
			}
			break;
		default:
			printf("ndof: unknown device %04hx:%04hx\n", vendor_id, product_id);
	}

	if (m_buttonMask == 0)
		m_buttonMask = (int) ~(UINT_MAX << m_buttonCount);

#ifdef DEBUG_NDOF_BUTTONS
	printf("ndof: %d buttons -> hex:%X\n", m_buttonCount, m_buttonMask);
#endif

	return m_deviceType != NDOF_UnknownDevice;
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

void GHOST_NDOFManager::sendButtonEvent(NDOF_ButtonT button, bool press, GHOST_TUns64 time, GHOST_IWindow *window)
{
	GHOST_ASSERT(button > NDOF_BUTTON_NONE && button < NDOF_BUTTON_LAST,
	             "rogue button trying to escape NDOF manager");

	GHOST_EventNDOFButton *event = new GHOST_EventNDOFButton(time, window);
	GHOST_TEventNDOFButtonData *data = (GHOST_TEventNDOFButtonData *) event->getData();

	data->action = press ? GHOST_kPress : GHOST_kRelease;
	data->button = button;

#ifdef DEBUG_NDOF_BUTTONS
	printf("%s %s\n", ndof_button_names[button], press ? "pressed" : "released");
#endif

	m_system.pushEvent(event);
}

void GHOST_NDOFManager::sendKeyEvent(GHOST_TKey key, bool press, GHOST_TUns64 time, GHOST_IWindow *window)
{
	GHOST_TEventType type = press ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;
	GHOST_EventKey *event = new GHOST_EventKey(time, type, window, key);

#ifdef DEBUG_NDOF_BUTTONS
	printf("keyboard %s\n", press ? "down" : "up");
#endif

	m_system.pushEvent(event);
}

void GHOST_NDOFManager::updateButton(int button_number, bool press, GHOST_TUns64 time)
{
	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();

#ifdef DEBUG_NDOF_BUTTONS
	printf("ndof: button %d -> ", button_number);
#endif

	NDOF_ButtonT button = (button_number < m_buttonCount) ? m_hidMap[button_number] : NDOF_BUTTON_NONE;

	switch (button)
	{
		case NDOF_BUTTON_NONE:
#ifdef DEBUG_NDOF_BUTTONS
			printf("discarded\n");
#endif
			break;
		case NDOF_BUTTON_ESC: sendKeyEvent(GHOST_kKeyEsc, press, time, window); break;
		case NDOF_BUTTON_ALT: sendKeyEvent(GHOST_kKeyLeftAlt, press, time, window); break;
		case NDOF_BUTTON_SHIFT: sendKeyEvent(GHOST_kKeyLeftShift, press, time, window); break;
		case NDOF_BUTTON_CTRL: sendKeyEvent(GHOST_kKeyLeftControl, press, time, window); break;
		default: sendButtonEvent(button, press, time, window);
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
		// warn the rogue user/developer, but allow it
		GHOST_PRINTF("ndof: dead zone of %.2f is rather high...\n", dz);
	}
	m_deadZone = dz;

	GHOST_PRINTF("ndof: dead zone set to %.2f\n", dz);
}

static bool atHomePosition(GHOST_TEventNDOFMotionData *ndof)
{
#define HOME(foo) (ndof->foo == 0.f)
	return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
}

static bool nearHomePosition(GHOST_TEventNDOFMotionData *ndof, float threshold)
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

	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();

	if (window == NULL) {
		m_motionState = GHOST_kNotStarted; // avoid large 'dt' times when changing windows
		return false; // delivery will fail, so don't bother sending
	}

	GHOST_EventNDOFMotion *event = new GHOST_EventNDOFMotion(m_motionTime, window);
	GHOST_TEventNDOFMotionData *data = (GHOST_TEventNDOFMotionData *) event->getData();

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
	m_prevMotionTime = m_motionTime;

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
#ifdef DEBUG_NDOF_MOTION
				printf("ndof motion ignored -- %s\n", progress_string[data->progress]);
#endif
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
	printf("    T=(%d,%d,%d) R=(%d,%d,%d) raw\n",
	       m_translation[0], m_translation[1], m_translation[2],
	       m_rotation[0], m_rotation[1], m_rotation[2]);
	printf("    T=(%.2f,%.2f,%.2f) R=(%.2f,%.2f,%.2f) dt=%.3f\n",
	       data->tx, data->ty, data->tz,
	       data->rx, data->ry, data->rz,
	       data->dt);
#endif

	m_system.pushEvent(event);

	m_prevMotionTime = m_motionTime;

	return true;
}
