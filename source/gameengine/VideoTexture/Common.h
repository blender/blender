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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2006 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file VideoTexture/Common.h
 *  \ingroup bgevideotex
 */

#if defined WIN32
#define WINDOWS_LEAN_AND_MEAN
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef HRESULT
#define HRESULT long
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef WIN32
#define Sleep(time) sleep(time)
#endif

#ifndef FAILED
#define FAILED(Status) ((HRESULT)(Status)<0)
#endif

#include <iostream>
