/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Blender Key loader library external interface
 */

#ifndef BLENKEY_H
#define BLENKEY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char byte;

typedef	struct UserStructType {
	char name[100];
	char email[100];
	char shopid[100];
	unsigned long reldate;
	int keytype;		// 1 = Individual, 2 = Corporate, 3 = Unlimited
	int keylevel;		// key disclosure level, starts at 1
	int keyformat;		// if we change the keyformat, up BLENKEYFORMAT
} UserStruct;

char *Hexify(byte *in, unsigned int length);
byte *DeHexify(char *in);

byte checkfunc0(byte a, byte b);
byte checkfunc1(byte a, byte b);
byte checkfunc2(byte a, byte b);
byte checkfunc3(byte a, byte b);
byte checkfunc4(byte a, byte b);

// the byte size of the checksum datablock
// #define MAXBYTEDATABLOCK 1000

#define BLENKEYMAGIC     "0ce0ba52"
#define BLENKEYSEPERATOR "---+++---"
#define BLENKEYFORMAT 1


int ReadKeyFile(char *filename, UserStruct *User,
				char **Priv, char **Pub, byte **Byte, char **Python);

#ifdef __cplusplus
}
#endif

#endif /* BLENKEY_H */

