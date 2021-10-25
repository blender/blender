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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Windows-posix compatibility layer, windows-specific functions.
 */

/** \file blender/blenlib/intern/winstuff.c
 *  \ingroup bli
 */


#ifdef WIN32

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>

#include "MEM_guardedalloc.h"

#define WIN32_SKIP_HKEY_PROTECTION      // need to use HKEY
#include "BLI_winstuff.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "../blenkernel/BKE_global.h"  /* G.background, bad level include (no function calls) */

#include "utf_winfunc.h"
#include "utfconv.h"

/* FILE_MAXDIR + FILE_MAXFILE */

int BLI_getInstallationDir(char *str)
{
	char dir[FILE_MAXDIR];
	int a;
	/*change to utf support*/
	GetModuleFileName(NULL, str, FILE_MAX);
	BLI_split_dir_part(str, dir, sizeof(dir)); /* shouldn't be relative */
	a = strlen(dir);
	if (dir[a - 1] == '\\') dir[a - 1] = 0;
	
	strcpy(str, dir);
	
	return 1;
}

static void RegisterBlendExtension_Fail(HKEY root)
{
	printf("failed\n");
	if (root)
		RegCloseKey(root);
	if (!G.background)
		MessageBox(0, "Could not register file extension.", "Blender error", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
}

void RegisterBlendExtension(void)
{
	LONG lresult;
	HKEY hkey = 0;
	HKEY root = 0;
	BOOL usr_mode = false;
	DWORD dwd = 0;
	char buffer[256];

	char BlPath[MAX_PATH];
	char InstallDir[FILE_MAXDIR];
	char SysDir[FILE_MAXDIR];
	const char *ThumbHandlerDLL;
	char RegCmd[MAX_PATH * 2];
	char MBox[256];
	char *blender_app;
#ifndef _WIN64
	BOOL IsWOW64;
#endif

	printf("Registering file extension...");
	GetModuleFileName(0, BlPath, MAX_PATH);

	/* Replace the actual app name with the wrapper. */
	blender_app = strstr(BlPath, "blender-app.exe");
	if (blender_app != NULL) {
		strcpy(blender_app, "blender.exe");
	}

	/* root is HKLM by default */
	lresult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Classes", 0, KEY_ALL_ACCESS, &root);
	if (lresult != ERROR_SUCCESS) {
		/* try HKCU on failure */
		usr_mode = true;
		lresult = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Classes", 0, KEY_ALL_ACCESS, &root);
		if (lresult != ERROR_SUCCESS)
			RegisterBlendExtension_Fail(0);
	}

	lresult = RegCreateKeyEx(root, "blendfile", 0,
	                         NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);
	if (lresult == ERROR_SUCCESS) {
		strcpy(buffer, "Blender File");
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}
	if (lresult != ERROR_SUCCESS)
		RegisterBlendExtension_Fail(root);

	lresult = RegCreateKeyEx(root, "blendfile\\shell\\open\\command", 0,
	                         NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);
	if (lresult == ERROR_SUCCESS) {
		sprintf(buffer, "\"%s\" \"%%1\"", BlPath);
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}
	if (lresult != ERROR_SUCCESS)
		RegisterBlendExtension_Fail(root);

	lresult = RegCreateKeyEx(root, "blendfile\\DefaultIcon", 0,
	                         NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);
	if (lresult == ERROR_SUCCESS) {
		sprintf(buffer, "\"%s\", 1", BlPath);
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}
	if (lresult != ERROR_SUCCESS)
		RegisterBlendExtension_Fail(root);

	lresult = RegCreateKeyEx(root, ".blend", 0,
	                         NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);
	if (lresult == ERROR_SUCCESS) {
		strcpy(buffer, "blendfile");
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}
	if (lresult != ERROR_SUCCESS)
		RegisterBlendExtension_Fail(root);
	
	BLI_getInstallationDir(InstallDir);
	GetSystemDirectory(SysDir, FILE_MAXDIR);
#ifdef _WIN64
	ThumbHandlerDLL = "BlendThumb64.dll";
#else
	IsWow64Process(GetCurrentProcess(), &IsWOW64);
	if (IsWOW64 == true)
		ThumbHandlerDLL = "BlendThumb64.dll";
	else
		ThumbHandlerDLL = "BlendThumb.dll";
#endif
	snprintf(RegCmd, MAX_PATH * 2, "%s\\regsvr32 /s \"%s\\%s\"", SysDir, InstallDir, ThumbHandlerDLL);
	system(RegCmd);

	RegCloseKey(root);
	printf("success (%s)\n", usr_mode ? "user" : "system");
	if (!G.background) {
		sprintf(MBox, "File extension registered for %s.", usr_mode ? "the current user. To register for all users, run as an administrator" : "all users");
		MessageBox(0, MBox, "Blender", MB_OK | MB_ICONINFORMATION);
	}
	TerminateProcess(GetCurrentProcess(), 0);
}

void get_default_root(char *root)
{
	char str[MAX_PATH + 1];
	
	/* the default drive to resolve a directory without a specified drive 
	 * should be the Windows installation drive, since this was what the OS
	 * assumes. */
	if (GetWindowsDirectory(str, MAX_PATH + 1)) {
		root[0] = str[0];
		root[1] = ':';
		root[2] = '\\';
		root[3] = '\0';
	}
	else {
		/* if GetWindowsDirectory fails, something has probably gone wrong, 
		 * we are trying the blender install dir though */
		if (GetModuleFileName(NULL, str, MAX_PATH + 1)) {
			printf("Error! Could not get the Windows Directory - "
			       "Defaulting to Blender installation Dir!\n");
			root[0] = str[0];
			root[1] = ':';
			root[2] = '\\';
			root[3] = '\0';
		}
		else {
			DWORD tmp;
			int i;
			int rc = 0;
			/* now something has gone really wrong - still trying our best guess */
			printf("Error! Could not get the Windows Directory - "
			       "Defaulting to first valid drive! Path might be invalid!\n");
			tmp = GetLogicalDrives();
			for (i = 2; i < 26; i++) {
				if ((tmp >> i) & 1) {
					root[0] = 'a' + i;
					root[1] = ':';
					root[2] = '\\';
					root[3] = '\0';
					if (GetFileAttributes(root) != 0xFFFFFFFF) {
						rc = i;
						break;
					}
				}
			}
			if (0 == rc) {
				printf("ERROR in 'get_default_root': can't find a valid drive!\n");
				root[0] = 'C';
				root[1] = ':';
				root[2] = '\\';
				root[3] = '\0';
			}
		}
	}
}

/* UNUSED */
#if 0
int check_file_chars(char *filename)
{
	char *p = filename;
	while (*p) {
		switch (*p) {
			case ':':
			case '?':
			case '*':
			case '|':
			case '\\':
			case '/':
			case '\"':
				return 0;
				break;
		}

		p++;
	}
	return 1;
}
#endif

#else

/* intentionally empty for UNIX */

#endif
