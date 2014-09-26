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

/* Binary name to launch. */
#define BLENDER_BINARY L"blender-app.exe"

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <Shellapi.h>

#include "utfconv.h"

#include "BLI_utildefines.h"
#include "BLI_winstuff.h"

static void local_hacks_do(void)
{
	_putenv_s("OMP_WAIT_POLICY", "PASSIVE");
}

int main(int argc, const char **UNUSED(argv_c))
{
	PROCESS_INFORMATION processInformation = {0};
	STARTUPINFOW startupInfo = {0};
	BOOL result;
	wchar_t command[65536];
	int i, len = sizeof(command) / sizeof(wchar_t);
	wchar_t **argv_16 = CommandLineToArgvW(GetCommandLineW(), &argc);
	int argci = 0;

	local_hacks_do();

	wcsncpy(command, BLENDER_BINARY, len - 1);
	len -= wcslen(BLENDER_BINARY);
	for (i = 1; i < argc; ++i) {
		size_t argument_len = wcslen(argv_16[i]);
		wcsncat(command, L" \"", len - 2);
		wcsncat(command, argv_16[i], len - 3);
		len -= argument_len + 1;
		if (argv_16[i][argument_len - 1] == '\\') {
			wcsncat(command, L"\\", len - 1);
			len--;
		}
		wcsncat(command, L"\"", len - 1);
	}

	LocalFree(argv_16);

	startupInfo.cb = sizeof(startupInfo);
	result = CreateProcessW(NULL, command, NULL, NULL, TRUE,
	                        0, NULL, NULL,
	                        &startupInfo, &processInformation);

	if (!result) {
		fprintf(stderr, "%S\n", L"Error launching " BLENDER_BINARY);
		return EXIT_FAILURE;
	}

	WaitForSingleObject(processInformation.hProcess, INFINITE);

	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);

	return EXIT_SUCCESS;
}
