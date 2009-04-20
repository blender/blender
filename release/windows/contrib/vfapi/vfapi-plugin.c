/*
 * VFAPI-Plugin
 *
 * Copyright (c) 2006 Peter Schlaile
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>

#define	VF_STREAM_VIDEO		0x00000001
#define	VF_STREAM_AUDIO		0x00000002
#define	VF_OK				0x00000000
#define	VF_ERROR			0x80004005

typedef	struct {
	DWORD	dwSize;
	DWORD	dwAPIVersion;	
	DWORD	dwVersion;	
	DWORD	dwSupportStreamType;	
	char	cPluginInfo[256];	
	char	cFileType[256]; 	
} VF_PluginInfo,*LPVF_PluginInfo;

typedef	DWORD	VF_FileHandle,*LPVF_FileHandle;

typedef	struct {
	DWORD	dwSize;			
	DWORD	dwHasStreams;		
} VF_FileInfo,*LPVF_FileInfo;

typedef	struct {
	DWORD	dwSize;			
	DWORD	dwLengthL;	
	DWORD	dwLengthH;	
	DWORD	dwRate;			
	DWORD	dwScale;		
	DWORD	dwWidth;		
	DWORD	dwHeight;		
	DWORD	dwBitCount;		
} VF_StreamInfo_Video,*LPVF_StreamInfo_Video;

typedef	struct {
	DWORD	dwSize;			
	DWORD	dwLengthL;	
	DWORD	dwLengthH;	
	DWORD	dwRate;			
	DWORD	dwScale;		
	DWORD	dwChannels;		
	DWORD	dwBitsPerSample;	
	DWORD	dwBlockAlign;		
} VF_StreamInfo_Audio,*LPVF_StreamInfo_Audio;

typedef	struct {
	DWORD	dwSize;			
	DWORD	dwFrameNumberL;	
	DWORD	dwFrameNumberH;	
	void	*lpData;		
	long	lPitch;			
} VF_ReadData_Video,*LPVF_ReadData_Video;

typedef	struct {
	DWORD	dwSize;			
	LONGLONG	dwSamplePos;	
	DWORD	dwSampleCount;		
	DWORD	dwReadedSampleCount;	
	DWORD	dwBufSize;		
	void	*lpBuf;			
} VF_ReadData_Audio,*LPVF_ReadData_Audio;

typedef	struct {
	DWORD	dwSize;			
	HRESULT (__stdcall *OpenFile)( 
		char *lpFileName, LPVF_FileHandle lpFileHandle );
	HRESULT (__stdcall *CloseFile)( VF_FileHandle hFileHandle );
	HRESULT (__stdcall *GetFileInfo)( VF_FileHandle hFileHandle,
					 LPVF_FileInfo lpFileInfo );
	HRESULT (__stdcall *GetStreamInfo)( VF_FileHandle hFileHandle,
					   DWORD dwStream,void *lpStreamInfo );
	HRESULT (__stdcall *ReadData)( VF_FileHandle hFileHandle,
				      DWORD dwStream,void *lpData ); 
} VF_PluginFunc,*LPVF_PluginFunc;

__declspec(dllexport) HRESULT vfGetPluginInfo( 
	LPVF_PluginInfo lpPluginInfo )
{
	if (!lpPluginInfo || lpPluginInfo->dwSize != sizeof(VF_PluginInfo)) {
		return VF_ERROR;
	}

	lpPluginInfo->dwAPIVersion = 1;
	lpPluginInfo->dwVersion = 1;
	lpPluginInfo->dwSupportStreamType = VF_STREAM_VIDEO;
	strcpy(lpPluginInfo->cPluginInfo, "Blender Frameserver");
	strcpy(lpPluginInfo->cFileType, 
	       "Blender Frame-URL-File (*.blu)|*.blu");

	return VF_OK;
}

static unsigned long getipaddress(const char * ipaddr)
{
        struct hostent  *host;
        unsigned long   ip;

        if (((ip = inet_addr(ipaddr)) == INADDR_NONE)
            && strcmp(ipaddr, "255.255.255.255") != 0) {
                if ((host = gethostbyname(ipaddr)) != NULL) {
                        memcpy(&ip, host->h_addr, sizeof(ip));
                }
        }

        return (ip);
}

static void my_send(SOCKET sock, char * str)
{
	send(sock, str, strlen(str), 0);
}

static int my_recv(SOCKET sock, char * line, int maxlen)
{
	int got = 0;
	int toget = maxlen;

	while (toget > 0) {
		got = recv(sock, line, toget, 0);
		if (got <= 0) {
			return got;
		}
		toget -= got;
		line += got;
	}
	return maxlen;
}

static int my_gets(SOCKET sock, char * line, int maxlen)
{
	int last_rval = 0;

	while (((last_rval = my_recv(sock, line, 1)) == 1) && maxlen > 0) {
		if (*line == '\n') {
			line++;
			*line = 0;
			break;
		} else {
			line++;
			maxlen--;
		}
	}
	return last_rval;
}

typedef struct conndesc_ {
	struct sockaddr_in      addr;
	int width;
	int height;
	int start;
	int end;
	int rate;
	int ratescale;
} conndesc;



HRESULT __stdcall VF_OpenFileFunc_Blen( 
	char *lpFileName, LPVF_FileHandle lpFileHandle )
{
	conndesc * rval;
	char * host;
	char * p;
	int port;
	SOCKET s_in;
	char buf[256];
	struct sockaddr_in      addr;
	FILE* fp;

	p = lpFileName;
	while (*p && *p != '.') p++;
	if (*p) p++;
	if (strcmp(p, "blu") != 0) {
		return VF_ERROR;
	}

	fp = fopen(lpFileName, "r");
	if (!fp) {
		return VF_ERROR;
	}
	fgets(buf, 256, fp);
	fclose(fp);

	host = buf;
	p = host;
	while (*p && *p != ':') p++;
	if (*p) p++;
	p[-1] = 0;
	port = atoi(p);
	if (!port) {
		port = 8080;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = getipaddress(host);

	s_in = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s_in < 0) {
		return VF_ERROR;
	}

	if (connect(s_in, (struct sockaddr*) &addr,
		    sizeof(addr)) < 0) {
		closesocket(s_in);
		return VF_ERROR;
	}

	rval = (conndesc*) malloc(sizeof(conndesc));

	rval->addr = addr;

	my_send(s_in, "GET /info.txt HTTP/1.0\n\n");

	for (;;) {
		char * key;
		char * val;

		if (my_gets(s_in, buf, 250) <= 0) {
			break;
		}

		key = buf;
		val = buf;
		while (*val && *val != ' ') val++;
		if (*val) {
			*val = 0;
			val++;
			
			if (strcmp(key, "width") == 0) {
				rval->width = atoi(val);
			} else if (strcmp(key, "height") == 0) {
				rval->height = atoi(val);
			} else if (strcmp(key, "start") == 0) {
				rval->start = atoi(val);
			} else if (strcmp(key, "end") == 0) {
				rval->end = atoi(val);
			} else if (strcmp(key, "rate") == 0) {
				rval->rate = atoi(val);
			} else if (strcmp(key, "ratescale") == 0) {
				rval->ratescale = atoi(val);
			}
		}
	}

	closesocket(s_in);

	*lpFileHandle = (VF_FileHandle) rval;

	return VF_OK;
}

HRESULT __stdcall VF_CloseFileFunc_Blen( 
	VF_FileHandle hFileHandle )
{
	free((conndesc*) hFileHandle);

	return VF_OK;
}

HRESULT __stdcall VF_GetFileInfoFunc_Blen( 
	VF_FileHandle hFileHandle,
	LPVF_FileInfo lpFileInfo )
{
	conndesc * c = (conndesc*) hFileHandle;
	if (c == 0) { 
		return VF_ERROR; 
	}

	if (lpFileInfo->dwSize != sizeof(VF_FileInfo)) {
		return VF_ERROR;
	}
	
	lpFileInfo->dwHasStreams = VF_STREAM_VIDEO;

	return VF_OK;
}

HRESULT __stdcall VF_GetStreamInfoFunc_Blen( 
	VF_FileHandle hFileHandle,
	DWORD dwStream,void *lpStreamInfo )
{
	conndesc * c = (conndesc*) hFileHandle;

	LPVF_StreamInfo_Video v = (LPVF_StreamInfo_Video) lpStreamInfo;

	if (c == 0 || dwStream != VF_STREAM_VIDEO || v == 0) { 
		return VF_ERROR;
	}

	v->dwLengthL = c->end - c->start;
	v->dwLengthH = 0;
	v->dwScale = c->ratescale;
	v->dwRate = c->rate;
	v->dwWidth = c->width;
	v->dwHeight = c->height;
	v->dwBitCount = 24;

	return VF_OK;
}

HRESULT __stdcall VF_ReadDataFunc_Blen( 
	VF_FileHandle hFileHandle,
	DWORD dwStream,void *lpData )
{
	char req[256];
	char buf[256];
	SOCKET s_in;
	int width;
	int height;
	int y;
	int rval;
	unsigned char * framebuf;

	conndesc * c = (conndesc*) hFileHandle;
	LPVF_ReadData_Video v = (LPVF_ReadData_Video) lpData;

	if (c == 0 || dwStream != VF_STREAM_VIDEO || v == 0) { 
		return VF_ERROR;
	}

	s_in = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s_in < 0) {
		return VF_ERROR;
	}

	if (connect(s_in, (struct sockaddr*) &c->addr,
		    sizeof(c->addr)) < 0) {
		goto errout;
	}

	sprintf(req, "GET /images/ppm/%d.ppm HTTP/1.0\n\n",
		(int) (v->dwFrameNumberL) + c->start);

	my_send(s_in, req);

	do {
		if (my_gets(s_in, buf, 256) <= 0) {
			goto errout;
		}
	} while (strcmp(buf, "P6\n") != 0);

	do {
                rval = my_gets(s_in, buf, 256); 
        } while ( (buf[0] == '#' || buf[0] == '\n') && rval >= 0);

        if (sscanf(buf, "%d %d\n", &width, &height) != 2) {
		goto errout;
        }

	if (width != c->width || height != c->height) {
		goto errout;
	}

	my_gets(s_in, buf, 256); /* 255 */

	framebuf = (unsigned char*) v->lpData;

	for (y = 0; y < height; y++) {
		unsigned char * p = framebuf + v->lPitch * y;
		unsigned char * e = p + width * 3;

		my_recv(s_in, (char*) p, width * 3);
		while (p != e) {
			unsigned char tmp = p[2];
			p[2] = p[0];
			p[0] = tmp;

			p += 3;
		}
	}
	closesocket(s_in);
	return VF_OK;
 errout:
	closesocket(s_in);
	return VF_ERROR;
}

__declspec(dllexport) HRESULT vfGetPluginFunc( 
	LPVF_PluginFunc lpPluginFunc )
{
	if (!lpPluginFunc || lpPluginFunc->dwSize != sizeof(VF_PluginFunc)) {
		return VF_ERROR;
	}

	lpPluginFunc->OpenFile = VF_OpenFileFunc_Blen;
	lpPluginFunc->CloseFile = VF_CloseFileFunc_Blen;
	lpPluginFunc->GetFileInfo = VF_GetFileInfoFunc_Blen;
	lpPluginFunc->GetStreamInfo = VF_GetStreamInfoFunc_Blen;
	lpPluginFunc->ReadData = VF_ReadDataFunc_Blen;

	return VF_OK;
}



