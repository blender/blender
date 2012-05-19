#ifndef CLCC_CLEW_HPP_INCLUDED
#define CLCC_CLEW_HPP_INCLUDED

//////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2009 Organic Vectory B.V.
//  Written by George van Venrooij
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file license.txt)
//////////////////////////////////////////////////////////////////////////

//! \file clew.h
//! \brief OpenCL run-time loader header
//!
//! This file contains a copy of the contents of CL.H and CL_PLATFORM.H from the 
//! official OpenCL spec. The purpose of this code is to load the OpenCL dynamic
//! library at run-time and thus allow the executable to function on many
//! platforms regardless of the vendor of the OpenCL driver actually installed.
//! Some of the techniques used here were inspired by work done in the GLEW
//! library (http://glew.sourceforge.net/)

//  Run-time dynamic linking functionality based on concepts used in GLEW
#ifdef  __OPENCL_CL_H
#error cl.h included before clew.h
#endif

#ifdef  __OPENCL_CL_PLATFORM_H
#error cl_platform.h included before clew.h
#endif

#ifndef CLCC_GENERATE_DOCUMENTATION
//  Prevent cl.h inclusion
#define __OPENCL_CL_H
//  Prevent cl_platform.h inclusion
#define __CL_PLATFORM_H
#endif  //  CLCC_GENERATE_DOCUMENTATION

/*******************************************************************************
* Copyright (c) 2008-2009 The Khronos Group Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and/or associated documentation files (the
* "Materials"), to deal in the Materials without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Materials, and to
* permit persons to whom the Materials are furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Materials.
*
* THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
******************************************************************************/
#ifdef __APPLE__
/* Contains #defines for AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER below */
#include <AvailabilityMacros.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLCC_GENERATE_DOCUMENTATION

#if defined(_WIN32)
#define CL_API_ENTRY
#define CL_API_CALL __stdcall
#else
#define CL_API_ENTRY
#define CL_API_CALL
#endif

#if defined(__APPLE__)
//JBKK removed for compatibility with blender trunk #define CL_API_SUFFIX__VERSION_1_0   AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER
#define CL_API_SUFFIX__VERSION_1_0
#define CL_EXTENSION_WEAK_LINK       __attribute__((weak_import))
#else
#define CL_API_SUFFIX__VERSION_1_0
#define CL_EXTENSION_WEAK_LINK                         
#endif

#if defined(_WIN32) && defined(_MSC_VER)

/* scalar types  */
typedef signed   __int8         cl_char;
typedef unsigned __int8         cl_uchar;
typedef signed   __int16        cl_short;
typedef unsigned __int16        cl_ushort;
typedef signed   __int32        cl_int;
typedef unsigned __int32        cl_uint;
typedef signed   __int64        cl_long;
typedef unsigned __int64        cl_ulong;

typedef unsigned __int16        cl_half;
typedef float                   cl_float;
typedef double                  cl_double;


/*
* Vector types 
*
*  Note:   OpenCL requires that all types be naturally aligned. 
*          This means that vector types must be naturally aligned.
*          For example, a vector of four floats must be aligned to
*          a 16 byte boundary (calculated as 4 * the natural 4-byte 
*          alignment of the float).  The alignment qualifiers here
*          will only function properly if your compiler supports them
*          and if you don't actively work to defeat them.  For example,
*          in order for a cl_float4 to be 16 byte aligned in a struct,
*          the start of the struct must itself be 16-byte aligned. 
*
*          Maintaining proper alignment is the user's responsibility.
*/
typedef signed   __int8          cl_char2[2];
typedef signed   __int8          cl_char4[4];
typedef signed   __int8          cl_char8[8];
typedef signed   __int8          cl_char16[16];
typedef unsigned __int8         cl_uchar2[2];
typedef unsigned __int8         cl_uchar4[4];
typedef unsigned __int8         cl_uchar8[8];
typedef unsigned __int8         cl_uchar16[16];

typedef signed   __int16         cl_short2[2];
typedef signed   __int16         cl_short4[4];
typedef signed   __int16         cl_short8[8];
typedef signed   __int16         cl_short16[16];
typedef unsigned __int16        cl_ushort2[2];
typedef unsigned __int16        cl_ushort4[4];
typedef unsigned __int16        cl_ushort8[8];
typedef unsigned __int16        cl_ushort16[16];

typedef signed   __int32         cl_int2[2];
typedef signed   __int32         cl_int4[4];
typedef signed   __int32         cl_int8[8];
typedef signed   __int32         cl_int16[16];
typedef unsigned __int32        cl_uint2[2];
typedef unsigned __int32        cl_uint4[4];
typedef unsigned __int32        cl_uint8[8];
typedef unsigned __int32        cl_uint16[16];

typedef signed   __int64         cl_long2[2];
typedef signed   __int64         cl_long4[4];
typedef signed   __int64         cl_long8[8];
typedef signed   __int64         cl_long16[16];
typedef unsigned __int64        cl_ulong2[2];
typedef unsigned __int64        cl_ulong4[4];
typedef unsigned __int64        cl_ulong8[8];
typedef unsigned __int64        cl_ulong16[16];

typedef float           cl_float2[2];
typedef float           cl_float4[4];
typedef float           cl_float8[8];
typedef float           cl_float16[16];

typedef double          cl_double2[2];
typedef double          cl_double4[4];
typedef double          cl_double8[8];
typedef double          cl_double16[16];
/* There are no vector types for half */

#else

#include <stdint.h>

/* scalar types  */
typedef int8_t          cl_char;
typedef uint8_t         cl_uchar;
typedef int16_t         cl_short    __attribute__((aligned(2)));
typedef uint16_t        cl_ushort   __attribute__((aligned(2)));
typedef int32_t         cl_int      __attribute__((aligned(4)));
typedef uint32_t        cl_uint     __attribute__((aligned(4)));
typedef int64_t         cl_long     __attribute__((aligned(8)));
typedef uint64_t        cl_ulong    __attribute__((aligned(8)));

typedef uint16_t        cl_half     __attribute__((aligned(2)));
typedef float           cl_float    __attribute__((aligned(4)));
typedef double          cl_double   __attribute__((aligned(8)));

/*
* Vector types 
*
*  Note:   OpenCL requires that all types be naturally aligned. 
*          This means that vector types must be naturally aligned.
*          For example, a vector of four floats must be aligned to
*          a 16 byte boundary (calculated as 4 * the natural 4-byte 
*          alignment of the float).  The alignment qualifiers here
*          will only function properly if your compiler supports them
*          and if you don't actively work to defeat them.  For example,
*          in order for a cl_float4 to be 16 byte aligned in a struct,
*          the start of the struct must itself be 16-byte aligned. 
*
*          Maintaining proper alignment is the user's responsibility.
*/
typedef int8_t          cl_char2[2]     __attribute__((aligned(2)));
typedef int8_t          cl_char4[4]     __attribute__((aligned(4)));
typedef int8_t          cl_char8[8]     __attribute__((aligned(8)));
typedef int8_t          cl_char16[16]   __attribute__((aligned(16)));
typedef uint8_t         cl_uchar2[2]    __attribute__((aligned(2)));
typedef uint8_t         cl_uchar4[4]    __attribute__((aligned(4)));
typedef uint8_t         cl_uchar8[8]    __attribute__((aligned(8)));
typedef uint8_t         cl_uchar16[16]  __attribute__((aligned(16)));

typedef int16_t         cl_short2[2]     __attribute__((aligned(4)));
typedef int16_t         cl_short4[4]     __attribute__((aligned(8)));
typedef int16_t         cl_short8[8]     __attribute__((aligned(16)));
typedef int16_t         cl_short16[16]   __attribute__((aligned(32)));
typedef uint16_t        cl_ushort2[2]    __attribute__((aligned(4)));
typedef uint16_t        cl_ushort4[4]    __attribute__((aligned(8)));
typedef uint16_t        cl_ushort8[8]    __attribute__((aligned(16)));
typedef uint16_t        cl_ushort16[16]  __attribute__((aligned(32)));

typedef int32_t         cl_int2[2]      __attribute__((aligned(8)));
typedef int32_t         cl_int4[4]      __attribute__((aligned(16)));
typedef int32_t         cl_int8[8]      __attribute__((aligned(32)));
typedef int32_t         cl_int16[16]    __attribute__((aligned(64)));
typedef uint32_t        cl_uint2[2]     __attribute__((aligned(8)));
typedef uint32_t        cl_uint4[4]     __attribute__((aligned(16)));
typedef uint32_t        cl_uint8[8]     __attribute__((aligned(32)));
typedef uint32_t        cl_uint16[16]   __attribute__((aligned(64)));

typedef int64_t         cl_long2[2]     __attribute__((aligned(16)));
typedef int64_t         cl_long4[4]     __attribute__((aligned(32)));
typedef int64_t         cl_long8[8]     __attribute__((aligned(64)));
typedef int64_t         cl_long16[16]   __attribute__((aligned(128)));
typedef uint64_t        cl_ulong2[2]    __attribute__((aligned(16)));
typedef uint64_t        cl_ulong4[4]    __attribute__((aligned(32)));
typedef uint64_t        cl_ulong8[8]    __attribute__((aligned(64)));
typedef uint64_t        cl_ulong16[16]  __attribute__((aligned(128)));

typedef float           cl_float2[2]    __attribute__((aligned(8)));
typedef float           cl_float4[4]    __attribute__((aligned(16)));
typedef float           cl_float8[8]    __attribute__((aligned(32)));
typedef float           cl_float16[16]  __attribute__((aligned(64)));

typedef double          cl_double2[2]   __attribute__((aligned(16)));
typedef double          cl_double4[4]   __attribute__((aligned(32)));
typedef double          cl_double8[8]   __attribute__((aligned(64)));
typedef double          cl_double16[16] __attribute__((aligned(128)));

/* There are no vector types for half */

#endif

/******************************************************************************/

// Macro names and corresponding values defined by OpenCL

#define CL_CHAR_BIT         8
#define CL_SCHAR_MAX        127
#define CL_SCHAR_MIN        (-127-1)
#define CL_CHAR_MAX         CL_SCHAR_MAX
#define CL_CHAR_MIN         CL_SCHAR_MIN
#define CL_UCHAR_MAX        255
#define CL_SHRT_MAX         32767
#define CL_SHRT_MIN         (-32767-1)
#define CL_USHRT_MAX        65535
#define CL_INT_MAX          2147483647
#define CL_INT_MIN          (-2147483647-1)
#define CL_UINT_MAX         0xffffffffU
#define CL_LONG_MAX         ((cl_long) 0x7FFFFFFFFFFFFFFFLL)
#define CL_LONG_MIN         ((cl_long) -0x7FFFFFFFFFFFFFFFLL - 1LL)
#define CL_ULONG_MAX        ((cl_ulong) 0xFFFFFFFFFFFFFFFFULL)

#define CL_FLT_DIG          6
#define CL_FLT_MANT_DIG     24
#define CL_FLT_MAX_10_EXP   +38
#define CL_FLT_MAX_EXP      +128
#define CL_FLT_MIN_10_EXP   -37
#define CL_FLT_MIN_EXP      -125
#define CL_FLT_RADIX        2
#if defined(_MSC_VER)
// MSVC doesn't understand hex floats
#define CL_FLT_MAX          3.402823466e+38F
#define CL_FLT_MIN          1.175494351e-38F
#define CL_FLT_EPSILON      1.192092896e-07F
#else
#define CL_FLT_MAX          0x1.fffffep127f
#define CL_FLT_MIN          0x1.0p-126f
#define CL_FLT_EPSILON      0x1.0p-23f
#endif

#define CL_DBL_DIG          15
#define CL_DBL_MANT_DIG     53
#define CL_DBL_MAX_10_EXP   +308
#define CL_DBL_MAX_EXP      +1024
#define CL_DBL_MIN_10_EXP   -307
#define CL_DBL_MIN_EXP      -1021
#define CL_DBL_RADIX        2
#if defined(_MSC_VER)
// MSVC doesn't understand hex floats
#define CL_DBL_MAX          1.7976931348623158e+308
#define CL_DBL_MIN          2.2250738585072014e-308
#define CL_DBL_EPSILON      2.2204460492503131e-016
#else
#define CL_DBL_MAX          0x1.fffffffffffffp1023
#define CL_DBL_MIN          0x1.0p-1022
#define CL_DBL_EPSILON      0x1.0p-52
#endif

#include <stddef.h>


//  CL.h contents
/******************************************************************************/

typedef struct _cl_platform_id *    cl_platform_id;
typedef struct _cl_device_id *      cl_device_id;
typedef struct _cl_context *        cl_context;
typedef struct _cl_command_queue *  cl_command_queue;
typedef struct _cl_mem *            cl_mem;
typedef struct _cl_program *        cl_program;
typedef struct _cl_kernel *         cl_kernel;
typedef struct _cl_event *          cl_event;
typedef struct _cl_sampler *        cl_sampler;

typedef cl_uint             cl_bool;                     /* WARNING!  Unlike cl_ types in cl_platform.h, cl_bool is not guaranteed to be the same size as the bool in kernels. */ 
typedef cl_ulong            cl_bitfield;
typedef cl_bitfield         cl_device_type;
typedef cl_uint             cl_platform_info;
typedef cl_uint             cl_device_info;
typedef cl_bitfield         cl_device_address_info;
typedef cl_bitfield         cl_device_fp_config;
typedef cl_uint             cl_device_mem_cache_type;
typedef cl_uint             cl_device_local_mem_type;
typedef cl_bitfield         cl_device_exec_capabilities;
typedef cl_bitfield         cl_command_queue_properties;

typedef intptr_t			cl_context_properties;
typedef cl_uint             cl_context_info;
typedef cl_uint             cl_command_queue_info;
typedef cl_uint             cl_channel_order;
typedef cl_uint             cl_channel_type;
typedef cl_bitfield         cl_mem_flags;
typedef cl_uint             cl_mem_object_type;
typedef cl_uint             cl_mem_info;
typedef cl_uint             cl_image_info;
typedef cl_uint             cl_addressing_mode;
typedef cl_uint             cl_filter_mode;
typedef cl_uint             cl_sampler_info;
typedef cl_bitfield         cl_map_flags;
typedef cl_uint             cl_program_info;
typedef cl_uint             cl_program_build_info;
typedef cl_int              cl_build_status;
typedef cl_uint             cl_kernel_info;
typedef cl_uint             cl_kernel_work_group_info;
typedef cl_uint             cl_event_info;
typedef cl_uint             cl_command_type;
typedef cl_uint             cl_profiling_info;

typedef struct _cl_image_format {
    cl_channel_order        image_channel_order;
    cl_channel_type         image_channel_data_type;
} cl_image_format;



/******************************************************************************/

// Error Codes
#define CL_SUCCESS                                  0
#define CL_DEVICE_NOT_FOUND                         -1
#define CL_DEVICE_NOT_AVAILABLE                     -2
#define CL_COMPILER_NOT_AVAILABLE                   -3
#define CL_MEM_OBJECT_ALLOCATION_FAILURE            -4
#define CL_OUT_OF_RESOURCES                         -5
#define CL_OUT_OF_HOST_MEMORY                       -6
#define CL_PROFILING_INFO_NOT_AVAILABLE             -7
#define CL_MEM_COPY_OVERLAP                         -8
#define CL_IMAGE_FORMAT_MISMATCH                    -9
#define CL_IMAGE_FORMAT_NOT_SUPPORTED               -10
#define CL_BUILD_PROGRAM_FAILURE                    -11
#define CL_MAP_FAILURE                              -12

#define CL_INVALID_VALUE                            -30
#define CL_INVALID_DEVICE_TYPE                      -31
#define CL_INVALID_PLATFORM                         -32
#define CL_INVALID_DEVICE                           -33
#define CL_INVALID_CONTEXT                          -34
#define CL_INVALID_QUEUE_PROPERTIES                 -35
#define CL_INVALID_COMMAND_QUEUE                    -36
#define CL_INVALID_HOST_PTR                         -37
#define CL_INVALID_MEM_OBJECT                       -38
#define CL_INVALID_IMAGE_FORMAT_DESCRIPTOR          -39
#define CL_INVALID_IMAGE_SIZE                       -40
#define CL_INVALID_SAMPLER                          -41
#define CL_INVALID_BINARY                           -42
#define CL_INVALID_BUILD_OPTIONS                    -43
#define CL_INVALID_PROGRAM                          -44
#define CL_INVALID_PROGRAM_EXECUTABLE               -45
#define CL_INVALID_KERNEL_NAME                      -46
#define CL_INVALID_KERNEL_DEFINITION                -47
#define CL_INVALID_KERNEL                           -48
#define CL_INVALID_ARG_INDEX                        -49
#define CL_INVALID_ARG_VALUE                        -50
#define CL_INVALID_ARG_SIZE                         -51
#define CL_INVALID_KERNEL_ARGS                      -52
#define CL_INVALID_WORK_DIMENSION                   -53
#define CL_INVALID_WORK_GROUP_SIZE                  -54
#define CL_INVALID_WORK_ITEM_SIZE                   -55
#define CL_INVALID_GLOBAL_OFFSET                    -56
#define CL_INVALID_EVENT_WAIT_LIST                  -57
#define CL_INVALID_EVENT                            -58
#define CL_INVALID_OPERATION                        -59
#define CL_INVALID_GL_OBJECT                        -60
#define CL_INVALID_BUFFER_SIZE                      -61
#define CL_INVALID_MIP_LEVEL                        -62
#define CL_INVALID_GLOBAL_WORK_SIZE                 -63

// OpenCL Version
#define CL_VERSION_1_0                              1

// cl_bool
#define CL_FALSE                                    0
#define CL_TRUE                                     1

// cl_platform_info
#define CL_PLATFORM_PROFILE                         0x0900
#define CL_PLATFORM_VERSION                         0x0901
#define CL_PLATFORM_NAME                            0x0902
#define CL_PLATFORM_VENDOR                          0x0903
#define CL_PLATFORM_EXTENSIONS                      0x0904

// cl_device_type - bitfield
#define CL_DEVICE_TYPE_DEFAULT                      (1 << 0)
#define CL_DEVICE_TYPE_CPU                          (1 << 1)
#define CL_DEVICE_TYPE_GPU                          (1 << 2)
#define CL_DEVICE_TYPE_ACCELERATOR                  (1 << 3)
#define CL_DEVICE_TYPE_ALL                          0xFFFFFFFF

// cl_device_info
#define CL_DEVICE_TYPE                              0x1000
#define CL_DEVICE_VENDOR_ID                         0x1001
#define CL_DEVICE_MAX_COMPUTE_UNITS                 0x1002
#define CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS          0x1003
#define CL_DEVICE_MAX_WORK_GROUP_SIZE               0x1004
#define CL_DEVICE_MAX_WORK_ITEM_SIZES               0x1005
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR       0x1006
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT      0x1007
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT        0x1008
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG       0x1009
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT      0x100A
#define CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE     0x100B
#define CL_DEVICE_MAX_CLOCK_FREQUENCY               0x100C
#define CL_DEVICE_ADDRESS_BITS                      0x100D
#define CL_DEVICE_MAX_READ_IMAGE_ARGS               0x100E
#define CL_DEVICE_MAX_WRITE_IMAGE_ARGS              0x100F
#define CL_DEVICE_MAX_MEM_ALLOC_SIZE                0x1010
#define CL_DEVICE_IMAGE2D_MAX_WIDTH                 0x1011
#define CL_DEVICE_IMAGE2D_MAX_HEIGHT                0x1012
#define CL_DEVICE_IMAGE3D_MAX_WIDTH                 0x1013
#define CL_DEVICE_IMAGE3D_MAX_HEIGHT                0x1014
#define CL_DEVICE_IMAGE3D_MAX_DEPTH                 0x1015
#define CL_DEVICE_IMAGE_SUPPORT                     0x1016
#define CL_DEVICE_MAX_PARAMETER_SIZE                0x1017
#define CL_DEVICE_MAX_SAMPLERS                      0x1018
#define CL_DEVICE_MEM_BASE_ADDR_ALIGN               0x1019
#define CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE          0x101A
#define CL_DEVICE_SINGLE_FP_CONFIG                  0x101B
#define CL_DEVICE_GLOBAL_MEM_CACHE_TYPE             0x101C
#define CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE         0x101D
#define CL_DEVICE_GLOBAL_MEM_CACHE_SIZE             0x101E
#define CL_DEVICE_GLOBAL_MEM_SIZE                   0x101F
#define CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE          0x1020
#define CL_DEVICE_MAX_CONSTANT_ARGS                 0x1021
#define CL_DEVICE_LOCAL_MEM_TYPE                    0x1022
#define CL_DEVICE_LOCAL_MEM_SIZE                    0x1023
#define CL_DEVICE_ERROR_CORRECTION_SUPPORT          0x1024
#define CL_DEVICE_PROFILING_TIMER_RESOLUTION        0x1025
#define CL_DEVICE_ENDIAN_LITTLE                     0x1026
#define CL_DEVICE_AVAILABLE                         0x1027
#define CL_DEVICE_COMPILER_AVAILABLE                0x1028
#define CL_DEVICE_EXECUTION_CAPABILITIES            0x1029
#define CL_DEVICE_QUEUE_PROPERTIES                  0x102A
#define CL_DEVICE_NAME                              0x102B
#define CL_DEVICE_VENDOR                            0x102C
#define CL_DRIVER_VERSION                           0x102D
#define CL_DEVICE_PROFILE                           0x102E
#define CL_DEVICE_VERSION                           0x102F
#define CL_DEVICE_EXTENSIONS                        0x1030
#define CL_DEVICE_PLATFORM                          0x1031

// cl_device_fp_config - bitfield
#define CL_FP_DENORM                                (1 << 0)
#define CL_FP_INF_NAN                               (1 << 1)
#define CL_FP_ROUND_TO_NEAREST                      (1 << 2)
#define CL_FP_ROUND_TO_ZERO                         (1 << 3)
#define CL_FP_ROUND_TO_INF                          (1 << 4)
#define CL_FP_FMA                                   (1 << 5)

// cl_device_mem_cache_type
#define CL_NONE                                     0x0
#define CL_READ_ONLY_CACHE                          0x1
#define CL_READ_WRITE_CACHE                         0x2

// cl_device_local_mem_type
#define CL_LOCAL                                    0x1
#define CL_GLOBAL                                   0x2

// cl_device_exec_capabilities - bitfield
#define CL_EXEC_KERNEL                              (1 << 0)
#define CL_EXEC_NATIVE_KERNEL                       (1 << 1)

// cl_command_queue_properties - bitfield
#define CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE      (1 << 0)
#define CL_QUEUE_PROFILING_ENABLE                   (1 << 1)

// cl_context_info
#define CL_CONTEXT_REFERENCE_COUNT                  0x1080
#define CL_CONTEXT_DEVICES                          0x1081
#define CL_CONTEXT_PROPERTIES                       0x1082

// cl_context_properties
#define CL_CONTEXT_PLATFORM                         0x1084

// cl_command_queue_info
#define CL_QUEUE_CONTEXT                            0x1090
#define CL_QUEUE_DEVICE                             0x1091
#define CL_QUEUE_REFERENCE_COUNT                    0x1092
#define CL_QUEUE_PROPERTIES                         0x1093

// cl_mem_flags - bitfield
#define CL_MEM_READ_WRITE                           (1 << 0)
#define CL_MEM_WRITE_ONLY                           (1 << 1)
#define CL_MEM_READ_ONLY                            (1 << 2)
#define CL_MEM_USE_HOST_PTR                         (1 << 3)
#define CL_MEM_ALLOC_HOST_PTR                       (1 << 4)
#define CL_MEM_COPY_HOST_PTR                        (1 << 5)

// cl_channel_order
#define CL_R                                        0x10B0
#define CL_A                                        0x10B1
#define CL_RG                                       0x10B2
#define CL_RA                                       0x10B3
#define CL_RGB                                      0x10B4
#define CL_RGBA                                     0x10B5
#define CL_BGRA                                     0x10B6
#define CL_ARGB                                     0x10B7
#define CL_INTENSITY                                0x10B8
#define CL_LUMINANCE                                0x10B9

// cl_channel_type
#define CL_SNORM_INT8                               0x10D0
#define CL_SNORM_INT16                              0x10D1
#define CL_UNORM_INT8                               0x10D2
#define CL_UNORM_INT16                              0x10D3
#define CL_UNORM_SHORT_565                          0x10D4
#define CL_UNORM_SHORT_555                          0x10D5
#define CL_UNORM_INT_101010                         0x10D6
#define CL_SIGNED_INT8                              0x10D7
#define CL_SIGNED_INT16                             0x10D8
#define CL_SIGNED_INT32                             0x10D9
#define CL_UNSIGNED_INT8                            0x10DA
#define CL_UNSIGNED_INT16                           0x10DB
#define CL_UNSIGNED_INT32                           0x10DC
#define CL_HALF_FLOAT                               0x10DD
#define CL_FLOAT                                    0x10DE

// cl_mem_object_type
#define CL_MEM_OBJECT_BUFFER                        0x10F0
#define CL_MEM_OBJECT_IMAGE2D                       0x10F1
#define CL_MEM_OBJECT_IMAGE3D                       0x10F2

// cl_mem_info
#define CL_MEM_TYPE                                 0x1100
#define CL_MEM_FLAGS                                0x1101
#define CL_MEM_SIZE                                 0x1102
#define CL_MEM_HOST_PTR                             0x1103
#define CL_MEM_MAP_COUNT                            0x1104
#define CL_MEM_REFERENCE_COUNT                      0x1105
#define CL_MEM_CONTEXT                              0x1106

// cl_image_info
#define CL_IMAGE_FORMAT                             0x1110
#define CL_IMAGE_ELEMENT_SIZE                       0x1111
#define CL_IMAGE_ROW_PITCH                          0x1112
#define CL_IMAGE_SLICE_PITCH                        0x1113
#define CL_IMAGE_WIDTH                              0x1114
#define CL_IMAGE_HEIGHT                             0x1115
#define CL_IMAGE_DEPTH                              0x1116

// cl_addressing_mode
#define CL_ADDRESS_NONE                             0x1130
#define CL_ADDRESS_CLAMP_TO_EDGE                    0x1131
#define CL_ADDRESS_CLAMP                            0x1132
#define CL_ADDRESS_REPEAT                           0x1133

// cl_filter_mode
#define CL_FILTER_NEAREST                           0x1140
#define CL_FILTER_LINEAR                            0x1141

// cl_sampler_info
#define CL_SAMPLER_REFERENCE_COUNT                  0x1150
#define CL_SAMPLER_CONTEXT                          0x1151
#define CL_SAMPLER_NORMALIZED_COORDS                0x1152
#define CL_SAMPLER_ADDRESSING_MODE                  0x1153
#define CL_SAMPLER_FILTER_MODE                      0x1154

// cl_map_flags - bitfield
#define CL_MAP_READ                                 (1 << 0)
#define CL_MAP_WRITE                                (1 << 1)

// cl_program_info
#define CL_PROGRAM_REFERENCE_COUNT                  0x1160
#define CL_PROGRAM_CONTEXT                          0x1161
#define CL_PROGRAM_NUM_DEVICES                      0x1162
#define CL_PROGRAM_DEVICES                          0x1163
#define CL_PROGRAM_SOURCE                           0x1164
#define CL_PROGRAM_BINARY_SIZES                     0x1165
#define CL_PROGRAM_BINARIES                         0x1166

// cl_program_build_info
#define CL_PROGRAM_BUILD_STATUS                     0x1181
#define CL_PROGRAM_BUILD_OPTIONS                    0x1182
#define CL_PROGRAM_BUILD_LOG                        0x1183

// cl_build_status
#define CL_BUILD_SUCCESS                            0
#define CL_BUILD_NONE                               -1
#define CL_BUILD_ERROR                              -2
#define CL_BUILD_IN_PROGRESS                        -3

// cl_kernel_info
#define CL_KERNEL_FUNCTION_NAME                     0x1190
#define CL_KERNEL_NUM_ARGS                          0x1191
#define CL_KERNEL_REFERENCE_COUNT                   0x1192
#define CL_KERNEL_CONTEXT                           0x1193
#define CL_KERNEL_PROGRAM                           0x1194

// cl_kernel_work_group_info
#define CL_KERNEL_WORK_GROUP_SIZE                   0x11B0
#define CL_KERNEL_COMPILE_WORK_GROUP_SIZE           0x11B1
#define CL_KERNEL_LOCAL_MEM_SIZE                    0x11B2

// cl_event_info
#define CL_EVENT_COMMAND_QUEUE                      0x11D0
#define CL_EVENT_COMMAND_TYPE                       0x11D1
#define CL_EVENT_REFERENCE_COUNT                    0x11D2
#define CL_EVENT_COMMAND_EXECUTION_STATUS           0x11D3

// cl_command_type
#define CL_COMMAND_NDRANGE_KERNEL                   0x11F0
#define CL_COMMAND_TASK                             0x11F1
#define CL_COMMAND_NATIVE_KERNEL                    0x11F2
#define CL_COMMAND_READ_BUFFER                      0x11F3
#define CL_COMMAND_WRITE_BUFFER                     0x11F4
#define CL_COMMAND_COPY_BUFFER                      0x11F5
#define CL_COMMAND_READ_IMAGE                       0x11F6
#define CL_COMMAND_WRITE_IMAGE                      0x11F7
#define CL_COMMAND_COPY_IMAGE                       0x11F8
#define CL_COMMAND_COPY_IMAGE_TO_BUFFER             0x11F9
#define CL_COMMAND_COPY_BUFFER_TO_IMAGE             0x11FA
#define CL_COMMAND_MAP_BUFFER                       0x11FB
#define CL_COMMAND_MAP_IMAGE                        0x11FC
#define CL_COMMAND_UNMAP_MEM_OBJECT                 0x11FD
#define CL_COMMAND_MARKER                           0x11FE
#define CL_COMMAND_ACQUIRE_GL_OBJECTS               0x11FF
#define CL_COMMAND_RELEASE_GL_OBJECTS               0x1200

// command execution status
#define CL_COMPLETE                                 0x0
#define CL_RUNNING                                  0x1
#define CL_SUBMITTED                                0x2
#define CL_QUEUED                                   0x3

// cl_profiling_info
#define CL_PROFILING_COMMAND_QUEUED                 0x1280
#define CL_PROFILING_COMMAND_SUBMIT                 0x1281
#define CL_PROFILING_COMMAND_START                  0x1282
#define CL_PROFILING_COMMAND_END                    0x1283

/********************************************************************************************************/

/********************************************************************************************************/

//  Function signature typedef's

// Platform API
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETPLATFORMIDS)(cl_uint          /* num_entries */,
                 cl_platform_id * /* platforms */,
                 cl_uint *        /* num_platforms */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL * 
PFNCLGETPLATFORMINFO)(cl_platform_id   /* platform */, 
                  cl_platform_info /* param_name */,
                  size_t           /* param_value_size */, 
                  void *           /* param_value */,
                  size_t *         /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Device APIs
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETDEVICEIDS)(cl_platform_id   /* platform */,
               cl_device_type   /* device_type */, 
               cl_uint          /* num_entries */, 
               cl_device_id *   /* devices */, 
               cl_uint *        /* num_devices */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETDEVICEINFO)(cl_device_id    /* device */,
                cl_device_info  /* param_name */, 
                size_t          /* param_value_size */, 
                void *          /* param_value */,
                size_t *        /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Context APIs  
typedef CL_API_ENTRY cl_context (CL_API_CALL *
PFNCLCREATECONTEXT)(const cl_context_properties * /* properties */,
                cl_uint                       /* num_devices */,
                const cl_device_id *          /* devices */,
                void (*pfn_notify)(const char *, const void *, size_t, void *) /* pfn_notify */,
                void *                        /* user_data */,
                cl_int *                      /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_context (CL_API_CALL *
PFNCLCREATECONTEXTFROMTYPE)(const cl_context_properties * /* properties */,
                        cl_device_type                /* device_type */,
                        void (*pfn_notify)(const char *, const void *, size_t, void *) /* pfn_notify */,
                        void *                        /* user_data */,
                        cl_int *                      /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINCONTEXT)(cl_context /* context */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASECONTEXT)(cl_context /* context */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETCONTEXTINFO)(cl_context         /* context */, 
                 cl_context_info    /* param_name */, 
                 size_t             /* param_value_size */, 
                 void *             /* param_value */, 
                 size_t *           /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Command Queue APIs
typedef CL_API_ENTRY cl_command_queue (CL_API_CALL *
PFNCLCREATECOMMANDQUEUE)(cl_context                     /* context */, 
                     cl_device_id                   /* device */, 
                     cl_command_queue_properties    /* properties */,
                     cl_int *                       /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINCOMMANDQUEUE)(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASECOMMANDQUEUE)(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETCOMMANDQUEUEINFO)(cl_command_queue      /* command_queue */,
                      cl_command_queue_info /* param_name */,
                      size_t                /* param_value_size */,
                      void *                /* param_value */,
                      size_t *              /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLSETCOMMANDQUEUEPROPERTY)(cl_command_queue              /* command_queue */,
                          cl_command_queue_properties   /* properties */, 
                          cl_bool                        /* enable */,
                          cl_command_queue_properties * /* old_properties */) CL_API_SUFFIX__VERSION_1_0;

// Memory Object APIs
typedef CL_API_ENTRY cl_mem (CL_API_CALL *
PFNCLCREATEBUFFER)(cl_context   /* context */,
               cl_mem_flags /* flags */,
               size_t       /* size */,
               void *       /* host_ptr */,
               cl_int *     /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *
PFNCLCREATEIMAGE2D)(cl_context              /* context */,
                cl_mem_flags            /* flags */,
                const cl_image_format * /* image_format */,
                size_t                  /* image_width */,
                size_t                  /* image_height */,
                size_t                  /* image_row_pitch */, 
                void *                  /* host_ptr */,
                cl_int *                /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *
PFNCLCREATEIMAGE3D)(cl_context              /* context */,
                cl_mem_flags            /* flags */,
                const cl_image_format * /* image_format */,
                size_t                  /* image_width */, 
                size_t                  /* image_height */,
                size_t                  /* image_depth */, 
                size_t                  /* image_row_pitch */, 
                size_t                  /* image_slice_pitch */, 
                void *                  /* host_ptr */,
                cl_int *                /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINMEMOBJECT)(cl_mem /* memobj */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASEMEMOBJECT)(cl_mem /* memobj */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETSUPPORTEDIMAGEFORMATS)(cl_context           /* context */,
                           cl_mem_flags         /* flags */,
                           cl_mem_object_type   /* image_type */,
                           cl_uint              /* num_entries */,
                           cl_image_format *    /* image_formats */,
                           cl_uint *            /* num_image_formats */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETMEMOBJECTINFO)(cl_mem           /* memobj */,
                   cl_mem_info      /* param_name */, 
                   size_t           /* param_value_size */,
                   void *           /* param_value */,
                   size_t *         /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETIMAGEINFO)(cl_mem           /* image */,
               cl_image_info    /* param_name */, 
               size_t           /* param_value_size */,
               void *           /* param_value */,
               size_t *         /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Sampler APIs
typedef CL_API_ENTRY cl_sampler (CL_API_CALL *
PFNCLCREATESAMPLER)(cl_context          /* context */,
                cl_bool             /* normalized_coords */, 
                cl_addressing_mode  /* addressing_mode */, 
                cl_filter_mode      /* filter_mode */,
                cl_int *            /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINSAMPLER)(cl_sampler /* sampler */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASESAMPLER)(cl_sampler /* sampler */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETSAMPLERINFO)(cl_sampler         /* sampler */,
                 cl_sampler_info    /* param_name */,
                 size_t             /* param_value_size */,
                 void *             /* param_value */,
                 size_t *           /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Program Object APIs
typedef CL_API_ENTRY cl_program (CL_API_CALL *
PFNCLCREATEPROGRAMWITHSOURCE)(cl_context        /* context */,
                          cl_uint           /* count */,
                          const char **     /* strings */,
                          const size_t *    /* lengths */,
                          cl_int *          /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_program (CL_API_CALL *
PFNCLCREATEPROGRAMWITHBINARY)(cl_context                     /* context */,
                          cl_uint                        /* num_devices */,
                          const cl_device_id *           /* device_list */,
                          const size_t *                 /* lengths */,
                          const unsigned char **         /* binaries */,
                          cl_int *                       /* binary_status */,
                          cl_int *                       /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINPROGRAM)(cl_program /* program */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASEPROGRAM)(cl_program /* program */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLBUILDPROGRAM)(cl_program           /* program */,
               cl_uint              /* num_devices */,
               const cl_device_id * /* device_list */,
               const char *         /* options */, 
               void (*pfn_notify)(cl_program /* program */, void * /* user_data */),
               void *               /* user_data */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLUNLOADCOMPILER)(void) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETPROGRAMINFO)(cl_program         /* program */,
                 cl_program_info    /* param_name */,
                 size_t             /* param_value_size */,
                 void *             /* param_value */,
                 size_t *           /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETPROGRAMBUILDINFO)(cl_program            /* program */,
                      cl_device_id          /* device */,
                      cl_program_build_info /* param_name */,
                      size_t                /* param_value_size */,
                      void *                /* param_value */,
                      size_t *              /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Kernel Object APIs
typedef CL_API_ENTRY cl_kernel (CL_API_CALL *
PFNCLCREATEKERNEL)(cl_program      /* program */,
               const char *    /* kernel_name */,
               cl_int *        /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLCREATEKERNELSINPROGRAM)(cl_program     /* program */,
                         cl_uint        /* num_kernels */,
                         cl_kernel *    /* kernels */,
                         cl_uint *      /* num_kernels_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINKERNEL)(cl_kernel    /* kernel */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASEKERNEL)(cl_kernel   /* kernel */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLSETKERNELARG)(cl_kernel    /* kernel */,
               cl_uint      /* arg_index */,
               size_t       /* arg_size */,
               const void * /* arg_value */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETKERNELINFO)(cl_kernel       /* kernel */,
                cl_kernel_info  /* param_name */,
                size_t          /* param_value_size */,
                void *          /* param_value */,
                size_t *        /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETKERNELWORKGROUPINFO)(cl_kernel                  /* kernel */,
                         cl_device_id               /* device */,
                         cl_kernel_work_group_info  /* param_name */,
                         size_t                     /* param_value_size */,
                         void *                     /* param_value */,
                         size_t *                   /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Event Object APIs
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLWAITFOREVENTS)(cl_uint             /* num_events */,
                const cl_event *    /* event_list */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETEVENTINFO)(cl_event         /* event */,
               cl_event_info    /* param_name */,
               size_t           /* param_value_size */,
               void *           /* param_value */,
               size_t *         /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRETAINEVENT)(cl_event /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLRELEASEEVENT)(cl_event /* event */) CL_API_SUFFIX__VERSION_1_0;

// Profiling APIs
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLGETEVENTPROFILINGINFO)(cl_event            /* event */,
                        cl_profiling_info   /* param_name */,
                        size_t              /* param_value_size */,
                        void *              /* param_value */,
                        size_t *            /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0;

// Flush and Finish APIs
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLFLUSH)(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLFINISH)(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

// Enqueued Commands APIs
typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEREADBUFFER)(cl_command_queue    /* command_queue */,
                    cl_mem              /* buffer */,
                    cl_bool             /* blocking_read */,
                    size_t              /* offset */,
                    size_t              /* cb */, 
                    void *              /* ptr */,
                    cl_uint             /* num_events_in_wait_list */,
                    const cl_event *    /* event_wait_list */,
                    cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEWRITEBUFFER)(cl_command_queue   /* command_queue */, 
                     cl_mem             /* buffer */, 
                     cl_bool            /* blocking_write */, 
                     size_t             /* offset */, 
                     size_t             /* cb */, 
                     const void *       /* ptr */, 
                     cl_uint            /* num_events_in_wait_list */, 
                     const cl_event *   /* event_wait_list */, 
                     cl_event *         /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUECOPYBUFFER)(cl_command_queue    /* command_queue */, 
                    cl_mem              /* src_buffer */,
                    cl_mem              /* dst_buffer */, 
                    size_t              /* src_offset */,
                    size_t              /* dst_offset */,
                    size_t              /* cb */, 
                    cl_uint             /* num_events_in_wait_list */,
                    const cl_event *    /* event_wait_list */,
                    cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEREADIMAGE)(cl_command_queue     /* command_queue */,
                   cl_mem               /* image */,
                   cl_bool              /* blocking_read */, 
                   const size_t *       /* origin[3] */,
                   const size_t *       /* region[3] */,
                   size_t               /* row_pitch */,
                   size_t               /* slice_pitch */, 
                   void *               /* ptr */,
                   cl_uint              /* num_events_in_wait_list */,
                   const cl_event *     /* event_wait_list */,
                   cl_event *           /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEWRITEIMAGE)(cl_command_queue    /* command_queue */,
                    cl_mem              /* image */,
                    cl_bool             /* blocking_write */, 
                    const size_t *      /* origin[3] */,
                    const size_t *      /* region[3] */,
                    size_t              /* input_row_pitch */,
                    size_t              /* input_slice_pitch */, 
                    const void *        /* ptr */,
                    cl_uint             /* num_events_in_wait_list */,
                    const cl_event *    /* event_wait_list */,
                    cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUECOPYIMAGE)(cl_command_queue     /* command_queue */,
                   cl_mem               /* src_image */,
                   cl_mem               /* dst_image */, 
                   const size_t *       /* src_origin[3] */,
                   const size_t *       /* dst_origin[3] */,
                   const size_t *       /* region[3] */, 
                   cl_uint              /* num_events_in_wait_list */,
                   const cl_event *     /* event_wait_list */,
                   cl_event *           /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUECOPYIMAGETOBUFFER)(cl_command_queue /* command_queue */,
                           cl_mem           /* src_image */,
                           cl_mem           /* dst_buffer */, 
                           const size_t *   /* src_origin[3] */,
                           const size_t *   /* region[3] */, 
                           size_t           /* dst_offset */,
                           cl_uint          /* num_events_in_wait_list */,
                           const cl_event * /* event_wait_list */,
                           cl_event *       /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUECOPYBUFFERTOIMAGE)(cl_command_queue /* command_queue */,
                           cl_mem           /* src_buffer */,
                           cl_mem           /* dst_image */, 
                           size_t           /* src_offset */,
                           const size_t *   /* dst_origin[3] */,
                           const size_t *   /* region[3] */, 
                           cl_uint          /* num_events_in_wait_list */,
                           const cl_event * /* event_wait_list */,
                           cl_event *       /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY void * (CL_API_CALL *
PFNCLENQUEUEMAPBUFFER)(cl_command_queue /* command_queue */,
                   cl_mem           /* buffer */,
                   cl_bool          /* blocking_map */, 
                   cl_map_flags     /* map_flags */,
                   size_t           /* offset */,
                   size_t           /* cb */,
                   cl_uint          /* num_events_in_wait_list */,
                   const cl_event * /* event_wait_list */,
                   cl_event *       /* event */,
                   cl_int *         /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY void * (CL_API_CALL *
PFNCLENQUEUEMAPIMAGE)(cl_command_queue  /* command_queue */,
                  cl_mem            /* image */, 
                  cl_bool           /* blocking_map */, 
                  cl_map_flags      /* map_flags */, 
                  const size_t *    /* origin[3] */,
                  const size_t *    /* region[3] */,
                  size_t *          /* image_row_pitch */,
                  size_t *          /* image_slice_pitch */,
                  cl_uint           /* num_events_in_wait_list */,
                  const cl_event *  /* event_wait_list */,
                  cl_event *        /* event */,
                  cl_int *          /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEUNMAPMEMOBJECT)(cl_command_queue /* command_queue */,
                        cl_mem           /* memobj */,
                        void *           /* mapped_ptr */,
                        cl_uint          /* num_events_in_wait_list */,
                        const cl_event *  /* event_wait_list */,
                        cl_event *        /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUENDRANGEKERNEL)(cl_command_queue /* command_queue */,
                       cl_kernel        /* kernel */,
                       cl_uint          /* work_dim */,
                       const size_t *   /* global_work_offset */,
                       const size_t *   /* global_work_size */,
                       const size_t *   /* local_work_size */,
                       cl_uint          /* num_events_in_wait_list */,
                       const cl_event * /* event_wait_list */,
                       cl_event *       /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUETASK)(cl_command_queue  /* command_queue */,
              cl_kernel         /* kernel */,
              cl_uint           /* num_events_in_wait_list */,
              const cl_event *  /* event_wait_list */,
              cl_event *        /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUENATIVEKERNEL)(cl_command_queue  /* command_queue */,
                      void (*user_func)(void *), 
                      void *            /* args */,
                      size_t            /* cb_args */, 
                      cl_uint           /* num_mem_objects */,
                      const cl_mem *    /* mem_list */,
                      const void **     /* args_mem_loc */,
                      cl_uint           /* num_events_in_wait_list */,
                      const cl_event *  /* event_wait_list */,
                      cl_event *        /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEMARKER)(cl_command_queue    /* command_queue */,
                cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEWAITFOREVENTS)(cl_command_queue /* command_queue */,
                       cl_uint          /* num_events */,
                       const cl_event * /* event_list */) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *
PFNCLENQUEUEBARRIER)(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

// Extension function access
//
// Returns the extension function address for the given function name,
// or NULL if a valid function can not be found.  The client must
// check to make sure the address is not NULL, before using or 
// calling the returned function address.
//
typedef CL_API_ENTRY void * (CL_API_CALL * PFNCLGETEXTENSIONFUNCTIONADDRESS)(const char * /* func_name */) CL_API_SUFFIX__VERSION_1_0;


#define CLEW_STATIC

#ifdef CLEW_STATIC
#  define CLEWAPI extern
#else
#  ifdef CLEW_BUILD
#    define CLEWAPI extern __declspec(dllexport)
#  else
#    define CLEWAPI extern __declspec(dllimport)
#  endif
#endif

#if defined(_WIN32)
#define CLEW_FUN_EXPORT extern
#else
#define CLEW_FUN_EXPORT CLEWAPI
#endif

#define CLEW_GET_FUN(x) x


//  Variables holding function entry points
CLEW_FUN_EXPORT     PFNCLGETPLATFORMIDS                 __oclGetPlatformIDs                ;
CLEW_FUN_EXPORT     PFNCLGETPLATFORMINFO                __oclGetPlatformInfo               ;
CLEW_FUN_EXPORT     PFNCLGETDEVICEIDS                   __oclGetDeviceIDs                  ;
CLEW_FUN_EXPORT     PFNCLGETDEVICEINFO                  __oclGetDeviceInfo                 ;
CLEW_FUN_EXPORT     PFNCLCREATECONTEXT                  __oclCreateContext                 ;
CLEW_FUN_EXPORT     PFNCLCREATECONTEXTFROMTYPE          __oclCreateContextFromType         ;
CLEW_FUN_EXPORT     PFNCLRETAINCONTEXT                  __oclRetainContext                 ;
CLEW_FUN_EXPORT     PFNCLRELEASECONTEXT                 __oclReleaseContext                ;
CLEW_FUN_EXPORT     PFNCLGETCONTEXTINFO                 __oclGetContextInfo                ;
CLEW_FUN_EXPORT     PFNCLCREATECOMMANDQUEUE             __oclCreateCommandQueue            ;
CLEW_FUN_EXPORT     PFNCLRETAINCOMMANDQUEUE             __oclRetainCommandQueue            ;
CLEW_FUN_EXPORT     PFNCLRELEASECOMMANDQUEUE            __oclReleaseCommandQueue           ;
CLEW_FUN_EXPORT     PFNCLGETCOMMANDQUEUEINFO            __oclGetCommandQueueInfo           ;
CLEW_FUN_EXPORT     PFNCLSETCOMMANDQUEUEPROPERTY        __oclSetCommandQueueProperty       ;
CLEW_FUN_EXPORT     PFNCLCREATEBUFFER                   __oclCreateBuffer                  ;
CLEW_FUN_EXPORT     PFNCLCREATEIMAGE2D                  __oclCreateImage2D                 ;
CLEW_FUN_EXPORT     PFNCLCREATEIMAGE3D                  __oclCreateImage3D                 ;
CLEW_FUN_EXPORT     PFNCLRETAINMEMOBJECT                __oclRetainMemObject               ;
CLEW_FUN_EXPORT     PFNCLRELEASEMEMOBJECT               __oclReleaseMemObject              ;
CLEW_FUN_EXPORT     PFNCLGETSUPPORTEDIMAGEFORMATS       __oclGetSupportedImageFormats      ;
CLEW_FUN_EXPORT     PFNCLGETMEMOBJECTINFO               __oclGetMemObjectInfo              ;
CLEW_FUN_EXPORT     PFNCLGETIMAGEINFO                   __oclGetImageInfo                  ;
CLEW_FUN_EXPORT     PFNCLCREATESAMPLER                  __oclCreateSampler                 ;
CLEW_FUN_EXPORT     PFNCLRETAINSAMPLER                  __oclRetainSampler                 ;
CLEW_FUN_EXPORT     PFNCLRELEASESAMPLER                 __oclReleaseSampler                ;
CLEW_FUN_EXPORT     PFNCLGETSAMPLERINFO                 __oclGetSamplerInfo                ;
CLEW_FUN_EXPORT     PFNCLCREATEPROGRAMWITHSOURCE        __oclCreateProgramWithSource       ;
CLEW_FUN_EXPORT     PFNCLCREATEPROGRAMWITHBINARY        __oclCreateProgramWithBinary       ;
CLEW_FUN_EXPORT     PFNCLRETAINPROGRAM                  __oclRetainProgram                 ;
CLEW_FUN_EXPORT     PFNCLRELEASEPROGRAM                 __oclReleaseProgram                ;
CLEW_FUN_EXPORT     PFNCLBUILDPROGRAM                   __oclBuildProgram                  ;
CLEW_FUN_EXPORT     PFNCLUNLOADCOMPILER                 __oclUnloadCompiler                ;
CLEW_FUN_EXPORT     PFNCLGETPROGRAMINFO                 __oclGetProgramInfo                ;
CLEW_FUN_EXPORT     PFNCLGETPROGRAMBUILDINFO            __oclGetProgramBuildInfo           ;
CLEW_FUN_EXPORT     PFNCLCREATEKERNEL                   __oclCreateKernel                  ;
CLEW_FUN_EXPORT     PFNCLCREATEKERNELSINPROGRAM         __oclCreateKernelsInProgram        ;
CLEW_FUN_EXPORT     PFNCLRETAINKERNEL                   __oclRetainKernel                  ;
CLEW_FUN_EXPORT     PFNCLRELEASEKERNEL                  __oclReleaseKernel                 ;
CLEW_FUN_EXPORT     PFNCLSETKERNELARG                   __oclSetKernelArg                  ;
CLEW_FUN_EXPORT     PFNCLGETKERNELINFO                  __oclGetKernelInfo                 ;
CLEW_FUN_EXPORT     PFNCLGETKERNELWORKGROUPINFO         __oclGetKernelWorkGroupInfo        ;
CLEW_FUN_EXPORT     PFNCLWAITFOREVENTS                  __oclWaitForEvents                 ;
CLEW_FUN_EXPORT     PFNCLGETEVENTINFO                   __oclGetEventInfo                  ;
CLEW_FUN_EXPORT     PFNCLRETAINEVENT                    __oclRetainEvent                   ;
CLEW_FUN_EXPORT     PFNCLRELEASEEVENT                   __oclReleaseEvent                  ;
CLEW_FUN_EXPORT     PFNCLGETEVENTPROFILINGINFO          __oclGetEventProfilingInfo         ;
CLEW_FUN_EXPORT     PFNCLFLUSH                          __oclFlush                         ;
CLEW_FUN_EXPORT     PFNCLFINISH                         __oclFinish                        ;
CLEW_FUN_EXPORT     PFNCLENQUEUEREADBUFFER              __oclEnqueueReadBuffer             ;
CLEW_FUN_EXPORT     PFNCLENQUEUEWRITEBUFFER             __oclEnqueueWriteBuffer            ;
CLEW_FUN_EXPORT     PFNCLENQUEUECOPYBUFFER              __oclEnqueueCopyBuffer             ;
CLEW_FUN_EXPORT     PFNCLENQUEUEREADIMAGE               __oclEnqueueReadImage              ;
CLEW_FUN_EXPORT     PFNCLENQUEUEWRITEIMAGE              __oclEnqueueWriteImage             ;
CLEW_FUN_EXPORT     PFNCLENQUEUECOPYIMAGE               __oclEnqueueCopyImage              ;
CLEW_FUN_EXPORT     PFNCLENQUEUECOPYIMAGETOBUFFER       __oclEnqueueCopyImageToBuffer      ;
CLEW_FUN_EXPORT     PFNCLENQUEUECOPYBUFFERTOIMAGE       __oclEnqueueCopyBufferToImage      ;
CLEW_FUN_EXPORT     PFNCLENQUEUEMAPBUFFER               __oclEnqueueMapBuffer              ;
CLEW_FUN_EXPORT     PFNCLENQUEUEMAPIMAGE                __oclEnqueueMapImage               ;
CLEW_FUN_EXPORT     PFNCLENQUEUEUNMAPMEMOBJECT          __oclEnqueueUnmapMemObject         ;
CLEW_FUN_EXPORT     PFNCLENQUEUENDRANGEKERNEL           __oclEnqueueNDRangeKernel          ;
CLEW_FUN_EXPORT     PFNCLENQUEUETASK                    __oclEnqueueTask                   ;
CLEW_FUN_EXPORT     PFNCLENQUEUENATIVEKERNEL            __oclEnqueueNativeKernel           ;
CLEW_FUN_EXPORT     PFNCLENQUEUEMARKER                  __oclEnqueueMarker                 ;
CLEW_FUN_EXPORT     PFNCLENQUEUEWAITFOREVENTS           __oclEnqueueWaitForEvents          ;
CLEW_FUN_EXPORT     PFNCLENQUEUEBARRIER                 __oclEnqueueBarrier                ;
CLEW_FUN_EXPORT     PFNCLGETEXTENSIONFUNCTIONADDRESS    __oclGetExtensionFunctionAddress   ;


#define	clGetPlatformIDs                CLEW_GET_FUN(__oclGetPlatformIDs                )
#define	clGetPlatformInfo               CLEW_GET_FUN(__oclGetPlatformInfo               )
#define	clGetDeviceIDs                  CLEW_GET_FUN(__oclGetDeviceIDs                  )
#define	clGetDeviceInfo                 CLEW_GET_FUN(__oclGetDeviceInfo                 )
#define	clCreateContext                 CLEW_GET_FUN(__oclCreateContext                 )
#define	clCreateContextFromType         CLEW_GET_FUN(__oclCreateContextFromType         )
#define	clRetainContext                 CLEW_GET_FUN(__oclRetainContext                 )
#define	clReleaseContext                CLEW_GET_FUN(__oclReleaseContext                )
#define	clGetContextInfo                CLEW_GET_FUN(__oclGetContextInfo                )
#define	clCreateCommandQueue            CLEW_GET_FUN(__oclCreateCommandQueue            )
#define	clRetainCommandQueue            CLEW_GET_FUN(__oclRetainCommandQueue            )
#define	clReleaseCommandQueue           CLEW_GET_FUN(__oclReleaseCommandQueue           )
#define	clGetCommandQueueInfo           CLEW_GET_FUN(__oclGetCommandQueueInfo           )
#define	clSetCommandQueueProperty       CLEW_GET_FUN(__oclSetCommandQueueProperty       )
#define	clCreateBuffer                  CLEW_GET_FUN(__oclCreateBuffer                  )
#define	clCreateImage2D                 CLEW_GET_FUN(__oclCreateImage2D                 )
#define	clCreateImage3D                 CLEW_GET_FUN(__oclCreateImage3D                 )
#define	clRetainMemObject               CLEW_GET_FUN(__oclRetainMemObject               )
#define	clReleaseMemObject              CLEW_GET_FUN(__oclReleaseMemObject              )
#define	clGetSupportedImageFormats      CLEW_GET_FUN(__oclGetSupportedImageFormats      )
#define	clGetMemObjectInfo              CLEW_GET_FUN(__oclGetMemObjectInfo              )
#define	clGetImageInfo                  CLEW_GET_FUN(__oclGetImageInfo                  )
#define	clCreateSampler                 CLEW_GET_FUN(__oclCreateSampler                 )
#define	clRetainSampler                 CLEW_GET_FUN(__oclRetainSampler                 )
#define	clReleaseSampler                CLEW_GET_FUN(__oclReleaseSampler                )
#define	clGetSamplerInfo                CLEW_GET_FUN(__oclGetSamplerInfo                )
#define	clCreateProgramWithSource       CLEW_GET_FUN(__oclCreateProgramWithSource       )
#define	clCreateProgramWithBinary       CLEW_GET_FUN(__oclCreateProgramWithBinary       )
#define	clRetainProgram                 CLEW_GET_FUN(__oclRetainProgram                 )
#define	clReleaseProgram                CLEW_GET_FUN(__oclReleaseProgram                )
#define	clBuildProgram                  CLEW_GET_FUN(__oclBuildProgram                  )
#define	clUnloadCompiler                CLEW_GET_FUN(__oclUnloadCompiler                )
#define	clGetProgramInfo                CLEW_GET_FUN(__oclGetProgramInfo                )
#define	clGetProgramBuildInfo           CLEW_GET_FUN(__oclGetProgramBuildInfo           )
#define	clCreateKernel                  CLEW_GET_FUN(__oclCreateKernel                  )
#define	clCreateKernelsInProgram        CLEW_GET_FUN(__oclCreateKernelsInProgram        )
#define	clRetainKernel                  CLEW_GET_FUN(__oclRetainKernel                  )
#define	clReleaseKernel                 CLEW_GET_FUN(__oclReleaseKernel                 )
#define	clSetKernelArg                  CLEW_GET_FUN(__oclSetKernelArg                  )
#define	clGetKernelInfo                 CLEW_GET_FUN(__oclGetKernelInfo                 )
#define	clGetKernelWorkGroupInfo        CLEW_GET_FUN(__oclGetKernelWorkGroupInfo        )
#define	clWaitForEvents                 CLEW_GET_FUN(__oclWaitForEvents                 )
#define	clGetEventInfo                  CLEW_GET_FUN(__oclGetEventInfo                  )
#define	clRetainEvent                   CLEW_GET_FUN(__oclRetainEvent                   )
#define	clReleaseEvent                  CLEW_GET_FUN(__oclReleaseEvent                  )
#define	clGetEventProfilingInfo         CLEW_GET_FUN(__oclGetEventProfilingInfo         )
#define	clFlush                         CLEW_GET_FUN(__oclFlush                         )
#define	clFinish                        CLEW_GET_FUN(__oclFinish                        )
#define	clEnqueueReadBuffer             CLEW_GET_FUN(__oclEnqueueReadBuffer             )
#define	clEnqueueWriteBuffer            CLEW_GET_FUN(__oclEnqueueWriteBuffer            )
#define	clEnqueueCopyBuffer             CLEW_GET_FUN(__oclEnqueueCopyBuffer             )
#define	clEnqueueReadImage              CLEW_GET_FUN(__oclEnqueueReadImage              )
#define	clEnqueueWriteImage             CLEW_GET_FUN(__oclEnqueueWriteImage             )
#define	clEnqueueCopyImage              CLEW_GET_FUN(__oclEnqueueCopyImage              )
#define	clEnqueueCopyImageToBuffer      CLEW_GET_FUN(__oclEnqueueCopyImageToBuffer      )
#define	clEnqueueCopyBufferToImage      CLEW_GET_FUN(__oclEnqueueCopyBufferToImage      )
#define	clEnqueueMapBuffer              CLEW_GET_FUN(__oclEnqueueMapBuffer              )
#define	clEnqueueMapImage               CLEW_GET_FUN(__oclEnqueueMapImage               )
#define	clEnqueueUnmapMemObject         CLEW_GET_FUN(__oclEnqueueUnmapMemObject         )
#define	clEnqueueNDRangeKernel          CLEW_GET_FUN(__oclEnqueueNDRangeKernel          )
#define	clEnqueueTask                   CLEW_GET_FUN(__oclEnqueueTask                   )
#define	clEnqueueNativeKernel           CLEW_GET_FUN(__oclEnqueueNativeKernel           )
#define	clEnqueueMarker                 CLEW_GET_FUN(__oclEnqueueMarker                 )
#define	clEnqueueWaitForEvents          CLEW_GET_FUN(__oclEnqueueWaitForEvents          )
#define	clEnqueueBarrier                CLEW_GET_FUN(__oclEnqueueBarrier                )
#define	clGetExtensionFunctionAddress   CLEW_GET_FUN(__oclGetExtensionFunctionAddress   )

#endif  //  CLCC_GENERATE_DOCUMENTATION

#define CLEW_SUCCESS                0       //!<    Success error code
#define CLEW_ERROR_OPEN_FAILED      -1      //!<    Error code for failing to open the dynamic library
#define CLEW_ERROR_ATEXIT_FAILED    -2      //!<    Error code for failing to queue the closing of the dynamic library to atexit()

//! \brief Load OpenCL dynamic library and set function entry points
int         clewInit        (const char*);
//! \brief Convert an OpenCL error code to its string equivalent
const char* clewErrorString (cl_int error);

#ifdef __cplusplus
}
#endif

#endif  //  CLCC_CLEW_HPP_INCLUDED
