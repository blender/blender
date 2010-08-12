#include "GHOST_TabletManagerWin32.h"
#include "GHOST_WindowWin32.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PACKETDATA PK_CURSOR | PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_ORIENTATION
#define PACKETMODE PK_BUTTONS
// #define PACKETTILT PKEXT_ABSOLUTE

#include "pktdef.h"

#define MAX_QUEUE_SIZE 100

GHOST_TabletManagerWin32::GHOST_TabletManagerWin32()
	{
	dropTool();

	// open WinTab
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

		WORD specV, implV;
		func_Info(WTI_INTERFACE, IFC_SPECVERSION, &specV);
		func_Info(WTI_INTERFACE, IFC_IMPLVERSION, &implV);
		printf("WinTab version %d.%d (%d.%d)\n",
			HIBYTE(specV), LOBYTE(specV), HIBYTE(implV), LOBYTE(implV));

		// query for overall capabilities and ranges
		char tabletName[LC_NAMELEN];
		if (func_Info(WTI_DEVICES, DVC_NAME, tabletName))
			puts(tabletName);

		AXIS xRange, yRange;
		func_Info(WTI_DEVICES, DVC_X, &xRange);
		func_Info(WTI_DEVICES, DVC_Y, &yRange);

		printf("active area: %dx%d\n", xRange.axMax, yRange.axMax);

		func_Info(WTI_DEVICES, DVC_NCSRTYPES, &cursorCount);
		func_Info(WTI_DEVICES, DVC_FIRSTCSR, &cursorBase);

		AXIS pressureRange;
		hasPressure = func_Info(WTI_DEVICES, DVC_NPRESSURE, &pressureRange);// && pressureRange.axMax != 0;

		printf("pressure sensitivity: ");
		if (hasPressure)
			{
			printf("%d to %d\n", pressureRange.axMin, pressureRange.axMax);
			pressureScale = 1.f / pressureRange.axMax;
			}
		else
			{
			printf("none\n");
			pressureScale = 0.f;
			}

		printf("tilt sensitivity:\n");
		AXIS tiltRange[3];
		hasTilt = func_Info(WTI_DEVICES, DVC_ORIENTATION, tiltRange);

		if (hasTilt)
			{
			// cheat by using available data from Intuos4. test on other tablets!!!
			azimuthScale = 1.f / HIWORD(tiltRange[1].axResolution);
			altitudeScale = 1.f / tiltRange[1].axMax;
			printf("azi scale %f\n", azimuthScale);
			printf("alt scale %f\n", altitudeScale);

			// leave this code in place to help support tablets I haven't tested
			const char* axisName[] = {"azimuth","altitude","twist"};
			const char* unitName[] = {NULL,"inch","cm","circle"};
			for (int i = 0; i < 3; ++i)
				{
				AXIS const& t = tiltRange[i];
				if (t.axResolution)
					printf("%s: %d to %d values per %d.%d %s\n",
						axisName[i], t.axMin, t.axMax,
						HIWORD(t.axResolution), LOWORD(t.axResolution),
						unitName[t.axUnits]);
				}
			}
		else
			{
			printf("none\n");
			}

#if 0 // WTX_TILT -- cartesian tilt extension, no conversion needed
		// this isn't working for [mce], so let it rest for now
		printf("raw tilt sensitivity:\n");
		hasTilt = false;
		UINT tag = 0;
		UINT extensionCount;
		func_Info(WTI_INTERFACE, IFC_NEXTENSIONS, &extensionCount);
//		for (UINT i = 0; func_Info(WTI_EXTENSIONS + i, EXT_TAG, &tag); ++i)
		for (UINT i = 0; i < extensionCount; ++i)
			{
			printf("trying extension %d\n", i);
			func_Info(WTI_EXTENSIONS + i, EXT_TAG, &tag);
			if (tag == WTX_TILT)
				{
				hasTilt = true;
				break;
				}
			}

		if (hasTilt)
			{
			func_Info(WTI_EXTENSIONS + tag, EXT_MASK, &tiltMask);
			AXIS tiltRange[2];
			func_Info(WTI_EXTENSIONS + tag, EXT_AXES, tiltRange);
			printf("%d to %d along x\n", tiltRange[0].axMin, tiltRange[0].axMax);
			printf("%d to %d along y\n", tiltRange[1].axMin, tiltRange[1].axMax);
			tiltScaleX = 1.f / tiltRange[0].axMax;
			tiltScaleY = 1.f / tiltRange[1].axMax;
			}
		else
			{
			printf("none\n");
			tiltScaleX = tiltScaleY = 0.f;
			}
#endif // WTX_TILT
		}
	}

GHOST_TabletManagerWin32::~GHOST_TabletManagerWin32()
	{
	// close WinTab
	FreeLibrary(lib_Wintab);
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

	// set up context
	LOGCONTEXT archetype;
	func_Info(WTI_DEFSYSCTX, 0, &archetype);

	strcpy(archetype.lcName, "blender special");
	archetype.lcPktData = PACKETDATA;
	archetype.lcPktMode = PACKETMODE;
	archetype.lcOptions |= CXO_MESSAGES | CXO_CSRMESSAGES;

/*
	if (hasTilt)
		{
		archetype.lcPktData |= tiltMask;
		archetype.lcMoveMask |= tiltMask;
		}
*/

	// open the context
	HCTX context = func_Open(window->getHWND(), &archetype, TRUE);

	// request a deep packet queue
	int tabletQueueSize = MAX_QUEUE_SIZE;
	while (!func_QueueSizeSet(context, tabletQueueSize))
		--tabletQueueSize;
	printf("tablet queue size: %d\n", tabletQueueSize);

	contexts[window] = context;
	}

void GHOST_TabletManagerWin32::closeForWindow(GHOST_WindowWin32* window)
	{
	HCTX context = contextForWindow(window);

	if (context)
		{
		func_Close(context);
		// also remove it from our books:
		contexts.erase(contexts.find(window));
		}
	}

void GHOST_TabletManagerWin32::convertTilt(ORIENTATION const& ort, TabletToolData& data)
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
	altRad = fabs(ort.orAltitude) * altitudeScale * M_PI/2.0;
	azmRad = ort.orAzimuth * azimuthScale * M_PI*2.0;

	/* find length of the stylus' projected vector on the XY plane */
	vecLen = cos(altRad);

	/* from there calculate X and Y components based on azimuth */
	data.tilt_x = sin(azmRad) * vecLen;
	data.tilt_y = sin(M_PI/2.0 - azmRad) * vecLen;
	}

void GHOST_TabletManagerWin32::processPackets(HCTX context)
	{
	PACKET packets[MAX_QUEUE_SIZE];
	int n = func_PacketsGet(context, MAX_QUEUE_SIZE, packets);
//	printf("processing %d packets\n", n);

	for (int i = 0; i < n; ++i)
		{
		PACKET const& packet = packets[i];
		TabletToolData data = {activeTool};
		int x = packet.pkX;
		int y = packet.pkY;

		if (activeTool.type == TABLET_MOUSE)
			if (x == prevMouseX && y == prevMouseY)
				// don't send any "mouse hasn't moved" events
				continue;
			else {
				prevMouseX = x;
				prevMouseY = y;
				}

		// every packet from a WT_PACKET message comes from the same tool
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

		if (activeTool.hasPressure)
			{
			if (packet.pkNormalPressure)
				{
				data.pressure = pressureScale * packet.pkNormalPressure;
				printf(" %d%%", (int)(100 * data.pressure));
				}
			else
				data.tool.hasPressure = false;
			}

		if (activeTool.hasTilt)
			{
			// ORIENTATION const& tilt = packet.pkOrientation;
			// printf(" /%d,%d/", tilt.orAzimuth, tilt.orAltitude);
			convertTilt(packet.pkOrientation, data);

			// data.tilt_x = tiltScaleX * packet.pkTilt.tiltX;
			// data.tilt_y = tiltScaleY * packet.pkTilt.tiltY;
			printf(" /%.2f,%.2f/", data.tilt_x, data.tilt_y);
			}

		putchar('\n');
		}
	}

void GHOST_TabletManagerWin32::changeTool(HCTX context, UINT serialNumber)
	{
	puts("-- changing tool --");

	dropTool();

	PACKET packet;
	func_Packet(context, serialNumber, &packet);
	UINT cursor = (packet.pkCursor - cursorBase) % cursorCount;

	printf("%d mod %d = %d\n", packet.pkCursor - cursorBase, cursorCount, cursor);

	switch (cursor)
		{
		case 0: // older Intuos tablets can track two cursors at once
		case 3: // so we test for both here
			activeTool.type = TABLET_MOUSE;
			break;
		case 1:
		case 4:
			activeTool.type = TABLET_PEN;
			break;
		case 2:
		case 5:
			activeTool.type = TABLET_ERASER;
			break;
		default:
			activeTool.type = TABLET_NONE;
		}

	WTPKT toolData;
	func_Info(WTI_CURSORS + cursor, CSR_PKTDATA, &toolData);
	activeTool.hasPressure = toolData & PK_NORMAL_PRESSURE;
	activeTool.hasTilt = toolData & PK_ORIENTATION;
//	activeTool.hasTilt = toolData & tiltMask;

	if (activeTool.hasPressure)
		puts(" - pressure");

	if (activeTool.hasTilt)
		puts(" - tilt");

	// and just for fun:
	if (toolData & PK_BUTTONS)
		puts(" - buttons");
	}

void GHOST_TabletManagerWin32::dropTool()
	{
	activeTool.type = TABLET_NONE;
	activeTool.hasPressure = false;
	activeTool.hasTilt = false;
	
	prevMouseX = prevMouseY = 0;
	}
