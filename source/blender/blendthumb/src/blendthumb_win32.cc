/*
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
 */

/** \file
 * \ingroup blendthumb
 *
 * Thumbnail from Blend file extraction for MS-Windows.
 */

#include <math.h>
#include <new>
#include <shlwapi.h>
#include <string>
#include <thumbcache.h> /* for #IThumbnailProvider */

#include "Wincodec.h"

#include "blendthumb.hh"

#include "BLI_filereader.h"

#pragma comment(lib, "shlwapi.lib")

/**
 * This thumbnail provider implements #IInitializeWithStream to enable being hosted
 * in an isolated process for robustness.
 */
class CBlendThumb : public IInitializeWithStream, public IThumbnailProvider {
 public:
  CBlendThumb() : _cRef(1), _pStream(NULL)
  {
  }

  virtual ~CBlendThumb()
  {
    if (_pStream) {
      _pStream->Release();
    }
  }

  IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
  {
    static const QITAB qit[] = {
        QITABENT(CBlendThumb, IInitializeWithStream),
        QITABENT(CBlendThumb, IThumbnailProvider),
        {0},
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
    if (!cRef) {
      delete this;
    }
    return cRef;
  }

  /** IInitializeWithStream */
  IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

  /** IThumbnailProvider */
  IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

 private:
  long _cRef;
  IStream *_pStream; /* provided in Initialize(). */
};

HRESULT CBlendThumb_CreateInstance(REFIID riid, void **ppv)
{
  CBlendThumb *pNew = new (std::nothrow) CBlendThumb();
  HRESULT hr = pNew ? S_OK : E_OUTOFMEMORY;
  if (SUCCEEDED(hr)) {
    hr = pNew->QueryInterface(riid, ppv);
    pNew->Release();
  }
  return hr;
}

IFACEMETHODIMP CBlendThumb::Initialize(IStream *pStream, DWORD)
{
  if (_pStream != NULL) {
    /* Can only be initialized once. */
    return E_UNEXPECTED;
  }
  /* Take a reference to the stream. */
  return pStream->QueryInterface(&_pStream);
}

/**
 * #FileReader compatible wrapper around the Windows stream that gives access to the .blend file.
 */
typedef struct {
  FileReader reader;

  IStream *_pStream;
} StreamReader;

static ssize_t stream_read(FileReader *reader, void *buffer, size_t size)
{
  StreamReader *stream = (StreamReader *)reader;

  ULONG readsize;
  stream->_pStream->Read(buffer, size, &readsize);
  stream->reader.offset += readsize;

  return (ssize_t)readsize;
}

static off64_t stream_seek(FileReader *reader, off64_t offset, int whence)
{
  StreamReader *stream = (StreamReader *)reader;

  DWORD origin = STREAM_SEEK_SET;
  switch (whence) {
    case SEEK_CUR:
      origin = STREAM_SEEK_CUR;
      break;
    case SEEK_END:
      origin = STREAM_SEEK_END;
      break;
  }
  LARGE_INTEGER offsetI;
  offsetI.QuadPart = offset;
  ULARGE_INTEGER newPos;
  stream->_pStream->Seek(offsetI, origin, &newPos);
  stream->reader.offset = newPos.QuadPart;

  return stream->reader.offset;
}

static void stream_close(FileReader *reader)
{
  StreamReader *stream = (StreamReader *)reader;
  delete stream;
}

IFACEMETHODIMP CBlendThumb::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
  HRESULT hr = S_FALSE;

  StreamReader *file = new StreamReader;
  file->reader.read = stream_read;
  file->reader.seek = stream_seek;
  file->reader.close = stream_close;
  file->reader.offset = 0;
  file->_pStream = _pStream;

  file->reader.seek(&file->reader, 0, SEEK_SET);

  /* Extract thumbnail from stream. */
  Thumbnail thumb;
  if (blendthumb_create_thumb_from_file(&file->reader, &thumb) != BT_OK) {
    return S_FALSE;
  }

  /* Convert to BGRA for Windows. */
  for (int i = 0; i < thumb.width * thumb.height; i++) {
    std::swap(thumb.data[4 * i], thumb.data[4 * i + 2]);
  }

  *phbmp = CreateBitmap(thumb.width, thumb.height, 1, 32, thumb.data.data());
  if (!*phbmp) {
    return E_FAIL;
  }
  *pdwAlpha = WTSAT_ARGB;

  /* Scale down the thumbnail if required. */
  if ((unsigned)thumb.width > cx || (unsigned)thumb.height > cx) {
    float scale = 1.0f / (std::max(thumb.width, thumb.height) / (float)cx);
    LONG NewWidth = (LONG)(thumb.width * scale);
    LONG NewHeight = (LONG)(thumb.height * scale);

    IWICImagingFactory *pImgFac;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImgFac));

    IWICBitmap *WICBmp;
    hr = pImgFac->CreateBitmapFromHBITMAP(*phbmp, 0, WICBitmapUseAlpha, &WICBmp);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = NewWidth;
    bmi.bmiHeader.biHeight = -NewHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE *pBits;
    HBITMAP ResizedHBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&pBits, NULL, 0);
    hr = ResizedHBmp ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
      IWICBitmapScaler *pIScaler;
      hr = pImgFac->CreateBitmapScaler(&pIScaler);
      hr = pIScaler->Initialize(WICBmp, NewWidth, NewHeight, WICBitmapInterpolationModeFant);

      WICRect rect = {0, 0, NewWidth, NewHeight};
      hr = pIScaler->CopyPixels(&rect, NewWidth * 4, NewWidth * NewHeight * 4, pBits);

      if (SUCCEEDED(hr)) {
        DeleteObject(*phbmp);
        *phbmp = ResizedHBmp;
      }
      else {
        DeleteObject(ResizedHBmp);
      }

      pIScaler->Release();
    }
    WICBmp->Release();
    pImgFac->Release();
  }
  else {
    hr = S_OK;
  }
  return hr;
}
