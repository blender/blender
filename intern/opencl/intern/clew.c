//////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2009 Organic Vectory B.V.
//  Written by George van Venrooij
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file license.txt)
//////////////////////////////////////////////////////////////////////////

#include "clew.h"

//! \file clew.c
//! \brief OpenCL run-time loader source

#ifndef CLCC_GENERATE_DOCUMENTATION
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <windows.h>

    typedef HMODULE             CLCC_DYNLIB_HANDLE;

    #define CLCC_DYNLIB_OPEN    LoadLibrary
    #define CLCC_DYNLIB_CLOSE   FreeLibrary
    #define CLCC_DYNLIB_IMPORT  GetProcAddress
#else
    #include <dlfcn.h>
    
    typedef void*                   CLCC_DYNLIB_HANDLE;

    #define CLCC_DYNLIB_OPEN(path)  dlopen(path, RTLD_NOW | RTLD_GLOBAL)
    #define CLCC_DYNLIB_CLOSE       dlclose
    #define CLCC_DYNLIB_IMPORT      dlsym
#endif
#else
    //typedef implementation_defined  CLCC_DYNLIB_HANDLE;
    //#define CLCC_DYNLIB_OPEN(path)  implementation_defined
    //#define CLCC_DYNLIB_CLOSE       implementation_defined
    //#define CLCC_DYNLIB_IMPORT      implementation_defined
#endif

#include <stdlib.h>

//! \brief module handle
static CLCC_DYNLIB_HANDLE module = NULL;

//  Variables holding function entry points
#ifndef CLCC_GENERATE_DOCUMENTATION
PFNCLGETPLATFORMIDS                 __oclGetPlatformIDs                = NULL;
PFNCLGETPLATFORMINFO                __oclGetPlatformInfo               = NULL;
PFNCLGETDEVICEIDS                   __oclGetDeviceIDs                  = NULL;
PFNCLGETDEVICEINFO                  __oclGetDeviceInfo                 = NULL;
PFNCLCREATECONTEXT                  __oclCreateContext                 = NULL;
PFNCLCREATECONTEXTFROMTYPE          __oclCreateContextFromType         = NULL;
PFNCLRETAINCONTEXT                  __oclRetainContext                 = NULL;
PFNCLRELEASECONTEXT                 __oclReleaseContext                = NULL;
PFNCLGETCONTEXTINFO                 __oclGetContextInfo                = NULL;
PFNCLCREATECOMMANDQUEUE             __oclCreateCommandQueue            = NULL;
PFNCLRETAINCOMMANDQUEUE             __oclRetainCommandQueue            = NULL;
PFNCLRELEASECOMMANDQUEUE            __oclReleaseCommandQueue           = NULL;
PFNCLGETCOMMANDQUEUEINFO            __oclGetCommandQueueInfo           = NULL;
PFNCLSETCOMMANDQUEUEPROPERTY        __oclSetCommandQueueProperty       = NULL;
PFNCLCREATEBUFFER                   __oclCreateBuffer                  = NULL;
PFNCLCREATEIMAGE2D                  __oclCreateImage2D                 = NULL;
PFNCLCREATEIMAGE3D                  __oclCreateImage3D                 = NULL;
PFNCLRETAINMEMOBJECT                __oclRetainMemObject               = NULL;
PFNCLRELEASEMEMOBJECT               __oclReleaseMemObject              = NULL;
PFNCLGETSUPPORTEDIMAGEFORMATS       __oclGetSupportedImageFormats      = NULL;
PFNCLGETMEMOBJECTINFO               __oclGetMemObjectInfo              = NULL;
PFNCLGETIMAGEINFO                   __oclGetImageInfo                  = NULL;
PFNCLCREATESAMPLER                  __oclCreateSampler                 = NULL;
PFNCLRETAINSAMPLER                  __oclRetainSampler                 = NULL;
PFNCLRELEASESAMPLER                 __oclReleaseSampler                = NULL;
PFNCLGETSAMPLERINFO                 __oclGetSamplerInfo                = NULL;
PFNCLCREATEPROGRAMWITHSOURCE        __oclCreateProgramWithSource       = NULL;
PFNCLCREATEPROGRAMWITHBINARY        __oclCreateProgramWithBinary       = NULL;
PFNCLRETAINPROGRAM                  __oclRetainProgram                 = NULL;
PFNCLRELEASEPROGRAM                 __oclReleaseProgram                = NULL;
PFNCLBUILDPROGRAM                   __oclBuildProgram                  = NULL;
PFNCLUNLOADCOMPILER                 __oclUnloadCompiler                = NULL;
PFNCLGETPROGRAMINFO                 __oclGetProgramInfo                = NULL;
PFNCLGETPROGRAMBUILDINFO            __oclGetProgramBuildInfo           = NULL;
PFNCLCREATEKERNEL                   __oclCreateKernel                  = NULL;
PFNCLCREATEKERNELSINPROGRAM         __oclCreateKernelsInProgram        = NULL;
PFNCLRETAINKERNEL                   __oclRetainKernel                  = NULL;
PFNCLRELEASEKERNEL                  __oclReleaseKernel                 = NULL;
PFNCLSETKERNELARG                   __oclSetKernelArg                  = NULL;
PFNCLGETKERNELINFO                  __oclGetKernelInfo                 = NULL;
PFNCLGETKERNELWORKGROUPINFO         __oclGetKernelWorkGroupInfo        = NULL;
PFNCLWAITFOREVENTS                  __oclWaitForEvents                 = NULL;
PFNCLGETEVENTINFO                   __oclGetEventInfo                  = NULL;
PFNCLRETAINEVENT                    __oclRetainEvent                   = NULL;
PFNCLRELEASEEVENT                   __oclReleaseEvent                  = NULL;
PFNCLGETEVENTPROFILINGINFO          __oclGetEventProfilingInfo         = NULL;
PFNCLFLUSH                          __oclFlush                         = NULL;
PFNCLFINISH                         __oclFinish                        = NULL;
PFNCLENQUEUEREADBUFFER              __oclEnqueueReadBuffer             = NULL;
PFNCLENQUEUEWRITEBUFFER             __oclEnqueueWriteBuffer            = NULL;
PFNCLENQUEUECOPYBUFFER              __oclEnqueueCopyBuffer             = NULL;
PFNCLENQUEUEREADIMAGE               __oclEnqueueReadImage              = NULL;
PFNCLENQUEUEWRITEIMAGE              __oclEnqueueWriteImage             = NULL;
PFNCLENQUEUECOPYIMAGE               __oclEnqueueCopyImage              = NULL;
PFNCLENQUEUECOPYIMAGETOBUFFER       __oclEnqueueCopyImageToBuffer      = NULL;
PFNCLENQUEUECOPYBUFFERTOIMAGE       __oclEnqueueCopyBufferToImage      = NULL;
PFNCLENQUEUEMAPBUFFER               __oclEnqueueMapBuffer              = NULL;
PFNCLENQUEUEMAPIMAGE                __oclEnqueueMapImage               = NULL;
PFNCLENQUEUEUNMAPMEMOBJECT          __oclEnqueueUnmapMemObject         = NULL;
PFNCLENQUEUENDRANGEKERNEL           __oclEnqueueNDRangeKernel          = NULL;
PFNCLENQUEUETASK                    __oclEnqueueTask                   = NULL;
PFNCLENQUEUENATIVEKERNEL            __oclEnqueueNativeKernel           = NULL;
PFNCLENQUEUEMARKER                  __oclEnqueueMarker                 = NULL;
PFNCLENQUEUEWAITFOREVENTS           __oclEnqueueWaitForEvents          = NULL;
PFNCLENQUEUEBARRIER                 __oclEnqueueBarrier                = NULL;
PFNCLGETEXTENSIONFUNCTIONADDRESS    __oclGetExtensionFunctionAddress   = NULL;
#endif  //  CLCC_GENERATE_DOCUMENTATION


//! \brief Unloads OpenCL dynamic library, should not be called directly
static void clewExit(void)
{
    if (module != NULL)
    {
        //  Ignore errors
        CLCC_DYNLIB_CLOSE(module);
        module = NULL;
    }
}

//! \param path path to dynamic library to load
//! \return CLEW_ERROR_OPEN_FAILED if the library could not be opened
//! CLEW_ERROR_ATEXIT_FAILED if atexit(clewExit) failed
//! CLEW_SUCCESS when the library was succesfully loaded
int clewInit(const char* path)
{
    int error = 0;

    //  Check if already initialized
    if (module != NULL)
    {
        return CLEW_SUCCESS;
    }

    //  Load library
    module = CLCC_DYNLIB_OPEN(path);

    //  Check for errors
    if (module == NULL)
    {
        return CLEW_ERROR_OPEN_FAILED;
    }

    //  Set unloading
    error = atexit(clewExit);

    if (error)
    {
        //  Failure queing atexit, shutdown with error
        CLCC_DYNLIB_CLOSE(module);
        module = NULL;

        return CLEW_ERROR_ATEXIT_FAILED;
    }

    //  Determine function entry-points
    __oclGetPlatformIDs                = (PFNCLGETPLATFORMIDS              )CLCC_DYNLIB_IMPORT(module, "clGetPlatformIDs");
    __oclGetPlatformInfo               = (PFNCLGETPLATFORMINFO             )CLCC_DYNLIB_IMPORT(module, "clGetPlatformInfo");
    __oclGetDeviceIDs                  = (PFNCLGETDEVICEIDS                )CLCC_DYNLIB_IMPORT(module, "clGetDeviceIDs");
    __oclGetDeviceInfo                 = (PFNCLGETDEVICEINFO               )CLCC_DYNLIB_IMPORT(module, "clGetDeviceInfo");
    __oclCreateContext                 = (PFNCLCREATECONTEXT               )CLCC_DYNLIB_IMPORT(module, "clCreateContext");
    __oclCreateContextFromType         = (PFNCLCREATECONTEXTFROMTYPE       )CLCC_DYNLIB_IMPORT(module, "clCreateContextFromType");
    __oclRetainContext                 = (PFNCLRETAINCONTEXT               )CLCC_DYNLIB_IMPORT(module, "clRetainContext");
    __oclReleaseContext                = (PFNCLRELEASECONTEXT              )CLCC_DYNLIB_IMPORT(module, "clReleaseContext");
    __oclGetContextInfo                = (PFNCLGETCONTEXTINFO              )CLCC_DYNLIB_IMPORT(module, "clGetContextInfo");
    __oclCreateCommandQueue            = (PFNCLCREATECOMMANDQUEUE          )CLCC_DYNLIB_IMPORT(module, "clCreateCommandQueue");
    __oclRetainCommandQueue            = (PFNCLRETAINCOMMANDQUEUE          )CLCC_DYNLIB_IMPORT(module, "clRetainCommandQueue");
    __oclReleaseCommandQueue           = (PFNCLRELEASECOMMANDQUEUE         )CLCC_DYNLIB_IMPORT(module, "clReleaseCommandQueue");
    __oclGetCommandQueueInfo           = (PFNCLGETCOMMANDQUEUEINFO         )CLCC_DYNLIB_IMPORT(module, "clGetCommandQueueInfo");
    __oclSetCommandQueueProperty       = (PFNCLSETCOMMANDQUEUEPROPERTY     )CLCC_DYNLIB_IMPORT(module, "clSetCommandQueueProperty");
    __oclCreateBuffer                  = (PFNCLCREATEBUFFER                )CLCC_DYNLIB_IMPORT(module, "clCreateBuffer");
    __oclCreateImage2D                 = (PFNCLCREATEIMAGE2D               )CLCC_DYNLIB_IMPORT(module, "clCreateImage2D");
    __oclCreateImage3D                 = (PFNCLCREATEIMAGE3D               )CLCC_DYNLIB_IMPORT(module, "clCreateImage3D");
    __oclRetainMemObject               = (PFNCLRETAINMEMOBJECT             )CLCC_DYNLIB_IMPORT(module, "clRetainMemObject");
    __oclReleaseMemObject              = (PFNCLRELEASEMEMOBJECT            )CLCC_DYNLIB_IMPORT(module, "clReleaseMemObject");
    __oclGetSupportedImageFormats      = (PFNCLGETSUPPORTEDIMAGEFORMATS    )CLCC_DYNLIB_IMPORT(module, "clGetSupportedImageFormats");
    __oclGetMemObjectInfo              = (PFNCLGETMEMOBJECTINFO            )CLCC_DYNLIB_IMPORT(module, "clGetMemObjectInfo");
    __oclGetImageInfo                  = (PFNCLGETIMAGEINFO                )CLCC_DYNLIB_IMPORT(module, "clGetImageInfo");
    __oclCreateSampler                 = (PFNCLCREATESAMPLER               )CLCC_DYNLIB_IMPORT(module, "clCreateSampler");
    __oclRetainSampler                 = (PFNCLRETAINSAMPLER               )CLCC_DYNLIB_IMPORT(module, "clRetainSampler");
    __oclReleaseSampler                = (PFNCLRELEASESAMPLER              )CLCC_DYNLIB_IMPORT(module, "clReleaseSampler");
    __oclGetSamplerInfo                = (PFNCLGETSAMPLERINFO              )CLCC_DYNLIB_IMPORT(module, "clGetSamplerInfo");
    __oclCreateProgramWithSource       = (PFNCLCREATEPROGRAMWITHSOURCE     )CLCC_DYNLIB_IMPORT(module, "clCreateProgramWithSource");
    __oclCreateProgramWithBinary       = (PFNCLCREATEPROGRAMWITHBINARY     )CLCC_DYNLIB_IMPORT(module, "clCreateProgramWithBinary");
    __oclRetainProgram                 = (PFNCLRETAINPROGRAM               )CLCC_DYNLIB_IMPORT(module, "clRetainProgram");
    __oclReleaseProgram                = (PFNCLRELEASEPROGRAM              )CLCC_DYNLIB_IMPORT(module, "clReleaseProgram");
    __oclBuildProgram                  = (PFNCLBUILDPROGRAM                )CLCC_DYNLIB_IMPORT(module, "clBuildProgram");
    __oclUnloadCompiler                = (PFNCLUNLOADCOMPILER              )CLCC_DYNLIB_IMPORT(module, "clUnloadCompiler");
    __oclGetProgramInfo                = (PFNCLGETPROGRAMINFO              )CLCC_DYNLIB_IMPORT(module, "clGetProgramInfo");
    __oclGetProgramBuildInfo           = (PFNCLGETPROGRAMBUILDINFO         )CLCC_DYNLIB_IMPORT(module, "clGetProgramBuildInfo");
    __oclCreateKernel                  = (PFNCLCREATEKERNEL                )CLCC_DYNLIB_IMPORT(module, "clCreateKernel");
    __oclCreateKernelsInProgram        = (PFNCLCREATEKERNELSINPROGRAM      )CLCC_DYNLIB_IMPORT(module, "clCreateKernelsInProgram");
    __oclRetainKernel                  = (PFNCLRETAINKERNEL                )CLCC_DYNLIB_IMPORT(module, "clRetainKernel");
    __oclReleaseKernel                 = (PFNCLRELEASEKERNEL               )CLCC_DYNLIB_IMPORT(module, "clReleaseKernel");
    __oclSetKernelArg                  = (PFNCLSETKERNELARG                )CLCC_DYNLIB_IMPORT(module, "clSetKernelArg");
    __oclGetKernelInfo                 = (PFNCLGETKERNELINFO               )CLCC_DYNLIB_IMPORT(module, "clGetKernelInfo");
    __oclGetKernelWorkGroupInfo        = (PFNCLGETKERNELWORKGROUPINFO      )CLCC_DYNLIB_IMPORT(module, "clGetKernelWorkGroupInfo");
    __oclWaitForEvents                 = (PFNCLWAITFOREVENTS               )CLCC_DYNLIB_IMPORT(module, "clWaitForEvents");
    __oclGetEventInfo                  = (PFNCLGETEVENTINFO                )CLCC_DYNLIB_IMPORT(module, "clGetEventInfo");
    __oclRetainEvent                   = (PFNCLRETAINEVENT                 )CLCC_DYNLIB_IMPORT(module, "clRetainEvent");
    __oclReleaseEvent                  = (PFNCLRELEASEEVENT                )CLCC_DYNLIB_IMPORT(module, "clReleaseEvent");
    __oclGetEventProfilingInfo         = (PFNCLGETEVENTPROFILINGINFO       )CLCC_DYNLIB_IMPORT(module, "clGetEventProfilingInfo");
    __oclFlush                         = (PFNCLFLUSH                       )CLCC_DYNLIB_IMPORT(module, "clFlush");
    __oclFinish                        = (PFNCLFINISH                      )CLCC_DYNLIB_IMPORT(module, "clFinish");
    __oclEnqueueReadBuffer             = (PFNCLENQUEUEREADBUFFER           )CLCC_DYNLIB_IMPORT(module, "clEnqueueReadBuffer");
    __oclEnqueueWriteBuffer            = (PFNCLENQUEUEWRITEBUFFER          )CLCC_DYNLIB_IMPORT(module, "clEnqueueWriteBuffer");
    __oclEnqueueCopyBuffer             = (PFNCLENQUEUECOPYBUFFER           )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyBuffer");
    __oclEnqueueReadImage              = (PFNCLENQUEUEREADIMAGE            )CLCC_DYNLIB_IMPORT(module, "clEnqueueReadImage");
    __oclEnqueueWriteImage             = (PFNCLENQUEUEWRITEIMAGE           )CLCC_DYNLIB_IMPORT(module, "clEnqueueWriteImage");
    __oclEnqueueCopyImage              = (PFNCLENQUEUECOPYIMAGE            )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyImage");
    __oclEnqueueCopyImageToBuffer      = (PFNCLENQUEUECOPYIMAGETOBUFFER    )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyImageToBuffer");
    __oclEnqueueCopyBufferToImage      = (PFNCLENQUEUECOPYBUFFERTOIMAGE    )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyBufferToImage");
    __oclEnqueueMapBuffer              = (PFNCLENQUEUEMAPBUFFER            )CLCC_DYNLIB_IMPORT(module, "clEnqueueMapBuffer");
    __oclEnqueueMapImage               = (PFNCLENQUEUEMAPIMAGE             )CLCC_DYNLIB_IMPORT(module, "clEnqueueMapImage");
    __oclEnqueueUnmapMemObject         = (PFNCLENQUEUEUNMAPMEMOBJECT       )CLCC_DYNLIB_IMPORT(module, "clEnqueueUnmapMemObject");
    __oclEnqueueNDRangeKernel          = (PFNCLENQUEUENDRANGEKERNEL        )CLCC_DYNLIB_IMPORT(module, "clEnqueueNDRangeKernel");
    __oclEnqueueTask                   = (PFNCLENQUEUETASK                 )CLCC_DYNLIB_IMPORT(module, "clEnqueueTask");
    __oclEnqueueNativeKernel           = (PFNCLENQUEUENATIVEKERNEL         )CLCC_DYNLIB_IMPORT(module, "clEnqueueNativeKernel");
    __oclEnqueueMarker                 = (PFNCLENQUEUEMARKER               )CLCC_DYNLIB_IMPORT(module, "clEnqueueMarker");
    __oclEnqueueWaitForEvents          = (PFNCLENQUEUEWAITFOREVENTS        )CLCC_DYNLIB_IMPORT(module, "clEnqueueWaitForEvents");
    __oclEnqueueBarrier                = (PFNCLENQUEUEBARRIER              )CLCC_DYNLIB_IMPORT(module, "clEnqueueBarrier");
    __oclGetExtensionFunctionAddress   = (PFNCLGETEXTENSIONFUNCTIONADDRESS )CLCC_DYNLIB_IMPORT(module, "clGetExtensionFunctionAddress");
	
	if(__oclGetPlatformIDs == NULL) return CLEW_ERROR_OPEN_FAILED;
	if(__oclGetPlatformInfo == NULL) return CLEW_ERROR_OPEN_FAILED;
	if(__oclGetDeviceIDs == NULL) return CLEW_ERROR_OPEN_FAILED;
	if(__oclGetDeviceInfo == NULL) return CLEW_ERROR_OPEN_FAILED;

    return CLEW_SUCCESS;
}

//! \param error CL error code
//! \return a string representation of the error code
const char* clewErrorString(cl_int error)
{
    static const char* strings[] =
    {
        // Error Codes
          "CL_SUCCESS"                                  //   0
        , "CL_DEVICE_NOT_FOUND"                         //  -1
        , "CL_DEVICE_NOT_AVAILABLE"                     //  -2
        , "CL_COMPILER_NOT_AVAILABLE"                   //  -3
        , "CL_MEM_OBJECT_ALLOCATION_FAILURE"            //  -4
        , "CL_OUT_OF_RESOURCES"                         //  -5
        , "CL_OUT_OF_HOST_MEMORY"                       //  -6
        , "CL_PROFILING_INFO_NOT_AVAILABLE"             //  -7
        , "CL_MEM_COPY_OVERLAP"                         //  -8
        , "CL_IMAGE_FORMAT_MISMATCH"                    //  -9
        , "CL_IMAGE_FORMAT_NOT_SUPPORTED"               //  -10
        , "CL_BUILD_PROGRAM_FAILURE"                    //  -11
        , "CL_MAP_FAILURE"                              //  -12

        , ""    //  -13
        , ""    //  -14
        , ""    //  -15
        , ""    //  -16
        , ""    //  -17
        , ""    //  -18
        , ""    //  -19

        , ""    //  -20
        , ""    //  -21
        , ""    //  -22
        , ""    //  -23
        , ""    //  -24
        , ""    //  -25
        , ""    //  -26
        , ""    //  -27
        , ""    //  -28
        , ""    //  -29

        , "CL_INVALID_VALUE"                            //  -30
        , "CL_INVALID_DEVICE_TYPE"                      //  -31
        , "CL_INVALID_PLATFORM"                         //  -32
        , "CL_INVALID_DEVICE"                           //  -33
        , "CL_INVALID_CONTEXT"                          //  -34
        , "CL_INVALID_QUEUE_PROPERTIES"                 //  -35
        , "CL_INVALID_COMMAND_QUEUE"                    //  -36
        , "CL_INVALID_HOST_PTR"                         //  -37
        , "CL_INVALID_MEM_OBJECT"                       //  -38
        , "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"          //  -39
        , "CL_INVALID_IMAGE_SIZE"                       //  -40
        , "CL_INVALID_SAMPLER"                          //  -41
        , "CL_INVALID_BINARY"                           //  -42
        , "CL_INVALID_BUILD_OPTIONS"                    //  -43
        , "CL_INVALID_PROGRAM"                          //  -44
        , "CL_INVALID_PROGRAM_EXECUTABLE"               //  -45
        , "CL_INVALID_KERNEL_NAME"                      //  -46
        , "CL_INVALID_KERNEL_DEFINITION"                //  -47
        , "CL_INVALID_KERNEL"                           //  -48
        , "CL_INVALID_ARG_INDEX"                        //  -49
        , "CL_INVALID_ARG_VALUE"                        //  -50
        , "CL_INVALID_ARG_SIZE"                         //  -51
        , "CL_INVALID_KERNEL_ARGS"                      //  -52
        , "CL_INVALID_WORK_DIMENSION"                   //  -53
        , "CL_INVALID_WORK_GROUP_SIZE"                  //  -54
        , "CL_INVALID_WORK_ITEM_SIZE"                   //  -55
        , "CL_INVALID_GLOBAL_OFFSET"                    //  -56
        , "CL_INVALID_EVENT_WAIT_LIST"                  //  -57
        , "CL_INVALID_EVENT"                            //  -58
        , "CL_INVALID_OPERATION"                        //  -59
        , "CL_INVALID_GL_OBJECT"                        //  -60
        , "CL_INVALID_BUFFER_SIZE"                      //  -61
        , "CL_INVALID_MIP_LEVEL"                        //  -62
        , "CL_INVALID_GLOBAL_WORK_SIZE"                 //  -63
    };

    return strings[-error];
}
