/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2006 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#if defined WIN32
#define WINDOWS_LEAN_AND_MEAN
#endif

#if !defined NULL
#define NULL 0
#endif

#if !defined HRESULT
#define HRESULT long
#endif

#if !defined DWORD
#define DWORD unsigned long
#endif

#if !defined S_OK
#define S_OK ((HRESULT)0L)
#endif

#if !defined BYTE
#define BYTE unsigned char
#endif

#if !defined WIN32
#define Sleep(time) sleep(time)
#endif

#if !defined FAILED
#define FAILED(Status) ((HRESULT)(Status)<0)
#endif

#include <iostream>
