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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma comment(linker, "/subsystem:windows")

/* Binary name to launch. */
#define BLENDER_BINARY "blender-app.exe"

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
	PROCESS_INFORMATION processInformation = {0};
	STARTUPINFOA startupInfo = {0};
	BOOL result;

	_putenv_s("OMP_WAIT_POLICY", "PASSIVE");

	startupInfo.cb = sizeof(startupInfo);
	result = CreateProcessA(NULL, BLENDER_BINARY, NULL, NULL, FALSE,
	                       0, NULL, NULL,
	                       &startupInfo, &processInformation);

	if (!result) {
		fprintf(stderr, "Error launching " BLENDER_BINARY "\n");
		return EXIT_FAILURE;
	}

	WaitForSingleObject(processInformation.hProcess, INFINITE);

	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);

	return EXIT_SUCCESS;
}
