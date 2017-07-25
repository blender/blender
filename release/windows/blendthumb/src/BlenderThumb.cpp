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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <new>

#pragma comment(lib, "shlwapi.lib")

// this thumbnail provider implements IInitializeWithStream to enable being hosted
// in an isolated process for robustness

class CBlendThumb : public IInitializeWithStream, public IThumbnailProvider
{
public:
	CBlendThumb() : _cRef(1), _pStream(NULL) {}

	virtual ~CBlendThumb()
	{
		if (_pStream)
		{
			_pStream->Release();
		}
	}

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] =
		{
			QITABENT(CBlendThumb, IInitializeWithStream),
			QITABENT(CBlendThumb, IThumbnailProvider),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		ULONG cRef = InterlockedDecrement(&_cRef);
		if (!cRef)
		{
			delete this;
		}
		return cRef;
	}

	// IInitializeWithStream
	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

	// IThumbnailProvider
	IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

private:
	long _cRef;
	IStream *_pStream;	 // provided during initialization.
};

HRESULT CBlendThumb_CreateInstance(REFIID riid, void **ppv)
{
	CBlendThumb *pNew = new (std::nothrow) CBlendThumb();
	HRESULT hr = pNew ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pNew->QueryInterface(riid, ppv);
		pNew->Release();
	}
	return hr;
}

// IInitializeWithStream
IFACEMETHODIMP CBlendThumb::Initialize(IStream *pStream, DWORD)
{
	HRESULT hr = E_UNEXPECTED;  // can only be inited once
	if (_pStream == NULL)
	{
		// take a reference to the stream if we have not been inited yet
		hr = pStream->QueryInterface(&_pStream);
	}
	return hr;
}

#include <math.h>
#include <zlib.h>
#include "Wincodec.h"
const unsigned char gzip_magic[3] = { 0x1f, 0x8b, 0x08 };

// IThumbnailProvider
IFACEMETHODIMP CBlendThumb::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	ULONG BytesRead;
	HRESULT hr = S_FALSE;
	LARGE_INTEGER SeekPos;

	// Compressed?	
	unsigned char in_magic[3];
	_pStream->Read(&in_magic,3,&BytesRead);
	bool gzipped = true;
	for ( int i=0; i < 3; i++ )
		if ( in_magic[i] != gzip_magic[i] )
		{
			gzipped = false;
			break;
		}

	if (gzipped)
	{
		// Zlib inflate
		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;

		// Get compressed file length
		SeekPos.QuadPart = 0;
		_pStream->Seek(SeekPos,STREAM_SEEK_END,NULL);

		// Get compressed and uncompressed size
		uLong source_size;
		uLongf dest_size;
		//SeekPos.QuadPart = -4; // last 4 bytes define size of uncompressed file
		//ULARGE_INTEGER Tell;
		//_pStream->Seek(SeekPos,STREAM_SEEK_END,&Tell);
		//source_size = (uLong)Tell.QuadPart + 4; // src
		//_pStream->Read(&dest_size,4,&BytesRead); // dest
		dest_size = 1024*70; // thumbnail is currently always inside the first 65KB...if it moves or enlargens this line will have to change or go!
		source_size = (uLong)max(SeekPos.QuadPart,dest_size); // for safety, assume no compression
		
		// Input
		Bytef* src = new Bytef[source_size];
		stream.next_in = (Bytef*)src;
		stream.avail_in = (uInt)source_size;
		
		// Output
		Bytef* dest = new Bytef[dest_size];
		stream.next_out = (Bytef*)dest;
		stream.avail_out = dest_size; 

		// IStream to src
		SeekPos.QuadPart = 0;
		_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);
		_pStream->Read(src,source_size,&BytesRead);
		
		// Do the inflation
		int err;
		err = inflateInit2(&stream,16); // 16 means "gzip"...nice!
		err = inflate(&stream, Z_FINISH);		
		err = inflateEnd(&stream);
				
		// Replace the IStream, which is read-only
		_pStream->Release();
		_pStream = SHCreateMemStream(dest,dest_size);
		
		delete[] src;
		delete[] dest;
	}

	// Blender version, early out if sub 2.5
	SeekPos.QuadPart = 9;
	_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);
	char version[4];
	version[3] = '\0';
	_pStream->Read(&version,3,&BytesRead);
	if ( BytesRead != 3)
		return E_UNEXPECTED;
	int iVersion = atoi(version);
	if ( iVersion < 250 )
		return S_FALSE;
	
	// 32 or 64 bit blend?
	SeekPos.QuadPart = 7;
	_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);

	char _PointerSize;
	_pStream->Read(&_PointerSize,1,&BytesRead);

	int PointerSize	= _PointerSize == '_' ? 4 : 8;
	int HeaderSize	= 16 + PointerSize;

	// Find and read thumbnail ("TEST") block
	SeekPos.QuadPart = 12;
	_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);
	int BlockOffset = 12;
	while ( _pStream )
	{
		// Scan current block
		char BlockName[5];
		BlockName[4] = '\0';
		int	BlockSize = 0;

		if (_pStream->Read(BlockName,4,&BytesRead) == S_OK && _pStream->Read((void*)&BlockSize,4,&BytesRead) == S_OK)
		{
			if ( strcmp (BlockName,"TEST") != 0 )
			{
				SeekPos.QuadPart = BlockOffset += HeaderSize + BlockSize;
				_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);
				continue;
			}
		}
		else break; // eof

		// Found the block
		SeekPos.QuadPart = BlockOffset + HeaderSize;
		_pStream->Seek(SeekPos,STREAM_SEEK_SET,NULL);

		int width, height;
		_pStream->Read((char*)&width,4,&BytesRead);
		_pStream->Read((char*)&height,4,&BytesRead);
		BlockSize -= 8;

		// Isolate RGBA data
		char* pRGBA = new char[BlockSize];
		_pStream->Read(pRGBA,BlockSize,&BytesRead);

		if (BytesRead != (ULONG)BlockSize)
			return E_UNEXPECTED;

		// Convert to BGRA for Windows
		for (int i=0; i < BlockSize; i+=4 )
		{
			#define RED_BYTE pRGBA[i]
			#define BLUE_BYTE pRGBA[i+2]

			char red = RED_BYTE;
			RED_BYTE = BLUE_BYTE;
			BLUE_BYTE = red;
		}
		
		// Flip vertically (Blender stores it upside-down)
		unsigned int LineSize = width*4;
		char* FlippedImage = new char[BlockSize];
		for (int i=0; i<height; i++)
		{
			if ( 0 != memcpy_s(&FlippedImage[ (height - i - 1)*LineSize ],LineSize,&pRGBA[ i*LineSize ],LineSize))
				return E_UNEXPECTED;
		}
		delete[] pRGBA;
		pRGBA = FlippedImage;

		// Create image
		*phbmp = CreateBitmap(width,height,1,32,pRGBA);
		if (!*phbmp)
			return E_FAIL;
		*pdwAlpha = WTSAT_ARGB; // it's actually BGRA, not sure why this works

		// Scale down if required
		if ( (unsigned)width > cx || (unsigned)height > cx )
		{
			float scale = 1.0f / (max(width,height) / (float)cx);
			LONG NewWidth = (LONG)(width *scale);
			LONG NewHeight = (LONG)(height *scale);

#ifdef _DEBUG
#if 1
			MessageBox(0,L"Attach now",L"Debugging",MB_OK);
#endif
#endif
			IWICImagingFactory *pImgFac;
			hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImgFac));
			
			IWICBitmap* WICBmp;
			hr = pImgFac->CreateBitmapFromHBITMAP(*phbmp,0,WICBitmapUseAlpha,&WICBmp);
			
			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
			bmi.bmiHeader.biWidth = NewWidth;
			bmi.bmiHeader.biHeight = -NewHeight;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			BYTE *pBits;
			HBITMAP ResizedHBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
			hr = ResizedHBmp ? S_OK : E_OUTOFMEMORY;
			if (SUCCEEDED(hr))
			{				
				IWICBitmapScaler* pIScaler;
				hr = pImgFac->CreateBitmapScaler(&pIScaler);
				hr = pIScaler->Initialize(WICBmp,NewWidth,NewHeight,WICBitmapInterpolationModeFant);
								
				WICRect rect = {0, 0, NewWidth, NewHeight};
				hr = pIScaler->CopyPixels(&rect, NewWidth * 4, NewWidth * NewHeight * 4, pBits);

				if (SUCCEEDED(hr))
				{
					DeleteObject(*phbmp);
					*phbmp = ResizedHBmp;
				}
				else
					DeleteObject(ResizedHBmp);

				pIScaler->Release();
			}
			WICBmp->Release();
			pImgFac->Release();
		}
		else
			hr = S_OK;
		break;
	}
	return hr;
}
