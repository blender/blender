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

// ex:ts=4

/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Blender Key loader library internal stuff
 */

#include "openssl/rand.h"
#include "openssl/rsa.h"
#include "openssl/ripemd.h"
#include "openssl/rc4.h"
#include "openssl/err.h"

#include "mt19937int.h" // Mersenne Twister (under artistic license)

#define MAXASCIIBLOCK 1000
#define MAXBYTEDATABLOCK 1000

#ifdef NDEBUG
	#define DEBUG 0
#else
	#define DEBUG 1
#endif

// keyloader and keymaker internal prototypes
int from_hex(char c);
byte ReadHexByteFp(FILE *fh, int *newlinetracker);
byte ReadHexByteCp(char **from);
int HexToInt(int a);
int HexToIntFp(FILE *fh, int *newlinetracker);
int HexToIntCp(char **from);
int Hex5ToInt(byte a, byte b, byte c, byte d, byte e);
int Hex5ToIntFp(FILE *fh, int *newlinetracker);
int Hex5ToIntCp(char **from);
int main(int argc, char **argv);

// keyloader only internal prototypes
char *scan_ascii(FILE *fh, UserStruct *User);
char *ReadHexCryptedData(FILE *fh, int *newlinetracker);
char *ReadHexCryptedKey(FILE *fh, int *newlinetracker);
void LoadRSApubKey(RSA *Pub);
byte *RSADecryptKey(char *HexCryptedKey);
char *DeCryptDatablock(byte *CryptKey, int keylen, char *HexCryptedData);
char *get_from_datablock(char **DataPtr, char *TypeString);
int Check_All_Byte_Calculus_Data(char *KeyBytePtr);

// keymaker only internal prototypes
void usage(void);
char *Create_Ascii_Part(int argc, char **argv);
void Create_User_RSA_Keys(unsigned int keylength,
						  char **rsaPrivString, char **rsaPubString);
char *Create_Byte_Calculus_Data(void);
byte *CreateCryptKey(unsigned int size);
char *CryptDatablock(byte *CryptKey, int keylen, char *KeyDataString);
char *RSACryptKey(RSA *rsa, byte *CryptKey, int KeyLen);
void add_to_datablock(char **DataString, char *HexString, char *TypeString);
void LoadRSAprivKey(RSA *Priv);
