#include "GHOST_TabletManagerWin32.h"
#include "GHOST_WindowWin32.h"
#include <stdio.h>
#include <stdlib.h>

#define PACKETDATA	PK_CURSOR | PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE
#define PACKETTILT 	PKEXT_ABSOLUTE
#define PACKETMODE	PK_BUTTONS

#include "pktdef.h"

#define MAX_QUEUE_SIZE 128

GHOST_TabletManagerWin32::GHOST_TabletManagerWin32()
	{
	resetActiveTool();

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

		WORD specV, implV;
		func_Info(WTI_INTERFACE, IFC_SPECVERSION, &specV);
		func_Info(WTI_INTERFACE, IFC_IMPLVERSION, &implV);
		printf("Wintab version %d.%d (%d.%d)\n",
			HIBYTE(specV), LOBYTE(specV), HIBYTE(implV), LOBYTE(implV));

		// query for overall capabilities and ranges
		char tabletName[LC_NAMELEN];
		if (func_Info(WTI_DEVICES, DVC_NAME, tabletName))
			puts(tabletName);

		AXIS xRange, yRange;
		func_Info(WTI_DEVICES, DVC_X, &xRange);
		func_Info(WTI_DEVICES, DVC_Y, &yRange);

		printf("active area: %dx%d\n", xRange.axMax, yRange.axMax);

		AXIS pressureRange;
		hasPressure = func_Info(WTI_DEVICES, DVC_NPRESSURE, &pressureRange) && pressureRange.axMax != 0;

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

		AXIS tiltRange;
		hasTilt = func_Info(WTI_DEVICES, DVC_ORIENTATION, &tiltRange) && tiltRange.axMax != 0;

		printf("tilt sensitivity: ");
		if (hasTilt)
			{
			printf("%d to %d\n", tiltRange.axMin, tiltRange.axMax);
			tiltScale = 1.f / tiltRange.axMax;
			}
		else
			{
			printf("none\n");
			tiltScale = 0.f;
			}
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

void GHOST_TabletManagerWin32::resetActiveTool()
	{
	activeTool.type = TABLET_NONE;
	activeTool.hasPressure = false;
	activeTool.hasTilt = false;
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
	func_Info(WTI_DEFCONTEXT, 0, &archetype);

	strcpy(archetype.lcName, "merwin special");
	archetype.lcPktData = PACKETDATA;
	archetype.lcPktMode = PACKETMODE;
	archetype.lcOptions |= CXO_SYSTEM | CXO_MESSAGES;

// BEGIN from Wacom's TILTTEST.c
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

void GHOST_TabletManagerWin32::processPackets(GHOST_WindowWin32* window)
	{
	HCTX context = contextForWindow(window);

	if (context)
		{
		PACKET packets[MAX_QUEUE_SIZE];
		int n = func_PacketsGet(context, MAX_QUEUE_SIZE, packets);
		printf("processing %d packets from ", n);

		// every packet from a WT_PACKET message comes from the same tool
		switch (packets[0].pkCursor) {
			case 0: /* first device */
			case 3: /* second device */
				activeTool.type = TABLET_MOUSE;
				puts("mouse");
				break;
			case 1:
			case 4:
				activeTool.type = TABLET_PEN;
				puts("pen");
				break;
			case 2:
			case 5:
				activeTool.type = TABLET_ERASER;
				puts("eraser");
				break;
		}

		for (int i = 0; i < n; ++i)
			{
			PACKET const& packet = packets[i];
			TabletToolData data = {activeTool};
			int x = packet.pkX;
			int y = packet.pkY;

			if (data.tool.hasPressure)
				{
				if (packet.pkNormalPressure)
					data.pressure = pressureScale * packet.pkNormalPressure;
				else
					data.tool.hasPressure = false;
				}

			if (data.tool.hasTilt)
				{
				data.tilt_x = tiltScale * packet.pkTilt.tiltX;
				data.tilt_y = tiltScale * packet.pkTilt.tiltY;
				}

			printf("  %.3f @ (%d,%d) /%.2f,%.2f/\n", data.pressure, x, y, data.tilt_x, data.tilt_y);
			}
		}
	}

void GHOST_TabletManagerWin32::changeTool(GHOST_WindowWin32* window)
	{
	HCTX context = contextForWindow(window);

	if (context)
		{
		puts("-- changing tool --");

		if (hasPressure)
			{
			puts(" - pressure");
			activeTool.hasPressure = true; // not necessarily, but good enough for testing
			}

		if (hasTilt)
			{
			puts(" - tilt");
			activeTool.hasTilt = true;
			}

#if 0
#define		kTransducerDeviceIdBitMask 				0x0001
#define		kTransducerAbsXBitMask 						0x0002
#define		kTransducerAbsYBitMask 						0x0004
#define		kTransducerVendor1BitMask					0x0008
#define		kTransducerVendor2BitMask 					0x0010
#define		kTransducerVendor3BitMask 					0x0020
#define		kTransducerButtonsBitMask					0x0040
#define		kTransducerTiltXBitMask 					0x0080
#define		kTransducerTiltYBitMask 					0x0100
#define		kTransducerAbsZBitMask 						0x0200
#define		kTransducerPressureBitMask		 			0x0400
#define		kTransducerTangentialPressureBitMask 	0x0800
#define		kTransducerOrientInfoBitMask 				0x1000
#define		kTransducerRotationBitMask	 				0x2000

		// this is what I really want to know:
//		UINT active;
//		UINT active2 = func_Info(WTI_CURSORS, CSR_ACTIVE, &active);
//		printf("active: %d %d\n", active, active2);

		WTPKT toolData;
		func_Info(WTI_CURSORS, CSR_PKTDATA, &toolData);
		activeTool.hasPressure = toolData & PK_NORMAL_PRESSURE;
		activeTool.hasTilt = toolData & PK_ORIENTATION;

//		UINT cap;
//		UINT cap2 = func_Info(WTI_CURSORS, CSR_CAPABILITIES, &cap);
		// capabilities same as Mac tablet code? Let's see...
		//		int cap = CGEventGetIntegerValueField(event, kCGTabletProximityEventCapabilityMask);
		printf("cursor capabilities: %d %d\n", cap, cap2);

				if (cap & kTransducerDeviceIdBitMask)
					printf("  - device id\n");
				if (cap & kTransducerAbsXBitMask)
					printf("  - abs x\n");
				if (cap & kTransducerAbsYBitMask)
					printf("  - abs y\n");
				if (cap & kTransducerAbsZBitMask)
					printf("  - abs z\n");
				if (cap & kTransducerVendor1BitMask)
					printf("  - vendor 1\n");
				if (cap & kTransducerVendor2BitMask)
					printf("  - vendor 2\n");
				if (cap & kTransducerVendor3BitMask)
					printf("  - vendor 3\n");
				if (cap & kTransducerButtonsBitMask)
					printf("  - buttons\n");
				if (cap & kTransducerTiltXBitMask)
					{
					printf("  - tilt x\n");
					hasTilt = true;
					}
				if (cap & kTransducerTiltYBitMask)
					{
					printf("  - tilt y\n");
					hasTilt = true;
					}
				if (cap & kTransducerPressureBitMask)
					{
					printf("  - pressure\n");
					hasPressure = true;
					}
				if (cap & kTransducerTangentialPressureBitMask)
					printf("  - tangential pressure\n");
				if (cap & kTransducerOrientInfoBitMask)
					printf("  - orientation\n");
				if (cap & kTransducerRotationBitMask)
					printf("  - rotation\n");
#endif
		}
	}
