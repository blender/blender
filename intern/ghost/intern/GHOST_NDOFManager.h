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
 
#ifndef _GHOST_NDOFMANAGER_H_
#define _GHOST_NDOFMANAGER_H_

#include "GHOST_System.h"


// #define DEBUG_NDOF_MOTION
// #define DEBUG_NDOF_BUTTONS

typedef enum {
	NDOF_UnknownDevice, // <-- motion will work fine, buttons are ignored

	// current devices
	NDOF_SpaceNavigator,
	NDOF_SpaceExplorer,
	NDOF_SpacePilotPro,
	NDOF_SpaceMousePro,

	// older devices
	NDOF_SpacePilot

	} NDOF_DeviceT;

// NDOF device button event types
typedef enum {
	// used internally, never sent
	NDOF_BUTTON_NONE,
	// these two are available from any 3Dconnexion device
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT,
	// standard views
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_BOTTOM,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_BACK,
	// more views
	NDOF_BUTTON_ISO1,
	NDOF_BUTTON_ISO2,
	// 90 degree rotations
	// these don't all correspond to physical buttons
	NDOF_BUTTON_ROLL_CW,
	NDOF_BUTTON_ROLL_CCW,
	NDOF_BUTTON_SPIN_CW,
	NDOF_BUTTON_SPIN_CCW,
	NDOF_BUTTON_TILT_CW,
	NDOF_BUTTON_TILT_CCW,
	// device control
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_PANZOOM,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	// general-purpose buttons
	// users can assign functions via keymap editor
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

	} NDOF_ButtonT;

class GHOST_NDOFManager
{
public:
	GHOST_NDOFManager(GHOST_System&);

	virtual ~GHOST_NDOFManager() {};

	// whether multi-axis functionality is available (via the OS or driver)
	// does not imply that a device is plugged in or being used
	virtual bool available() = 0;

	// each platform's device detection should call this
	// use standard USB/HID identifiers
	bool setDevice(unsigned short vendor_id, unsigned short product_id);

	// filter out small/accidental/uncalibrated motions by
	// setting up a "dead zone" around home position
	// set to 0 to disable
	// 0.1 is a safe and reasonable value
	void setDeadZone(float);

	// the latest raw axis data from the device
	// NOTE: axis data should be in blender view coordinates
	//       +X is to the right
	//       +Y is up
	//       +Z is out of the screen
	//       for rotations, look from origin to each +axis
	//       rotations are + when CCW, - when CW
	// each platform is responsible for getting axis data into this form
	// these values should not be scaled (just shuffled or flipped)
	void updateTranslation(short t[3], GHOST_TUns64 time);
	void updateRotation(short r[3], GHOST_TUns64 time);

	// the latest raw button data from the device
	// use HID button encoding (not NDOF_ButtonT)
	void updateButton(int button_number, bool press, GHOST_TUns64 time);
	void updateButtons(int button_bits, GHOST_TUns64 time);
	// NDOFButton events are sent immediately

	// processes and sends most recent raw data as an NDOFMotion event
	// returns whether an event was sent
	bool sendMotionEvent();

protected:
	GHOST_System& m_system;

private:
	void sendButtonEvent(NDOF_ButtonT, bool press, GHOST_TUns64 time, GHOST_IWindow*);
	void sendKeyEvent(GHOST_TKey, bool press, GHOST_TUns64 time, GHOST_IWindow*);

	NDOF_DeviceT m_deviceType;
	int m_buttonCount;
	int m_buttonMask;

	short m_translation[3];
	short m_rotation[3];
	int m_buttons; // bit field

	GHOST_TUns64 m_motionTime; // in milliseconds
	GHOST_TUns64 m_prevMotionTime; // time of most recent Motion event sent

	GHOST_TProgress m_motionState;
	bool m_motionEventPending;
	float m_deadZone; // discard motion with each component < this
};

#endif
