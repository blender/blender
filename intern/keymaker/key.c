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

/* ex:ts=4 */

/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Blender Key loader library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blenkey.h"	/* external interface */
#include "key_internal.h"

char *Hexify(byte *in, unsigned int length) {
	unsigned int i;
	char Tstring[3];
	char *out = malloc((2*length + 1) * sizeof(char));
	sprintf(out, "%02X", in[0]);
	for (i=1; i<length; i++) {
		sprintf(Tstring, "%02X", in[i]);
		strcat(out, Tstring);
	}
	return (out);
}

byte *DeHexify(char *in) {
	size_t len = strlen(in);
	byte *out = malloc((len/2) * sizeof(byte));
	unsigned int hexedbyte;
	unsigned int i;
	char *inp = in;
	byte *outp = out;
	for (i=0; i<(len/2); i++) {
		sscanf(inp, "%2x", &hexedbyte);
		inp += 2;
		*outp = (byte) hexedbyte;
		outp++;
	}
	/* printf("\nlen=%d, string=[%s]\n", len, Hexify(out, len/2)); */
	return (out);
}

int from_hex(char c) {
	return (c<'A') ? (c-'0') : (c-'A'+10);
}

/* 5 ReadHex helper functions ------------------------------------------
 read one Hex byte (two characters) and skip newlines if necessary */
byte ReadHexByteFp(FILE *fh, int *newlinetracker) {
	unsigned char a;
	unsigned char a1, a2;
	/* read 2 bytes hexcode of ascii data type */
	fread(&a1, 1, 1, fh);
	fread(&a2, 1, 1, fh);
	a = 16 * (from_hex(a1)) + (from_hex(a2));
	/*printf("Char[%d] = %02X\n", *newlinetracker, a); */
	*newlinetracker += 2;
	/* skip the newlines */
	if (*newlinetracker == 72) {
		fseek(fh, 1, SEEK_CUR);
		*newlinetracker = 0;
		/*printf("LastChar = %02X\n", a); */
	}
	return((byte) a);
}
byte ReadHexByteCp(char **from) {
	int a;
	/* read 2 bytes hexcode of ascii data type */
	sscanf(*from, "%2x", &a);
	/*printf("Char = %02X\n", a); */
	*from += 2;
	return((byte) a);
}
/* Generic hex2int */
int HexToInt(int a) {
	if (a == 0x20) /* space, count as 0 ;-) */
		return 0;
	else
		return(a - '0');
}
/* Note: this is only to be used for the header type */
int HexToIntFp(FILE *fh, int *newlinetracker) {
	byte a = ReadHexByteFp(fh, newlinetracker);
	if (DEBUG) printf("%02X = %d\n", a, a); /* note: no HexToInt here */
	return(a);
}
int HexToIntCp(char **from) {
	byte a = ReadHexByteCp(from);
	if (DEBUG) printf("%02X = %d\n", a, a); /* note: no HexToInt here */
	return(a);
}
/* Note: this is only to be used for the header length */
int Hex5ToInt(byte a, byte b, byte c, byte d, byte e) {
	return(HexToInt((int) a) * 10000 +
		   HexToInt((int) b) * 1000 +
		   HexToInt((int) c) * 100 +
		   HexToInt((int) d) * 10 +
		   HexToInt((int) e));
}
/* Note: this is only to be used for the header length */
int Hex5ToIntFp(FILE *fh, int *newlinetracker) {
	byte a = ReadHexByteFp(fh, newlinetracker),
		 b = ReadHexByteFp(fh, newlinetracker),
		 c = ReadHexByteFp(fh, newlinetracker),
		 d = ReadHexByteFp(fh, newlinetracker),
		 e = ReadHexByteFp(fh, newlinetracker);
	if (DEBUG) printf("\n%02X%02X%02X%02X%02X = %d\n", a, b, c, d, e,
		Hex5ToInt(a, b, c, d, e));
	return(Hex5ToInt(a, b, c, d, e));
}
int Hex5ToIntCp(char **from) {
	byte a = ReadHexByteCp(from),
		 b = ReadHexByteCp(from),
		 c = ReadHexByteCp(from),
		 d = ReadHexByteCp(from),
		 e = ReadHexByteCp(from);
	if (DEBUG) printf("\n%02X%02X%02X%02X%02X = %d\n", a, b, c, d, e,
		Hex5ToInt(a, b, c, d, e));
	return(Hex5ToInt(a, b, c, d, e));
}
/* --------------------------------------------------------------------- */

/* return the biggest */
byte checkfunc0(byte a, byte b) {
	if (a > b) return a;
	else return b;
}
/* return |a-b| */
byte checkfunc1(byte a, byte b) {
	if (a > b) return a - b;
	else return b - a;
}
/* return the sum mod 256 */
byte checkfunc2(byte a, byte b) {
	return ((a + b) % 256);
}
/* return the multiplication mod 256 */
byte checkfunc3(byte a, byte b) {
	return ((a * b) % 256);
}
/* return a/b or 0 */
byte checkfunc4(byte a, byte b) {
	if (b != 0) return (a / b);
	else return 0;
}

char *scan_ascii(FILE *fh, UserStruct *User) {
	long ascii_size;
	char *ascii_data = NULL;
	char *mdhex = NULL;
	byte md[RIPEMD160_DIGEST_LENGTH];
	char string[1024];
	char dosnewlines = 0;
	int lines = 0;
	int oldftell;

	/* NOTE: fscanf is notorious for its buffer overflows. This must be
	 fixed some day, consider this a proof-of-concept version. */

	fscanf(fh, "%1000[^\n]", string);
	sscanf(string, "%*s %s %s %lu %d %d %d",
		   User->email,
		   User->shopid,
		   &User->reldate,
		   &User->keytype,
		   &User->keylevel,
		   &User->keyformat);

	if (User->keyformat <= BLENKEYFORMAT) {
		if (DEBUG) printf(
			"Email:[%s] ShopID:[%s] RelDate:[%lu] KeyType[%d] KeyLevel[%d]\n",
			User->email, User->shopid, User->reldate, User->keytype,
			User->keylevel);

		/* read /n/n
		 check if we're reading dow newlines...
		*/
		oldftell = ftell(fh);
		getc(fh);
		if ((ftell(fh) - oldftell) == 2) {
			/* yes ! */
			dosnewlines = 1;
		}
		getc(fh);

		fscanf(fh, "%1000[^\n]", string);
		strncpy(User->name, string, sizeof(User->name) - 1);

		if (DEBUG) printf("Name:[%s]\n", User->name);

		getc(fh);

		/* 4 lines read uptil now... */
		lines = 4;

		while (getc(fh) != EOF) {
			fscanf(fh, "%1000[^\n]", string);
			lines++;
			/* if (DEBUG) printf("%s\n", string); */
			if (strcmp(string, BLENKEYSEPERATOR) == 0) {
				getc(fh);
				break;
			}
		}
	
		/* fh now points at the start of the datablock */
		ascii_size = ftell(fh);

		if (dosnewlines) {
			/* if we were reading on dos
			 ftell will also count the ^M 's in the file; 
			 substract them
			*/
			ascii_size -= lines;
		}

		ascii_data = malloc((ascii_size+1) * sizeof(char));
		fseek(fh, 0, SEEK_SET);
		fread(ascii_data, sizeof(char), ascii_size, fh);
		ascii_data[ascii_size] = '\0';

		if (DEBUG)
			printf("asciiblock is %ld bytes long:\n[%s]\n", ascii_size, ascii_data);

		/* calculate the hash checksum */
		RIPEMD160(ascii_data, ascii_size, md);
		free(ascii_data);
		mdhex = Hexify(md, RIPEMD160_DIGEST_LENGTH);
	}

	return(mdhex);
}

char *ReadHexCryptedData(FILE *fh, int *newlinetracker) {
	int HexCryptedDataLength = Hex5ToIntFp(fh, newlinetracker);
	int DataType = HexToIntFp(fh, newlinetracker);
	char *HexCryptedData = malloc((HexCryptedDataLength+1) * sizeof(char));
	int i;

	if (DataType != 1) {
		/* printf("Error: unexpected datatype for HexCryptedData\n"); */
		free(HexCryptedData);
		HexCryptedData = 0;
	} else {
		for (i=0; i<(HexCryptedDataLength/2); i++) {
			sprintf(HexCryptedData+2*i, "%02X", ReadHexByteFp(fh, newlinetracker));
		}
	}

	return(HexCryptedData);
}

char *ReadHexCryptedKey(FILE *fh, int *newlinetracker) {
	int HexCryptedKeyLength = Hex5ToIntFp(fh, newlinetracker);
	int DataType = HexToIntFp(fh, newlinetracker);
	char *HexCryptedKey = malloc((HexCryptedKeyLength+1) * sizeof(char));
	int i;

	if (DataType != 2) {
		/* printf("Error: unexpected datatype for HexCryptedKey\n"); */
		free(HexCryptedKey);
		HexCryptedKey = 0;
	} else {
		for (i=0; i<(HexCryptedKeyLength/2); i++) {
			sprintf(HexCryptedKey+2*i, "%02X", ReadHexByteFp(fh, newlinetracker));
		}
	}

	return(HexCryptedKey);
}

/* NOTE: CHANGE THIS INTO A KEY OF OUR OWN */
void LoadRSApubKey(RSA *Pub) {
    static unsigned char n[] =
"\xD1\x12\x0C\x6A\x34\x0A\xCF\x4C\x6B\x34\xA9\x3C\xDD\x1A\x2A\x68"
"\x34\xE5\xB4\xA2\x08\xE8\x9F\xCE\x76\xEF\x4B\x92\x9B\x99\xB4\x57"
"\x72\x95\x78\x1D\x9E\x21\x1B\xF9\x5C\x1B\x0E\xC9\xD0\x89\x75\x28"
"\x08\x13\x6A\xD8\xA9\xC2\xA4\x31\x91\x53\x5A\xB9\x26\x71\x8C\x05";
    static unsigned char e[] =
"\x01\x00\x01";
/*
	static unsigned char e[] = "\x11";
    static unsigned char n[] =
"\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
"\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
"\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
"\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
"\xF5";
*/

	Pub->e = BN_bin2bn(e, sizeof(e)-1, Pub->e);
	Pub->n = BN_bin2bn(n, sizeof(n)-1, Pub->n);
}

byte *RSADecryptKey(char *HexCryptedKey) {
	byte *CryptedKey = NULL;
	byte *Key = NULL;
	int KeyLen;
	int CryptedKeyLen = strlen(HexCryptedKey)/2;
	RSA *Pub = NULL;

	/* Load RSA public key */
	Pub = RSA_new();
	if (Pub == NULL) {
		/* printf("Error in RSA_new\n"); */
	} else {
		LoadRSApubKey(Pub);

		Key = malloc(RSA_size(Pub) * sizeof(byte));

		CryptedKey = DeHexify(HexCryptedKey);

		KeyLen = RSA_public_decrypt(CryptedKeyLen, CryptedKey, Key, Pub,
									RSA_PKCS1_PADDING);
		if (DEBUG)
			printf("CryptedKeyLen = %d, KeyLen = %d\n", CryptedKeyLen, KeyLen);
		if (KeyLen == -1) {
#ifndef NDEBUG
			printf("Error in RSA_public_decrypt: %s\n", ERR_error_string(ERR_get_error(), NULL));
#endif
			free(Key);
			Key = NULL;
		}
	}

	return (Key);
}

char *DeCryptDatablock(byte *CryptKey, int keylen, char *HexCryptedData) {
	RC4_KEY key;
	byte *CryptedData = DeHexify(HexCryptedData);
	unsigned int datalen = strlen(HexCryptedData)/2;
	char *KeyDataString = malloc(datalen * sizeof(char));

	RC4_set_key(&key, keylen, CryptKey);
	RC4(&key, datalen,  CryptedData, KeyDataString);
	free(CryptedData);

	return(KeyDataString);
}

char *get_from_datablock(char **DataPtr, char *TypeString) {
	int tstringsize = Hex5ToIntCp(DataPtr);
	int tstringtype = HexToIntCp(DataPtr);
	char *HexString = NULL;

	if (atoi(TypeString) != tstringtype) {
		/* printf("Unexpected type %d, expected %s\n", tstringtype, TypeString); */
	} else {
		HexString = malloc((tstringsize+1) * sizeof(char));

		strncpy(HexString, *DataPtr, tstringsize);
		*DataPtr += tstringsize;
		HexString[tstringsize] = '\0';
	}

	return(HexString);	
}

int ReadKeyFile(char *filename, UserStruct *User,
				char **Priv, char **Pub, byte **Byte, char **Python) {
	FILE *rawkeyfile;
	char *HexAsciiHash = NULL, *HexCryptedData = NULL, *HexCryptedKey =
		NULL;
	int newlinetracker = 0; /* line position, counts from 0-71 */
	byte *CryptKey = NULL;
	char *KeyDataString = NULL;
	char *KeyDataPtr = NULL;
	char *HexByte = NULL;
	char *mdhex = NULL;
	int ret_val = 1;

	if ((rawkeyfile = fopen(filename, "rb")) == NULL) {
		/* printf("error, cannot read %s\n", filename); */
	} else {
		/* Scan and interpret the ASCII part */
		HexAsciiHash = scan_ascii(rawkeyfile, User);
		if (DEBUG) printf("\nHexHash: %s\n", HexAsciiHash);

		/* Read the HexCryptedData */
		HexCryptedData = ReadHexCryptedData(rawkeyfile, &newlinetracker);
		if (DEBUG) printf("\nHexCryptedData: %s\n", HexCryptedData);

		/* Read the HexCryptedKey */
		HexCryptedKey = ReadHexCryptedKey(rawkeyfile, &newlinetracker);
		if (DEBUG) printf("\nHexCryptedKey: %s\n", HexCryptedKey);

		/* close keyfile */
		fclose(rawkeyfile);

		if (HexAsciiHash && HexCryptedKey && HexCryptedData) {
			/* Decrypt HexCryptedKey */
			CryptKey = RSADecryptKey(HexCryptedKey);

			if (CryptKey) {
				/* Decrypt HexCryptedData */
				KeyDataString = DeCryptDatablock(CryptKey, 16, HexCryptedData);
				free(CryptKey);
				CryptKey = NULL;

				if (KeyDataString) {
					if (DEBUG) printf("\nKeyDataString: %s\n", KeyDataString);

					/* Extract data from KeyDataString */
					KeyDataPtr = KeyDataString;
					mdhex   = get_from_datablock(&KeyDataPtr, "01");
					*Priv   = get_from_datablock(&KeyDataPtr, "02");
					*Pub    = get_from_datablock(&KeyDataPtr, "03");
					HexByte = get_from_datablock(&KeyDataPtr, "04");
					if (HexByte) {
						*Byte = DeHexify(HexByte);
						free(HexByte);
						HexByte = NULL;

						*Python = get_from_datablock(&KeyDataPtr, "05");

						/* Check ascii hash */
						if (strcmp(mdhex, HexAsciiHash) != 0) {
							/* printf("Ascii part checksums do not match !\n");
							 printf("found: %s\n", mdhex);
							 printf("check: %s\n", HexAsciiHash);
							*/
							ret_val = 2;
						} else {
							if (DEBUG) printf("\nThe ascii part checksum matches\n");
							/* everything ok ! */
							ret_val = 0;
						}
						free(mdhex);
						mdhex = NULL;
					}

					free(KeyDataString);
					KeyDataString = NULL;
				}
			}
		}

		/* cleanup */

		if (HexAsciiHash) {
			free(HexAsciiHash);
			HexAsciiHash = NULL;
		}

		if (HexCryptedKey) {
			free(HexCryptedKey);
			HexCryptedKey = NULL;
		}

		if (HexCryptedData) {
			free(HexCryptedData);
			HexCryptedData = NULL;
		}
	}

	return (ret_val);
}

