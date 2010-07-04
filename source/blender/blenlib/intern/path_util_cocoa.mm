/* path_util_cocoa.m
 *
 * Functions specific to osx that use API available only in Objective-C
 *
 *
 * $Id: util_cocoa.m 25007 2009-11-29 19:16:52Z kazanbas $
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Damien Plisson 2010
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */


#import <Cocoa/Cocoa.h>

#include <string.h>

#include "BLI_path_util.h"



/**
 * Gets the ~/Library/Application Data/Blender folder
 */
const char* BLI_osx_getBasePath(basePathesTypes pathType)
{
	static char tempPath[512] = "";
	
	NSAutoreleasePool *pool;
	NSString *basePath;
	NSArray *paths;
	
	pool = [[NSAutoreleasePool alloc] init];
	
	switch (pathType) {
			/* Standard pathes */
		case BasePath_Temporary:
			strcpy(tempPath, [NSTemporaryDirectory() cStringUsingEncoding:NSASCIIStringEncoding]);
			[pool drain];
			return tempPath;
			break;
			
			/* Blender specific pathes */
		case BasePath_BlenderShared:
			paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSLocalDomainMask, YES);
			if ([paths count] > 0)
				basePath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"Blender"];
			else { //Error
				basePath = @"";
			}
			break;
		case BasePath_BlenderUser:
			paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
			if ([paths count] > 0)
				basePath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"Blender"];
			else { //Error
				basePath = @"";
			}
			break;
		case BasePath_ApplicationBundle:
			basePath = [[NSBundle mainBundle] bundlePath];
			break;

		default:
			tempPath[0] = 0;
			[pool drain];
			return tempPath;
	}
		
	strcpy(tempPath, [basePath cStringUsingEncoding:NSASCIIStringEncoding]);
	
	[pool drain];
	return tempPath;
}