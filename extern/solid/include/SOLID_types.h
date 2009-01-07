/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef SOLID_TYPES_H
#define SOLID_TYPES_H

#ifndef DECLSPEC
# ifdef WIN32
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC
# endif
#endif

#define DT_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
    

typedef unsigned short DT_Index;
typedef unsigned short DT_Count;
typedef unsigned int   DT_Size;
typedef float          DT_Scalar; 
typedef int            DT_Bool;

#define DT_FALSE 0
#define DT_TRUE  1

#define DT_CONTINUE 0
#define DT_DONE 1

typedef DT_Scalar DT_Vector3[3]; 
typedef DT_Scalar DT_Quaternion[4]; 

#endif
