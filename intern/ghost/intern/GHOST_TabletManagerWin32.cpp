// safe & friendly WinTab wrapper
// by Mike Erwin, July 2010

#include "GHOST_TabletManagerWin32.h"
#include "GHOST_WindowWin32.h"
#include "GHOST_System.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventButton.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_QUEUE_SIZE 100

static bool wintab_initialized = false;

// BEGIN from Wacom's Utils.h
typedef UINT ( API * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( API * WTOPENA ) ( HWND, LPLOGCONTEXTA, BOOL );
typedef BOOL ( API * WTCLOSE ) ( HCTX );
typedef BOOL ( API * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( API * WTPACKETSGET ) ( HCTX, int, LPVOID );
typedef BOOL ( API * WTPACKET ) ( HCTX, UINT, LPVOID );
// END

// the Wintab library
static HMODULE lib_Wintab;

// WinTab function pointers
static WTOPENA func_Open;
static WTCLOSE func_Close;
static WTINFOA func_Info;
static WTQUEUESIZESET func_QueueSizeSet;
static WTPACKETSGET func_PacketsGet;
static WTPACKET func_Packet;

static void print(AXIS const& t, char const* label = NULL)
	{
	const char* unitName[] = {"dinosaur","inch","cm","circle"};

	if (label)
		printf("%s: ", label);

	printf("%d to %d, %d.%d per %s\n",
		t.axMin, t.axMax,
		HIWORD(t.axResolution), LOWORD(t.axResolution),
		unitName[t.axUnits]);
	}

static void print(WTPKT packet)
	{
	if (packet == 0)
		puts("- nothing special");
	else
		{
		if (packet & PK_CONTEXT) puts("- reporting context");
		if (packet & PK_STATUS) puts("- status bits");
		if (packet & PK_TIME) puts("- time stamp");
		if (packet & PK_CHANGED) puts("- change bit vector");
		if (packet & PK_SERIAL_NUMBER) puts("- packet serial number");
		if (packet & PK_CURSOR) puts("- reporting cursor");
		if (packet & PK_BUTTONS) puts("- buttons");
		if (packet & PK_X) puts("- x axis");
		if (packet & PK_Y) puts("- y axis");
		if (packet & PK_Z) puts("- z axis");
		if (packet & PK_NORMAL_PRESSURE) puts("- tip pressure");
		if (packet & PK_TANGENT_PRESSURE) puts("- barrel pressure");
		if (packet & PK_ORIENTATION) puts("- orientation/tilt");
		if (packet & PK_ROTATION) puts("- rotation");
		}
	}

Tablet::Tablet(int tabletID)
	: id(tabletID)
	{
	GHOST_ASSERT(wintab_initialized, "Tablet objects should only be created by TabletManager");

	// query for overall capabilities and ranges
	func_Info(WTI_DEVICES, DVC_NAME, name);
	printf("\n-- tablet %d: %s --\n", id, name);

	puts("\nactive tablet area");
	AXIS xRange, yRange;
	func_Info(WTI_DEVICES + id, DVC_X, &xRange);
	func_Info(WTI_DEVICES + id, DVC_Y, &yRange);
	print(xRange,"x"); print(yRange,"y");
	size_x = xRange.axMax;
	size_y = yRange.axMax;

	func_Info(WTI_DEVICES + id, DVC_NCSRTYPES, &cursorCount);
	func_Info(WTI_DEVICES + id, DVC_FIRSTCSR, &cursorBase);
	printf("\nowns tools %d to %d\n", cursorBase, cursorBase + cursorCount - 1);

	func_Info(WTI_DEVICES + id, DVC_PKTDATA, &(allTools));
	puts("\nall tools have"); print(allTools);
	func_Info(WTI_DEVICES + id, DVC_CSRDATA, &(someTools));
	puts("some tools also have"); print(someTools);

	puts("\npressure sensitivity");
	AXIS pressureRange;
	hasPressure = (allTools|someTools) & PK_NORMAL_PRESSURE
		&& func_Info(WTI_DEVICES + id, DVC_NPRESSURE, &pressureRange)
		&& pressureRange.axMax;

	if (hasPressure)
		{
		print(pressureRange);
		pressureScale = 1.f / pressureRange.axMax;
		}
	else
		pressureScale = 0.f;

	puts("\ntilt sensitivity");
	AXIS tiltRange[3];
	hasTilt = (allTools|someTools) & PK_ORIENTATION
		&& func_Info(WTI_DEVICES + id, DVC_ORIENTATION, tiltRange)
		&& tiltRange[0].axResolution && tiltRange[1].axResolution;

	if (hasTilt)
		{
		// leave this code in place to help support tablets I haven't tested
		const char* axisName[] = {"azimuth","altitude","twist"};
		for (int i = 0; i < 3; ++i)
			print(tiltRange[i], axisName[i]);

		azimuthScale = 1.f / tiltRange[0].axMax;
		altitudeScale = 1.f / tiltRange[1].axMax;
		}
	else
		{
		puts("none");
		azimuthScale = altitudeScale = 0.f;
		}

	for (UINT i = cursorBase; i < cursorBase + cursorCount; ++i)
		{
		// what can each cursor do?

		UINT physID;
		func_Info(WTI_CURSORS + i, CSR_PHYSID, &physID);
		if (physID == 0)
			// each 'real' cursor has a physical ID
			//  (on Wacom at least, not tested with other vendors)
			continue;

		TCHAR name[40];
		func_Info(WTI_CURSORS + i, CSR_NAME, name);
		printf("\ncursor %d: %s\n", i, name);

		// always returns 'yes' so don't bother
		// BOOL active;
		// func_Info(WTI_CURSORS + i, CSR_ACTIVE, &active);
		// printf("active: %s\n", active ? "yes" : "no");

		WTPKT packet;
		func_Info(WTI_CURSORS + i, CSR_PKTDATA, &packet);
		// only report the 'special' bits that distinguish this cursor from the rest
		puts("packet support"); print(packet & someTools);

		puts("buttons:");
		BYTE buttons;
		BYTE sysButtonMap[32];
		func_Info(WTI_CURSORS + i, CSR_BUTTONS, &buttons);
		func_Info(WTI_CURSORS + i, CSR_SYSBTNMAP, sysButtonMap);
		for (int i = 0; i < buttons; ++i)
			printf("  %d -> %d\n", i, sysButtonMap[i]);
		}
	}

bool Tablet::ownsCursor(int cursor)
	{
	return (cursor - cursorBase) < cursorCount;
	}

TabletTool Tablet::toolForCursor(int cursor)
	{
	TabletTool tool = {TABLET_NONE,false,false};
	if (ownsCursor(cursor))
		{
		// try one way to classify cursor
		UINT cursorType = (cursor - cursorBase) % cursorCount;
		printf("%d mod %d = %d\n", cursor - cursorBase, cursorCount, cursorType);
		switch (cursorType)
			{
			case 0: // older Intuos tablets can track two cursors at once
			case 3: // so we test for both here
				tool.type = TABLET_MOUSE;
				break;
			case 1:
			case 4:
				tool.type = TABLET_PEN;
				break;
			case 2:
			case 5:
				tool.type = TABLET_ERASER;
				break;
			default:
				tool.type = TABLET_NONE;
			}

		#if 0
		// now try another way
		func_Info(WTI_CURSORS + cursor, CSR_TYPE, &cursorType);
		switch (cursorType & 0xf06)
			{
			case 0x802:
				puts("general stylus");
				break;
			case 0x902:
				puts("airbrush");
				break;
			case 0x804:
				puts("art pen");
				break;
			case 0x004:
				puts("4D mouse");
				break;
			case 0x006:
				puts("5-button puck");
				break;
			default:
				puts("???");
			}
		#endif

		WTPKT toolData;
		func_Info(WTI_CURSORS + cursor, CSR_PKTDATA, &toolData);
		// discard any stray capabilities
		// (sometimes cursors claim to be able to do things
		//    that their tablet doesn't support)
		toolData &= (allTools|someTools);
		// only report the 'special' bits that distinguish this cursor from the rest
		puts("packet support"); print(toolData & someTools);
		putchar('\n');

		if (tool.type == TABLET_MOUSE)
			{
			// don't always trust CSR_PKTDATA!
			tool.hasPressure = false;
			tool.hasTilt = false;
			}
		else
			{
			tool.hasPressure = toolData & PK_NORMAL_PRESSURE;
			tool.hasTilt = toolData & PK_ORIENTATION;
			}
		}

	return tool;
	}

GHOST_TabletManagerWin32::GHOST_TabletManagerWin32()
	: activeWindow(NULL)
	{
	dropTool();

	// open WinTab
	GHOST_ASSERT(!wintab_initialized,"There should be only one TabletManagerWin32 object");
	lib_Wintab = LoadLibrary("wintab32.dll");

	if (lib_Wintab)
		{
		// connect function pointers
		func_Open = (WTOPENA) GetProcAddress(lib_Wintab,"WTOpenA");
		func_Close = (WTCLOSE) GetProcAddress(lib_Wintab,"WTClose");
		func_Info = (WTINFOA) GetProcAddress(lib_Wintab,"WTInfoA");
		func_QueueSizeSet = (WTQUEUESIZESET) GetProcAddress(lib_Wintab,"WTQueueSizeSet");
		func_PacketsGet = (WTPACKETSGET) GetProcAddress(lib_Wintab,"WTPacketsGet");
		func_Packet = (WTPACKET) GetProcAddress(lib_Wintab,"WTPacket");

		wintab_initialized = true;

		getHardwareInfo();
		}
	}

GHOST_TabletManagerWin32::~GHOST_TabletManagerWin32()
	{
	// close WinTab
	wintab_initialized = false;
	FreeLibrary(lib_Wintab);
	}

void GHOST_TabletManagerWin32::getHardwareInfo()
	{
	puts("\n-- graphics tablet info --");

	WORD specV, implV;
	func_Info(WTI_INTERFACE, IFC_SPECVERSION, &specV);
	func_Info(WTI_INTERFACE, IFC_IMPLVERSION, &implV);
	printf("WinTab version %d.%d (%d.%d)\n\n",
		HIBYTE(specV), LOBYTE(specV), HIBYTE(implV), LOBYTE(implV));

	UINT extensionCount;
	func_Info(WTI_INTERFACE, IFC_NEXTENSIONS, &extensionCount);
	for (UINT i = 0; i < extensionCount; ++i)
		{
		TCHAR name[40];
		func_Info(WTI_EXTENSIONS + i, EXT_NAME, name);
		printf("extension %d: %s\n", i, name);
		}

	UINT deviceCount, cursorCount;
	func_Info(WTI_INTERFACE, IFC_NDEVICES, &deviceCount);
	func_Info(WTI_INTERFACE, IFC_NCURSORS, &cursorCount);
	printf("\n%d tablets, %d tools\n", deviceCount, cursorCount);

	if (deviceCount > 1)
		; // support this?

	for (UINT i = 0; i < deviceCount; ++i)
		{
		Tablet tablet(i);
		tablets.push_back(tablet);
		}

	putchar('\n');
	}

bool GHOST_TabletManagerWin32::available()
	{
	return lib_Wintab // driver installed
		&& func_Info(0,0,NULL); // tablet plugged in
	}

HCTX GHOST_TabletManagerWin32::contextForWindow(GHOST_WindowWin32* window)
	{
	std::map<GHOST_WindowWin32*,HCTX>::iterator i = contexts.find(window);
	if (i == contexts.end())
		return 0; // not found
	else
		return i->second;
	}

void GHOST_TabletManagerWin32::openForWindow(GHOST_WindowWin32* window)
	{
	if (contextForWindow(window) != 0)
		// this window already has a tablet context
		return;

	int id = 0; // for testing...

	// set up context
	LOGCONTEXT archetype;
//	func_Info(WTI_DEFSYSCTX, 0, &archetype);
//	func_Info(WTI_DEFCONTEXT, 0, &archetype);
	func_Info(WTI_DSCTXS + id, 0, &archetype);

	strcpy(archetype.lcName, "blender special");

	archetype.lcPktData = PACKETDATA;
 	archetype.lcPktMode = PACKETMODE;
	archetype.lcMoveMask = PACKETDATA;

//	archetype.lcOptions |= CXO_CSRMESSAGES; // <-- lean mean version
	archetype.lcOptions |= CXO_SYSTEM | CXO_MESSAGES | CXO_CSRMESSAGES;

	archetype.lcSysMode = 1; // relative mode (mouse acts like mouse?)

// BEGIN derived from Wacom's TILTTEST.C:
	AXIS TabletX, TabletY;
	func_Info(WTI_DEVICES + id,DVC_X,&TabletX);
	func_Info(WTI_DEVICES + id,DVC_Y,&TabletY);
	archetype.lcInOrgX = 0;
	archetype.lcInOrgY = 0;
	archetype.lcInExtX = TabletX.axMax;
	archetype.lcInExtY = TabletY.axMax;
    /* output the data in screen coords */
	archetype.lcOutOrgX = archetype.lcOutOrgY = 0;
	archetype.lcOutExtX = GetSystemMetrics(SM_CXSCREEN);
    /* move origin to upper left */
	archetype.lcOutExtY = -GetSystemMetrics(SM_CYSCREEN);
// END

	// open the context
	HCTX context = func_Open(window->getHWND(), &archetype, TRUE);

	// request a deep packet queue
	int tabletQueueSize = MAX_QUEUE_SIZE;
	while (!func_QueueSizeSet(context, tabletQueueSize))
		--tabletQueueSize;
	printf("tablet queue size: %d\n", tabletQueueSize);

	contexts[window] = context;
	activeWindow = window;
	}

void GHOST_TabletManagerWin32::closeForWindow(GHOST_WindowWin32* window)
	{
	HCTX context = contextForWindow(window);

	if (context)
		{
		func_Close(context);
		// also remove it from our books:
		contexts.erase(contexts.find(window));
		if (activeWindow == window)
			activeWindow = NULL;
		}
	}

void Tablet::attachData(PACKET const& packet, TabletToolData& data)
	{
	if (data.tool.hasPressure)
		{
		if (packet.pkNormalPressure)
			{
			data.pressure = pressureScale * packet.pkNormalPressure;
			printf(" %d%%", (int)(100 * data.pressure));
			}
		else
			data.tool.hasPressure = false;
		}

	if (data.tool.hasTilt)
		{
		ORIENTATION const& o = packet.pkOrientation;
		if (o.orAzimuth || o.orAltitude)
			{
			// this code used to live in GHOST_WindowWin32
			// now it lives here
		
			float vecLen;
			float altRad, azmRad;	/* in radians */
		
			/*
			from the wintab spec:
			orAzimuth	Specifies the clockwise rotation of the
			cursor about the z axis through a full circular range.
		
			orAltitude	Specifies the angle with the x-y plane
			through a signed, semicircular range.  Positive values
			specify an angle upward toward the positive z axis;
			negative values specify an angle downward toward the negative z axis.
		
			wintab.h defines .orAltitude as a UINT but documents .orAltitude
			as positive for upward angles and negative for downward angles.
			WACOM uses negative altitude values to show that the pen is inverted;
			therefore we cast .orAltitude as an (int) and then use the absolute value.
			*/
	
			/* convert raw fixed point data to radians */
			altRad = fabs(o.orAltitude) * altitudeScale * M_PI/2.0;
			azmRad = o.orAzimuth * azimuthScale * M_PI*2.0;
		
			/* find length of the stylus' projected vector on the XY plane */
			vecLen = cos(altRad);
		
			/* from there calculate X and Y components based on azimuth */
			data.tilt_x = sin(azmRad) * vecLen;
			data.tilt_y = sin(M_PI/2.0 - azmRad) * vecLen;
	
			if (fabs(data.tilt_x) < 0.001 && fabs(data.tilt_x) < 0.001)
				{
				// really should fix the initial test for tool.hasTilt,
				// not apply a band-aid here.
				data.tool.hasTilt = false;
				data.tilt_x = 0.f;
				data.tilt_y = 0.f;
				}
			else
				{
				// printf(" /%d,%d/", o.orAzimuth, o.orAltitude);
				printf(" /%.2f,%.2f/", data.tilt_x, data.tilt_y);
				}
			}
		else
			data.tool.hasTilt = false;
		}
	}

bool GHOST_TabletManagerWin32::convertButton(const UINT button, GHOST_TButtonMask& ghost)
	{
	switch (sysButtonMap[button])
		{
		case 1:
			ghost = GHOST_kButtonMaskLeft;
			break;
		case 4:
			ghost = GHOST_kButtonMaskRight;
			break;
		case 7:
			ghost = GHOST_kButtonMaskMiddle;
			break;
		// sorry, no Button4 or Button5
		// they each map to 0 (unused)
		default:
			return false;
		};

	return true;
	}

bool GHOST_TabletManagerWin32::processPackets(GHOST_WindowWin32* window)
	{
	if (window == NULL)
		window = activeWindow;

	HCTX context = contextForWindow(window);

	bool anyProcessed = false;

	if (context)
		{
//		PACKET packet;
//		func_Packet(context, serialNumber, &packet);
		PACKET packets[MAX_QUEUE_SIZE];
		int n = func_PacketsGet(context, MAX_QUEUE_SIZE, packets);
//		printf("processing %d packets\n", n);

		for (int i = 0; i < n; ++i)
			{
			PACKET const& packet = packets[i];
			TabletToolData data = {activeTool};

			int x = packet.pkX;
			int y = packet.pkY;
	
			if (activeTool.type == TABLET_MOUSE)
				{
				// until scaling is working better, use system cursor position instead
//				POINT systemPos;
//				GetCursorPos(&systemPos);
//				x = systemPos.x;
//				y = systemPos.y;

				if (x == prevMouseX && y == prevMouseY && packet.pkButtons == prevButtons)
					// don't send any "mouse hasn't moved" events
					continue;
				else {
					prevMouseX = x;
					prevMouseY = y;
					}
				}

			anyProcessed = true;

			// If we're using a digitizing context, we need to
			// move the on-screen cursor ourselves.
			// SetCursorPos(x, y);

			switch (activeTool.type)
				{
				case TABLET_MOUSE:
					printf("mouse");
					break;
				case TABLET_PEN:
					printf("pen");
					break;
				case TABLET_ERASER:
					printf("eraser");
					break;
				default:
					printf("???");
				}
	
			printf(" (%d,%d)", x, y);
	

			activeTablet->attachData(packet, data);

			// at this point, construct a GHOST event and push it into the queue!

			GHOST_System* system = (GHOST_System*) GHOST_ISystem::getSystem();

			// any buttons changed?
			UINT diff = prevButtons ^ packet.pkButtons;
			if (diff)
				{
				for (int i = 0; i < 32 /*GHOST_kButtonNumMasks*/; ++i)
					{
					UINT mask = 1 << i;

					if (diff & mask)
						{
						GHOST_TButtonMask e_button;
						if (convertButton(i, e_button))
							{
							GHOST_TEventType e_action = (packet.pkButtons & mask) ? GHOST_kEventButtonDown : GHOST_kEventButtonUp;
	
							GHOST_EventButton* e = new GHOST_EventButton(system->getMilliSeconds(), e_action, window, e_button);
							GHOST_TabletData& e_data = ((GHOST_TEventButtonData*) e->getData())->tablet;
							e_data.Active = (GHOST_TTabletMode) data.tool.type;
							e_data.Pressure = data.pressure;
							e_data.Xtilt = data.tilt_x;
							e_data.Ytilt = data.tilt_y;
	
							printf(" button %d %s\n", i, (e_action == GHOST_kEventButtonDown) ? "down" : "up");
	
							system->pushEvent(e);
							}
						else puts(" mystery button (discarded)");
						}
					}

				prevButtons = packet.pkButtons;
				}
			else
				{
				GHOST_EventCursor* e = new GHOST_EventCursor(system->getMilliSeconds(), GHOST_kEventCursorMove, window, x, y);

				// use older TabletData struct for testing until mine is in place
				GHOST_TabletData& e_data = ((GHOST_TEventCursorData*) e->getData())->tablet;
				e_data.Active = (GHOST_TTabletMode) data.tool.type;
				e_data.Pressure = data.pressure;
				e_data.Xtilt = data.tilt_x;
				e_data.Ytilt = data.tilt_y;

				puts(" move");

				system->pushEvent(e);
				}
			}
		}
	return anyProcessed;
	}

void GHOST_TabletManagerWin32::changeTool(GHOST_WindowWin32* window, UINT serialNumber)
	{
	puts("-- changing tool --");

	dropTool();

	HCTX context = contextForWindow(window);

	if (context)
		{
		activeWindow = window;

		PACKET packet;
		func_Packet(context, serialNumber, &packet);

		for (std::vector<Tablet>::iterator i = tablets.begin(); i != tablets.end(); ++i)
			if (i->ownsCursor(packet.pkCursor))
				{
				activeTablet = &(*i);
				activeTool = i->toolForCursor(packet.pkCursor);

				// remember user's custom button assignments for this tool
				func_Info(WTI_CURSORS + packet.pkCursor, CSR_SYSBTNMAP, sysButtonMap);

				break;
				}
		}
	}

void GHOST_TabletManagerWin32::dropTool()
	{
	activeTool.type = TABLET_NONE;
	activeTool.hasPressure = false;
	activeTool.hasTilt = false;

	prevMouseX = prevMouseY = 0;
	prevButtons = 0;

	activeWindow = NULL;
	}

bool GHOST_TabletManagerWin32::anyButtonsDown()
	{
	return prevButtons != 0;
	}
