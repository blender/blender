/**
 * Functions for writing windows avi-format files.
 *
 * $Id$
 *
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32

#define  INC_OLE2
#include <windows.h>
#include <windowsx.h>
#include <memory.h>
#include <mmsystem.h>
#include <vfw.h>

#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"

#include "render_types.h"
#include "render.h"

#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_writeavi.h"

#include "BIF_toolbox.h"

// this defines the compression type for
// the output video stream

AVICOMPRESSOPTIONS opts;

static int sframe;
static PAVIFILE pfile = NULL;
static int avifileinitdone = 0;
static PAVISTREAM psUncompressed = NULL, psCompressed = NULL;

// function definitions
static void init_bmi(BITMAPINFOHEADER *bmi);
static void opts_to_acd(AviCodecData *acd);
static void acd_to_opts(AviCodecData *acd);
static void free_opts_data();
static int open_avi_codec_file(char * name);

///////////////////////////////////////////////////////////////////////////
//
// silly default parameters
//
///////////////////////////////////////////////////////////////////////////

#define DEFAULT_WIDTH   240
#define DEFAULT_HEIGHT  120
#define DEFAULT_LENGTH  100
#define DEFAULT_SIZE    6
#define DEFAULT_COLOR   RGB(255,0,0)
#define XSPEED		7
#define YSPEED		5

///////////////////////////////////////////////////////////////////////////
//
// useful macros
//
///////////////////////////////////////////////////////////////////////////

#define ALIGNULONG(i)     ((i+3)&(~3))                  /* ULONG aligned ! */
#define WIDTHBYTES(i)     ((unsigned)((i+31)&(~31))/8)  /* ULONG aligned ! */
#define DIBWIDTHBYTES(bi) (int)WIDTHBYTES((int)(bi).biWidth * (int)(bi).biBitCount)
#define DIBPTR(lpbi) ((LPBYTE)(lpbi) + \
	    (int)(lpbi)->biSize + \
	    (int)(lpbi)->biClrUsed * sizeof(RGBQUAD) )

///////////////////////////////////////////////////////////////////////////
//
// custom video stream instance structure
//
///////////////////////////////////////////////////////////////////////////

typedef struct {

    //
    // The Vtbl must come first
    //
    IAVIStreamVtbl * lpvtbl;

    //
    //  private ball instance data
    //
    ULONG	ulRefCount;

    DWORD       fccType;        // is this audio/video

    int         width;          // size in pixels of each frame
    int         height;
    int         length;         // length in frames of the pretend AVI movie
    int         size;
    COLORREF    color;          // ball color

} AVIBALL, * PAVIBALL;

///////////////////////////////////////////////////////////////////////////
//
// custom stream methods
//
///////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AVIBallQueryInterface(PAVISTREAM ps, REFIID riid, LPVOID * ppvObj);
HRESULT STDMETHODCALLTYPE AVIBallCreate       (PAVISTREAM ps, LONG lParam1, LONG lParam2);
ULONG   STDMETHODCALLTYPE AVIBallAddRef       (PAVISTREAM ps);
ULONG   STDMETHODCALLTYPE AVIBallRelease      (PAVISTREAM ps);
HRESULT STDMETHODCALLTYPE AVIBallInfo         (PAVISTREAM ps, AVISTREAMINFOW * psi, LONG lSize);
LONG    STDMETHODCALLTYPE AVIBallFindSample (PAVISTREAM ps, LONG lPos, LONG lFlags);
HRESULT STDMETHODCALLTYPE AVIBallReadFormat   (PAVISTREAM ps, LONG lPos, LPVOID lpFormat, LONG *lpcbFormat);
HRESULT STDMETHODCALLTYPE AVIBallSetFormat    (PAVISTREAM ps, LONG lPos, LPVOID lpFormat, LONG cbFormat);
HRESULT STDMETHODCALLTYPE AVIBallRead         (PAVISTREAM ps, LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG * plBytes,LONG * plSamples);
HRESULT STDMETHODCALLTYPE AVIBallWrite        (PAVISTREAM ps, LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, DWORD dwFlags, LONG *plSampWritten, LONG *plBytesWritten);
HRESULT STDMETHODCALLTYPE AVIBallDelete       (PAVISTREAM ps, LONG lStart, LONG lSamples);
HRESULT STDMETHODCALLTYPE AVIBallReadData     (PAVISTREAM ps, DWORD fcc, LPVOID lp,LONG *lpcb);
HRESULT STDMETHODCALLTYPE AVIBallWriteData    (PAVISTREAM ps, DWORD fcc, LPVOID lp,LONG cb);

IAVIStreamVtbl AVIBallHandler = {
    AVIBallQueryInterface,
    AVIBallAddRef,
    AVIBallRelease,
    AVIBallCreate,
    AVIBallInfo,
    AVIBallFindSample,
    AVIBallReadFormat,
    AVIBallSetFormat,
    AVIBallRead,
    AVIBallWrite,
    AVIBallDelete,
    AVIBallReadData,
    AVIBallWriteData
};


//
// This is the function an application would call to create a PAVISTREAM to
// reference the ball.  Then the standard AVIStream function calls can be
// used to work with this stream.
//
PAVISTREAM WINAPI NewBall(void)
{
	static AVIBALL ball;
    PAVIBALL pball = &ball;

    //
    // Fill the function table
    //
    pball->lpvtbl = &AVIBallHandler;

    //
    // Call our own create code to create a new instance (calls AVIBallCreate)
    // For now, don't use any lParams.
    //
    pball->lpvtbl->Create((PAVISTREAM) pball, 0, 0);

    return (PAVISTREAM) pball;
}

///////////////////////////////////////////////////////////////////////////
//
// This function is called to initialize an instance of the bouncing ball.
//
// When called, we look at the information possibly passed in <lParam1>,
// if any, and use it to determine the length of movie they want. (Not
// supported by NewBall right now, but it could be).
//
///////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE AVIBallCreate(PAVISTREAM ps, LONG lParam1, LONG lParam2)
{
    PAVIBALL pball = (PAVIBALL) ps;

    //
    // what type of data are we? (audio/video/other stream)
    //
    pball->fccType = streamtypeVIDEO;

    //
    // We define lParam1 as being the length of movie they want us to pretend
    // to be.
    //
    if (lParam1)
	pball->length = (int) lParam1;
    else
	pball->length = DEFAULT_LENGTH;

    switch (pball->fccType) {

	case streamtypeVIDEO:
	    pball->color  = DEFAULT_COLOR;
	    pball->width  = DEFAULT_WIDTH;
	    pball->height = DEFAULT_HEIGHT;
	    pball->size   = DEFAULT_SIZE;
	    pball->ulRefCount = 1;	// note that we are opened once
	    return AVIERR_OK;           // success

	case streamtypeAUDIO:
	    return ResultFromScode(AVIERR_UNSUPPORTED); // we don't do audio

	default:
	    return ResultFromScode(AVIERR_UNSUPPORTED); // or anything else
    }
}


//
// Increment our reference count
//
ULONG STDMETHODCALLTYPE AVIBallAddRef(PAVISTREAM ps)
{
    PAVIBALL pball = (PAVIBALL) ps;
    return (++pball->ulRefCount);
}


//
// Decrement our reference count
//
ULONG STDMETHODCALLTYPE AVIBallRelease(PAVISTREAM ps)
{
    PAVIBALL pball = (PAVIBALL) ps;
    if (--pball->ulRefCount)
	return pball->ulRefCount;

    // Free any data we're keeping around - like our private structure
    GlobalFreePtr(pball);

    return 0;
}


//
// Fills an AVISTREAMINFO structure
//
HRESULT STDMETHODCALLTYPE AVIBallInfo(PAVISTREAM ps, AVISTREAMINFOW * psi, LONG lSize)
{
    PAVIBALL pball = (PAVIBALL) ps;

    if (lSize < sizeof(AVISTREAMINFO))
	return ResultFromScode(AVIERR_BUFFERTOOSMALL);

    _fmemset(psi, 0, (int)lSize);

    // Fill out a stream header with information about us.
    psi->fccType                = pball->fccType;
    psi->fccHandler             = mmioFOURCC('B','a','l','l');
    psi->dwScale                = 1;
    psi->dwRate                 = 15;
    psi->dwLength               = pball->length;
    psi->dwSuggestedBufferSize  = pball->height * ALIGNULONG(pball->width);
    psi->rcFrame.right          = pball->width;
    psi->rcFrame.bottom         = pball->height;
    CopyMemory((PVOID)psi->szName,
               (PVOID)L"Bouncing ball video",
               sizeof(L"Bouncing ball video"));

    return AVIERR_OK;
}

///////////////////////////////////////////////////////////////////////////
//
// AVIBallReadFormat: needs to return the format of our data.
//
///////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE AVIBallReadFormat   (PAVISTREAM ps, LONG lPos,LPVOID lpFormat,LONG *lpcbFormat)
{
    PAVIBALL pball = (PAVIBALL) ps;
    LPBITMAPINFO    lpbi = (LPBITMAPINFO) lpFormat;
	
    if (lpFormat == NULL || *lpcbFormat == 0) {
		*lpcbFormat = sizeof(BITMAPINFOHEADER);
		return AVIERR_OK;
    }
	
    if (*lpcbFormat < sizeof(BITMAPINFOHEADER))
		return ResultFromScode(AVIERR_BUFFERTOOSMALL);
	
    // This is a relatively silly example: we build up our
    // format from scratch every time.
	
	/*
    lpbi->bmiHeader.biSize              = sizeof(BITMAPINFOHEADER);
    lpbi->bmiHeader.biCompression       = BI_RGB;
    lpbi->bmiHeader.biWidth             = pball->width;
    lpbi->bmiHeader.biHeight            = pball->height;
    lpbi->bmiHeader.biBitCount          = 8;
    lpbi->bmiHeader.biPlanes            = 1;
    lpbi->bmiHeader.biClrUsed           = 2;
    lpbi->bmiHeader.biSizeImage         = pball->height * DIBWIDTHBYTES(lpbi->bmiHeader);
	*/
	
	//init_bmi(&lpbi->bmiHeader);

	memset(&lpbi->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	lpbi->bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
    lpbi->bmiHeader.biWidth             = pball->width;
    lpbi->bmiHeader.biHeight            = pball->height;
	lpbi->bmiHeader.biPlanes			= 1;
	lpbi->bmiHeader.biBitCount			= 24;
	lpbi->bmiHeader.biSizeImage         = pball->width * pball->height * sizeof(RGBTRIPLE);

	/*
    lpbi->bmiColors[0].rgbRed           = 0;
    lpbi->bmiColors[0].rgbGreen         = 0;
    lpbi->bmiColors[0].rgbBlue          = 0;
    lpbi->bmiColors[1].rgbRed           = GetRValue(pball->color);
    lpbi->bmiColors[1].rgbGreen         = GetGValue(pball->color);
    lpbi->bmiColors[1].rgbBlue          = GetBValue(pball->color);
	*/
	
    *lpcbFormat = sizeof(BITMAPINFOHEADER);
	
    return AVIERR_OK;
}

///////////////////////////////////////////////////////////////////////////
//
// AVIBallRead: needs to return the data for a particular frame.
//
///////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE AVIBallRead (PAVISTREAM ps, LONG lStart,LONG lSamples,LPVOID lpBuffer,LONG cbBuffer,LONG * plBytes,LONG * plSamples)
{
    PAVIBALL pball = (PAVIBALL) ps;
    LONG   lSize = pball->height * ALIGNULONG(pball->width); // size of frame
	// in bytes
    int x, y;
    HPSTR hp = lpBuffer;
    int xPos, yPos;
	
    // Reject out of range values
    if (lStart < 0 || lStart >= pball->length)
		return ResultFromScode(AVIERR_BADPARAM);
	
    // Did they just want to know the size of our data?
    if (lpBuffer == NULL || cbBuffer == 0)
		goto exit;
	
    // Will our frame fit in the buffer passed?
    if (lSize > cbBuffer)
		return ResultFromScode(AVIERR_BUFFERTOOSMALL);
	
    // Figure out the position of the ball.
    // It just bounces back and forth.
	
    xPos = 5 + XSPEED * (int) lStart;			    // x = x0 + vt
    xPos = xPos % ((pball->width - pball->size) * 2);	    // limit to 2xwidth
    if (xPos > (pball->width - pball->size))		    // reflect if
		xPos = 2 * (pball->width - pball->size) - xPos;	    //   needed
	
    yPos = 5 + YSPEED * (int) lStart;
    yPos = yPos % ((pball->height - pball->size) * 2);
    if (yPos > (pball->height - pball->size))
		yPos = 2 * (pball->height - pball->size) - yPos;
	
    //
    // Build a DIB from scratch by writing in 1's where the ball is, 0's
    // where it isn't.
    //
    // Notice that we just build it in the buffer we've been passed.
    //
    // This is pretty ugly, I have to admit.
    //
    for (y = 0; y < pball->height; y++)
	{
		if (y >= yPos && y < yPos + pball->size)
		{
			for (x = 0; x < pball->width; x++)
			{
				*hp++ = (BYTE) ((x >= xPos && x < xPos + pball->size) ? 1 : 0);
			}
		}
		else
		{
			for (x = 0; x < pball->width; x++)
			{
				*hp++ = 0;
			}
		}
		
		hp += pball->width - ALIGNULONG(pball->width);
    }
	
exit:
    // We always return exactly one frame
    if (plSamples)
		*plSamples = 1;
	
    // Return the size of our frame
    if (plBytes)
		*plBytes = lSize;
	
    return AVIERR_OK;
}


HRESULT STDMETHODCALLTYPE AVIBallQueryInterface(PAVISTREAM ps, REFIID riid, LPVOID * ppvObj)
{
    PAVIBALL pball = (PAVIBALL) ps;

    // We support the Unknown interface (everybody does) and our Stream
    // interface.

    if (_fmemcmp(riid, &IID_IUnknown, sizeof(GUID)) == 0)
        *ppvObj = (LPVOID)pball;

    else if (_fmemcmp(riid, &IID_IAVIStream, sizeof(GUID)) == 0)
        *ppvObj = (LPVOID)pball;

    else {
        *ppvObj = NULL;
        return ResultFromScode(E_NOINTERFACE);
    }

    AVIBallAddRef(ps);

    return AVIERR_OK;
}

LONG    STDMETHODCALLTYPE AVIBallFindSample (PAVISTREAM ps, LONG lPos, LONG lFlags)
{
    // The only format change is frame 0
    if ((lFlags & FIND_TYPE) == FIND_FORMAT) {
	if ((lFlags & FIND_DIR) == FIND_NEXT && lPos > 0)
	    return -1;	// no more format changes
	else
	    return 0;

    // FIND_KEY and FIND_ANY always return the same position because
    // every frame is non-empty and a key frame
    } else
        return lPos;
}

HRESULT STDMETHODCALLTYPE AVIBallReadData     (PAVISTREAM ps, DWORD fcc, LPVOID lp, LONG *lpcb)
{
    return ResultFromScode(AVIERR_UNSUPPORTED);
}

HRESULT STDMETHODCALLTYPE AVIBallSetFormat    (PAVISTREAM ps, LONG lPos, LPVOID lpFormat, LONG cbFormat)
{
    return ResultFromScode(AVIERR_UNSUPPORTED);
}

HRESULT STDMETHODCALLTYPE AVIBallWriteData    (PAVISTREAM ps, DWORD fcc, LPVOID lp, LONG cb)
{
    return ResultFromScode(AVIERR_UNSUPPORTED);
}

HRESULT STDMETHODCALLTYPE AVIBallWrite        (PAVISTREAM ps, LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, DWORD dwFlags, LONG *plSampWritten, LONG *plBytesWritten)
{
    return ResultFromScode(AVIERR_UNSUPPORTED);
}

HRESULT STDMETHODCALLTYPE AVIBallDelete       (PAVISTREAM ps, LONG lStart, LONG lSamples)
{
    return ResultFromScode(AVIERR_UNSUPPORTED);
}


//////////////////////////////////////
static void init_bmi(BITMAPINFOHEADER *bmi)
{
	memset(bmi, 0, sizeof(BITMAPINFOHEADER));
	bmi->biSize = sizeof(BITMAPINFOHEADER);
	bmi->biWidth = R.rectx;
	bmi->biHeight = R.recty;
	bmi->biPlanes = 1;
	bmi->biBitCount = 24;
	bmi->biSizeImage = bmi->biWidth * bmi->biHeight * sizeof(RGBTRIPLE);
}


static void opts_to_acd(AviCodecData *acd)
{
	acd->fccType = opts.fccType;
	acd->fccHandler = opts.fccHandler;
	acd->dwKeyFrameEvery = opts.dwKeyFrameEvery;
	acd->dwQuality = opts.dwQuality;
	acd->dwBytesPerSecond = opts.dwBytesPerSecond;
	acd->dwFlags = opts.dwFlags;
	acd->dwInterleaveEvery = opts.dwInterleaveEvery;
	acd->cbFormat = opts.cbFormat;
	acd->cbParms = opts.cbParms;

	if (opts.lpFormat && opts.cbFormat) {
		acd->lpFormat = MEM_mallocN(opts.cbFormat, "avi.lpFormat");
		memcpy(acd->lpFormat, opts.lpFormat, opts.cbFormat);
	}

	if (opts.lpParms && opts.cbParms) {
		acd->lpParms = MEM_mallocN(opts.cbParms, "avi.lpParms");
		memcpy(acd->lpParms, opts.lpParms, opts.cbParms);
	}
}


static void acd_to_opts(AviCodecData *acd)
{
	memset(&opts, 0, sizeof(opts));
	if (acd) {
		opts.fccType = acd->fccType;
		opts.fccHandler = acd->fccHandler;
		opts.dwKeyFrameEvery = acd->dwKeyFrameEvery;
		opts.dwQuality = acd->dwQuality;
		opts.dwBytesPerSecond = acd->dwBytesPerSecond;
		opts.dwFlags = acd->dwFlags;
		opts.dwInterleaveEvery = acd->dwInterleaveEvery;
		opts.cbFormat = acd->cbFormat;
		opts.cbParms = acd->cbParms;
		
		if (acd->lpFormat && acd->cbFormat) {
			opts.lpFormat = malloc(opts.cbFormat);
			memcpy(opts.lpFormat, acd->lpFormat, opts.cbFormat);
		}

		if (acd->lpParms && acd->cbParms) {
			opts.lpParms = malloc(opts.cbParms);
			memcpy(opts.lpParms, acd->lpParms, opts.cbParms);
		}
	}
}

static void free_opts_data()
{
	if (opts.lpFormat) {
		free(opts.lpFormat);
		opts.lpFormat = NULL;
	}
	if (opts.lpParms) {
		free(opts.lpParms);
		opts.lpParms = NULL;
	}
}

static int open_avi_codec_file(char * name)
{
	HRESULT hr;
	WORD wVer;
	BITMAPINFOHEADER bmi;
	AVISTREAMINFO strhdr;
	int ret_val = 0;

	wVer = HIWORD(VideoForWindowsVersion());
	if (wVer < 0x010a){
		// this is probably an obsolete check...
		ret_val = 1;
	} else {
		AVIFileInit();
		avifileinitdone++;

		hr = AVIFileOpen(&pfile,		// returned file pointer
				name,					// file name
				OF_WRITE | OF_CREATE,	// mode to open file with
				NULL);					// use handler determined

		if (hr != AVIERR_OK) {
			ret_val = 1;
		} else {
			// initialize the BITMAPINFOHEADER 
			init_bmi(&bmi);
			// and associate a stream with the input images
			memset(&strhdr, 0, sizeof(strhdr));
			strhdr.fccType                = streamtypeVIDEO;	// stream type
			if (G.scene->r.avicodecdata) {
				strhdr.fccHandler             = G.scene->r.avicodecdata->fccHandler;
			}
			strhdr.dwScale                = 1;
			strhdr.dwRate                 = R.r.frs_sec;
			strhdr.dwSuggestedBufferSize  = bmi.biSizeImage;
			SetRect(&strhdr.rcFrame, 0, 0,						// rectangle for stream
				(int) bmi.biWidth,
				(int) bmi.biHeight);

			// And create the stream
			hr = AVIFileCreateStream(
					pfile,		    // file pointer
					&psUncompressed,// returned stream pointer
					&strhdr);	    // stream header

			if (hr != AVIERR_OK) {
				ret_val = 1;
			} else {
				acd_to_opts(G.scene->r.avicodecdata);
			}
		}
	}

	return(ret_val);
}


void end_avi_codec(void)
{
	free_opts_data();

	if (psUncompressed) {
		AVIStreamClose(psUncompressed);
		psUncompressed = NULL;
	}

	if (psCompressed) {
		AVIStreamClose(psCompressed);
		psCompressed = NULL;
	}

	if (pfile) {
		AVIFileClose(pfile);
		pfile = NULL;
	}

	if (avifileinitdone > 0) {
		AVIFileExit();
		avifileinitdone--;
	}
}


void start_avi_codec(void)
{
	HRESULT hr;
	BITMAPINFOHEADER bmi;
	char name[2048];
	char bakname[2048];
	
	makeavistring(name);
	sframe = (G.scene->r.sfra);

	strcpy(bakname, name);
	strcat(bakname, ".bak");

	if (BLI_exists(name)) {
		BLI_move(name, bakname);
	}

	// initialize the BITMAPINFOHEADER 
	init_bmi(&bmi);

	if (open_avi_codec_file(name)) {
		error("Can not open file %s", name);
		G.afbreek = 1;
	} else {
		// now create a compressed stream from the uncompressed
		// stream and the compression options
		hr = AVIMakeCompressedStream(
				&psCompressed,	// returned stream pointer
				psUncompressed,	// uncompressed stream
				&opts,			// compression options
				NULL);			// Unknown...
		if (hr != AVIERR_OK) {
			error("Codec is locked or not supported.");
			G.afbreek = 1;
		} else {
			hr = AVIStreamSetFormat(psCompressed, 0,
					&bmi,				// stream format
					bmi.biSize +		// format size
					bmi.biClrUsed * sizeof(RGBQUAD));	// plus size of colormap
			if (hr != AVIERR_OK) {
				error("Codec is locked or not supported.");
				G.afbreek = 1;
			}
		}
	}

	if (G.afbreek != 1) {
		printf("Created win avi: %s\n", name);
		if (BLI_exists(bakname)) {
			BLI_delete(bakname, 0, 0);
		}
	} else {
		// close the darn thing and remove it.
		end_avi_codec();
		if (BLI_exists(name)) {
			BLI_delete(name, 0, 0);
		}
		if (BLI_exists(bakname)) {
			BLI_move(bakname, name);
		}
	}
}


void append_avi_codec(int frame)
{
	HRESULT hr;
	BITMAPINFOHEADER bmi;
	RGBTRIPLE *buffer, *to;
	int x, y;
	unsigned char *from;

	if (psCompressed) {
		// initialize the BITMAPINFOHEADER 
		init_bmi(&bmi);

		// copy pixels
		buffer = MEM_mallocN(bmi.biSizeImage, "append_win_avi");
		to = buffer;
		from = (unsigned char *) R.rectot;
		for (y = R.recty; y > 0 ; y--) {
			for (x = R.rectx; x > 0 ; x--) {
				to->rgbtRed   = from[0];
				to->rgbtGreen = from[1];
				to->rgbtBlue  = from[2];
				to++; from += 4;
			}
		}

		hr = AVIStreamWrite(
				psCompressed,	// stream pointer
				frame - sframe,	// frame number
				1,				// number to write
				(LPBYTE) buffer,// pointer to data
				bmi.biSizeImage,// size of this frame
				AVIIF_KEYFRAME,	// flags....
				NULL,
				NULL);

		MEM_freeN(buffer);

		if (hr != AVIERR_OK) {
			G.afbreek = 1;
		} else {
			printf ("added frame %3d (frame %3d in avi): ", frame, frame-sframe);
		}
	}
}


int get_codec_settings(void)
{
	char name[2048];
	int ret_val = 0;
	AVICOMPRESSOPTIONS *aopts[1] = {&opts};
	AviCodecData *acd = G.scene->r.avicodecdata;
	static PAVISTREAM psdummy;

	acd_to_opts(G.scene->r.avicodecdata);

	psdummy = NewBall();

	if (psdummy == NULL) {
		ret_val = 1;
	} else {
		if (!AVISaveOptions(NULL,
				ICMF_CHOOSE_KEYFRAME | ICMF_CHOOSE_DATARATE,
				1,
				&psdummy,
				(LPAVICOMPRESSOPTIONS *) &aopts))
		{
			ret_val = 1;
		} else {
			if (acd) {
				free_avicodecdata(acd);
			} else {
				acd = G.scene->r.avicodecdata = MEM_callocN(sizeof(AviCodecData), "AviCodecData");
			}

			opts_to_acd(acd);

			AVISaveOptionsFree(1, aopts);
			memset(&opts, 0, sizeof(opts));
		}
	}

	return(ret_val);
}

#endif // _WIN32
