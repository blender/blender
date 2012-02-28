//////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2009 Organic Vectory B.V.
//  Written by George van Venrooij
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file doc/license/Boost.txt)
//  Extracted from the CLCC project - http://clcc.sourceforge.net/
//////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include "util_opencl.h"

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

CCL_NAMESPACE_BEGIN

//! \brief module handle
static CLCC_DYNLIB_HANDLE module = NULL;

//  Variables holding function entry points
#ifndef CLCC_GENERATE_DOCUMENTATION
PFNCLGETPLATFORMIDS                 __clewGetPlatformIDs                = NULL;
PFNCLGETPLATFORMINFO                __clewGetPlatformInfo               = NULL;
PFNCLGETDEVICEIDS                   __clewGetDeviceIDs                  = NULL;
PFNCLGETDEVICEINFO                  __clewGetDeviceInfo                 = NULL;
PFNCLCREATECONTEXT                  __clewCreateContext                 = NULL;
PFNCLCREATECONTEXTFROMTYPE          __clewCreateContextFromType         = NULL;
PFNCLRETAINCONTEXT                  __clewRetainContext                 = NULL;
PFNCLRELEASECONTEXT                 __clewReleaseContext                = NULL;
PFNCLGETCONTEXTINFO                 __clewGetContextInfo                = NULL;
PFNCLCREATECOMMANDQUEUE             __clewCreateCommandQueue            = NULL;
PFNCLRETAINCOMMANDQUEUE             __clewRetainCommandQueue            = NULL;
PFNCLRELEASECOMMANDQUEUE            __clewReleaseCommandQueue           = NULL;
PFNCLGETCOMMANDQUEUEINFO            __clewGetCommandQueueInfo           = NULL;
PFNCLSETCOMMANDQUEUEPROPERTY        __clewSetCommandQueueProperty       = NULL;
PFNCLCREATEBUFFER                   __clewCreateBuffer                  = NULL;
PFNCLCREATEIMAGE2D                  __clewCreateImage2D                 = NULL;
PFNCLCREATEIMAGE3D                  __clewCreateImage3D                 = NULL;
PFNCLRETAINMEMOBJECT                __clewRetainMemObject               = NULL;
PFNCLRELEASEMEMOBJECT               __clewReleaseMemObject              = NULL;
PFNCLGETSUPPORTEDIMAGEFORMATS       __clewGetSupportedImageFormats      = NULL;
PFNCLGETMEMOBJECTINFO               __clewGetMemObjectInfo              = NULL;
PFNCLGETIMAGEINFO                   __clewGetImageInfo                  = NULL;
PFNCLCREATESAMPLER                  __clewCreateSampler                 = NULL;
PFNCLRETAINSAMPLER                  __clewRetainSampler                 = NULL;
PFNCLRELEASESAMPLER                 __clewReleaseSampler                = NULL;
PFNCLGETSAMPLERINFO                 __clewGetSamplerInfo                = NULL;
PFNCLCREATEPROGRAMWITHSOURCE        __clewCreateProgramWithSource       = NULL;
PFNCLCREATEPROGRAMWITHBINARY        __clewCreateProgramWithBinary       = NULL;
PFNCLRETAINPROGRAM                  __clewRetainProgram                 = NULL;
PFNCLRELEASEPROGRAM                 __clewReleaseProgram                = NULL;
PFNCLBUILDPROGRAM                   __clewBuildProgram                  = NULL;
PFNCLUNLOADCOMPILER                 __clewUnloadCompiler                = NULL;
PFNCLGETPROGRAMINFO                 __clewGetProgramInfo                = NULL;
PFNCLGETPROGRAMBUILDINFO            __clewGetProgramBuildInfo           = NULL;
PFNCLCREATEKERNEL                   __clewCreateKernel                  = NULL;
PFNCLCREATEKERNELSINPROGRAM         __clewCreateKernelsInProgram        = NULL;
PFNCLRETAINKERNEL                   __clewRetainKernel                  = NULL;
PFNCLRELEASEKERNEL                  __clewReleaseKernel                 = NULL;
PFNCLSETKERNELARG                   __clewSetKernelArg                  = NULL;
PFNCLGETKERNELINFO                  __clewGetKernelInfo                 = NULL;
PFNCLGETKERNELWORKGROUPINFO         __clewGetKernelWorkGroupInfo        = NULL;
PFNCLWAITFOREVENTS                  __clewWaitForEvents                 = NULL;
PFNCLGETEVENTINFO                   __clewGetEventInfo                  = NULL;
PFNCLRETAINEVENT                    __clewRetainEvent                   = NULL;
PFNCLRELEASEEVENT                   __clewReleaseEvent                  = NULL;
PFNCLGETEVENTPROFILINGINFO          __clewGetEventProfilingInfo         = NULL;
PFNCLFLUSH                          __clewFlush                         = NULL;
PFNCLFINISH                         __clewFinish                        = NULL;
PFNCLENQUEUEREADBUFFER              __clewEnqueueReadBuffer             = NULL;
PFNCLENQUEUEWRITEBUFFER             __clewEnqueueWriteBuffer            = NULL;
PFNCLENQUEUECOPYBUFFER              __clewEnqueueCopyBuffer             = NULL;
PFNCLENQUEUEREADIMAGE               __clewEnqueueReadImage              = NULL;
PFNCLENQUEUEWRITEIMAGE              __clewEnqueueWriteImage             = NULL;
PFNCLENQUEUECOPYIMAGE               __clewEnqueueCopyImage              = NULL;
PFNCLENQUEUECOPYIMAGETOBUFFER       __clewEnqueueCopyImageToBuffer      = NULL;
PFNCLENQUEUECOPYBUFFERTOIMAGE       __clewEnqueueCopyBufferToImage      = NULL;
PFNCLENQUEUEMAPBUFFER               __clewEnqueueMapBuffer              = NULL;
PFNCLENQUEUEMAPIMAGE                __clewEnqueueMapImage               = NULL;
PFNCLENQUEUEUNMAPMEMOBJECT          __clewEnqueueUnmapMemObject         = NULL;
PFNCLENQUEUENDRANGEKERNEL           __clewEnqueueNDRangeKernel          = NULL;
PFNCLENQUEUETASK                    __clewEnqueueTask                   = NULL;
PFNCLENQUEUENATIVEKERNEL            __clewEnqueueNativeKernel           = NULL;
PFNCLENQUEUEMARKER                  __clewEnqueueMarker                 = NULL;
PFNCLENQUEUEWAITFOREVENTS           __clewEnqueueWaitForEvents          = NULL;
PFNCLENQUEUEBARRIER                 __clewEnqueueBarrier                = NULL;
PFNCLGETEXTENSIONFUNCTIONADDRESS    __clewGetExtensionFunctionAddress   = NULL;
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
int clLibraryInit()
{
#ifdef _WIN32
	const char *path = "OpenCL.dll";
#elif defined(__APPLE__)
	const char *path = "/Library/Frameworks/OpenCL.framework/OpenCL";
#else
	const char *path = "libOpenCL.so";
#endif
    int error = 0;

    //  Check if already initialized
    if (module != NULL)
    {
        return 1;
    }

    //  Load library
    module = CLCC_DYNLIB_OPEN(path);

    //  Check for errors
    if (module == NULL)
    {
        return 0;
    }

    //  Set unloading
    error = atexit(clewExit);

    if (error)
    {
        //  Failure queing atexit, shutdown with error
        CLCC_DYNLIB_CLOSE(module);
        module = NULL;

        return 0;
    }

    //  Determine function entry-points
    __clewGetPlatformIDs                = (PFNCLGETPLATFORMIDS              )CLCC_DYNLIB_IMPORT(module, "clGetPlatformIDs");
    __clewGetPlatformInfo               = (PFNCLGETPLATFORMINFO             )CLCC_DYNLIB_IMPORT(module, "clGetPlatformInfo");
    __clewGetDeviceIDs                  = (PFNCLGETDEVICEIDS                )CLCC_DYNLIB_IMPORT(module, "clGetDeviceIDs");
    __clewGetDeviceInfo                 = (PFNCLGETDEVICEINFO               )CLCC_DYNLIB_IMPORT(module, "clGetDeviceInfo");
    __clewCreateContext                 = (PFNCLCREATECONTEXT               )CLCC_DYNLIB_IMPORT(module, "clCreateContext");
    __clewCreateContextFromType         = (PFNCLCREATECONTEXTFROMTYPE       )CLCC_DYNLIB_IMPORT(module, "clCreateContextFromType");
    __clewRetainContext                 = (PFNCLRETAINCONTEXT               )CLCC_DYNLIB_IMPORT(module, "clRetainContext");
    __clewReleaseContext                = (PFNCLRELEASECONTEXT              )CLCC_DYNLIB_IMPORT(module, "clReleaseContext");
    __clewGetContextInfo                = (PFNCLGETCONTEXTINFO              )CLCC_DYNLIB_IMPORT(module, "clGetContextInfo");
    __clewCreateCommandQueue            = (PFNCLCREATECOMMANDQUEUE          )CLCC_DYNLIB_IMPORT(module, "clCreateCommandQueue");
    __clewRetainCommandQueue            = (PFNCLRETAINCOMMANDQUEUE          )CLCC_DYNLIB_IMPORT(module, "clRetainCommandQueue");
    __clewReleaseCommandQueue           = (PFNCLRELEASECOMMANDQUEUE         )CLCC_DYNLIB_IMPORT(module, "clReleaseCommandQueue");
    __clewGetCommandQueueInfo           = (PFNCLGETCOMMANDQUEUEINFO         )CLCC_DYNLIB_IMPORT(module, "clGetCommandQueueInfo");
    __clewSetCommandQueueProperty       = (PFNCLSETCOMMANDQUEUEPROPERTY     )CLCC_DYNLIB_IMPORT(module, "clSetCommandQueueProperty");
    __clewCreateBuffer                  = (PFNCLCREATEBUFFER                )CLCC_DYNLIB_IMPORT(module, "clCreateBuffer");
    __clewCreateImage2D                 = (PFNCLCREATEIMAGE2D               )CLCC_DYNLIB_IMPORT(module, "clCreateImage2D");
    __clewCreateImage3D                 = (PFNCLCREATEIMAGE3D               )CLCC_DYNLIB_IMPORT(module, "clCreateImage3D");
    __clewRetainMemObject               = (PFNCLRETAINMEMOBJECT             )CLCC_DYNLIB_IMPORT(module, "clRetainMemObject");
    __clewReleaseMemObject              = (PFNCLRELEASEMEMOBJECT            )CLCC_DYNLIB_IMPORT(module, "clReleaseMemObject");
    __clewGetSupportedImageFormats      = (PFNCLGETSUPPORTEDIMAGEFORMATS    )CLCC_DYNLIB_IMPORT(module, "clGetSupportedImageFormats");
    __clewGetMemObjectInfo              = (PFNCLGETMEMOBJECTINFO            )CLCC_DYNLIB_IMPORT(module, "clGetMemObjectInfo");
    __clewGetImageInfo                  = (PFNCLGETIMAGEINFO                )CLCC_DYNLIB_IMPORT(module, "clGetImageInfo");
    __clewCreateSampler                 = (PFNCLCREATESAMPLER               )CLCC_DYNLIB_IMPORT(module, "clCreateSampler");
    __clewRetainSampler                 = (PFNCLRETAINSAMPLER               )CLCC_DYNLIB_IMPORT(module, "clRetainSampler");
    __clewReleaseSampler                = (PFNCLRELEASESAMPLER              )CLCC_DYNLIB_IMPORT(module, "clReleaseSampler");
    __clewGetSamplerInfo                = (PFNCLGETSAMPLERINFO              )CLCC_DYNLIB_IMPORT(module, "clGetSamplerInfo");
    __clewCreateProgramWithSource       = (PFNCLCREATEPROGRAMWITHSOURCE     )CLCC_DYNLIB_IMPORT(module, "clCreateProgramWithSource");
    __clewCreateProgramWithBinary       = (PFNCLCREATEPROGRAMWITHBINARY     )CLCC_DYNLIB_IMPORT(module, "clCreateProgramWithBinary");
    __clewRetainProgram                 = (PFNCLRETAINPROGRAM               )CLCC_DYNLIB_IMPORT(module, "clRetainProgram");
    __clewReleaseProgram                = (PFNCLRELEASEPROGRAM              )CLCC_DYNLIB_IMPORT(module, "clReleaseProgram");
    __clewBuildProgram                  = (PFNCLBUILDPROGRAM                )CLCC_DYNLIB_IMPORT(module, "clBuildProgram");
    __clewUnloadCompiler                = (PFNCLUNLOADCOMPILER              )CLCC_DYNLIB_IMPORT(module, "clUnloadCompiler");
    __clewGetProgramInfo                = (PFNCLGETPROGRAMINFO              )CLCC_DYNLIB_IMPORT(module, "clGetProgramInfo");
    __clewGetProgramBuildInfo           = (PFNCLGETPROGRAMBUILDINFO         )CLCC_DYNLIB_IMPORT(module, "clGetProgramBuildInfo");
    __clewCreateKernel                  = (PFNCLCREATEKERNEL                )CLCC_DYNLIB_IMPORT(module, "clCreateKernel");
    __clewCreateKernelsInProgram        = (PFNCLCREATEKERNELSINPROGRAM      )CLCC_DYNLIB_IMPORT(module, "clCreateKernelsInProgram");
    __clewRetainKernel                  = (PFNCLRETAINKERNEL                )CLCC_DYNLIB_IMPORT(module, "clRetainKernel");
    __clewReleaseKernel                 = (PFNCLRELEASEKERNEL               )CLCC_DYNLIB_IMPORT(module, "clReleaseKernel");
    __clewSetKernelArg                  = (PFNCLSETKERNELARG                )CLCC_DYNLIB_IMPORT(module, "clSetKernelArg");
    __clewGetKernelInfo                 = (PFNCLGETKERNELINFO               )CLCC_DYNLIB_IMPORT(module, "clGetKernelInfo");
    __clewGetKernelWorkGroupInfo        = (PFNCLGETKERNELWORKGROUPINFO      )CLCC_DYNLIB_IMPORT(module, "clGetKernelWorkGroupInfo");
    __clewWaitForEvents                 = (PFNCLWAITFOREVENTS               )CLCC_DYNLIB_IMPORT(module, "clWaitForEvents");
    __clewGetEventInfo                  = (PFNCLGETEVENTINFO                )CLCC_DYNLIB_IMPORT(module, "clGetEventInfo");
    __clewRetainEvent                   = (PFNCLRETAINEVENT                 )CLCC_DYNLIB_IMPORT(module, "clRetainEvent");
    __clewReleaseEvent                  = (PFNCLRELEASEEVENT                )CLCC_DYNLIB_IMPORT(module, "clReleaseEvent");
    __clewGetEventProfilingInfo         = (PFNCLGETEVENTPROFILINGINFO       )CLCC_DYNLIB_IMPORT(module, "clGetEventProfilingInfo");
    __clewFlush                         = (PFNCLFLUSH                       )CLCC_DYNLIB_IMPORT(module, "clFlush");
    __clewFinish                        = (PFNCLFINISH                      )CLCC_DYNLIB_IMPORT(module, "clFinish");
    __clewEnqueueReadBuffer             = (PFNCLENQUEUEREADBUFFER           )CLCC_DYNLIB_IMPORT(module, "clEnqueueReadBuffer");
    __clewEnqueueWriteBuffer            = (PFNCLENQUEUEWRITEBUFFER          )CLCC_DYNLIB_IMPORT(module, "clEnqueueWriteBuffer");
    __clewEnqueueCopyBuffer             = (PFNCLENQUEUECOPYBUFFER           )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyBuffer");
    __clewEnqueueReadImage              = (PFNCLENQUEUEREADIMAGE            )CLCC_DYNLIB_IMPORT(module, "clEnqueueReadImage");
    __clewEnqueueWriteImage             = (PFNCLENQUEUEWRITEIMAGE           )CLCC_DYNLIB_IMPORT(module, "clEnqueueWriteImage");
    __clewEnqueueCopyImage              = (PFNCLENQUEUECOPYIMAGE            )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyImage");
    __clewEnqueueCopyImageToBuffer      = (PFNCLENQUEUECOPYIMAGETOBUFFER    )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyImageToBuffer");
    __clewEnqueueCopyBufferToImage      = (PFNCLENQUEUECOPYBUFFERTOIMAGE    )CLCC_DYNLIB_IMPORT(module, "clEnqueueCopyBufferToImage");
    __clewEnqueueMapBuffer              = (PFNCLENQUEUEMAPBUFFER            )CLCC_DYNLIB_IMPORT(module, "clEnqueueMapBuffer");
    __clewEnqueueMapImage               = (PFNCLENQUEUEMAPIMAGE             )CLCC_DYNLIB_IMPORT(module, "clEnqueueMapImage");
    __clewEnqueueUnmapMemObject         = (PFNCLENQUEUEUNMAPMEMOBJECT       )CLCC_DYNLIB_IMPORT(module, "clEnqueueUnmapMemObject");
    __clewEnqueueNDRangeKernel          = (PFNCLENQUEUENDRANGEKERNEL        )CLCC_DYNLIB_IMPORT(module, "clEnqueueNDRangeKernel");
    __clewEnqueueTask                   = (PFNCLENQUEUETASK                 )CLCC_DYNLIB_IMPORT(module, "clEnqueueTask");
    __clewEnqueueNativeKernel           = (PFNCLENQUEUENATIVEKERNEL         )CLCC_DYNLIB_IMPORT(module, "clEnqueueNativeKernel");
    __clewEnqueueMarker                 = (PFNCLENQUEUEMARKER               )CLCC_DYNLIB_IMPORT(module, "clEnqueueMarker");
    __clewEnqueueWaitForEvents          = (PFNCLENQUEUEWAITFOREVENTS        )CLCC_DYNLIB_IMPORT(module, "clEnqueueWaitForEvents");
    __clewEnqueueBarrier                = (PFNCLENQUEUEBARRIER              )CLCC_DYNLIB_IMPORT(module, "clEnqueueBarrier");
    __clewGetExtensionFunctionAddress   = (PFNCLGETEXTENSIONFUNCTIONADDRESS )CLCC_DYNLIB_IMPORT(module, "clGetExtensionFunctionAddress");

	if(__clewGetPlatformIDs == NULL)
		return 0;

    return 1;
}

//! \param error CL error code
//! \return a string representation of the error code
const char *clErrorString(cl_int error)
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

CCL_NAMESPACE_END

