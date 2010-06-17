/*
   Copyright (C) 2010 Sony Computer Entertainment Inc.
   All rights reserved.

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/


#include "MiniCL/cl.h"
#define __PHYSICS_COMMON_H__ 1
#ifdef _WIN32
#include "BulletMultiThreaded/Win32ThreadSupport.h"
#endif

#include "BulletMultiThreaded/SequentialThreadSupport.h"
#include "MiniCLTaskScheduler.h"
#include "MiniCLTask/MiniCLTask.h"
#include "LinearMath/btMinMax.h"

//#define DEBUG_MINICL_KERNELS 1




CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(
	cl_device_id            device ,
	cl_device_info          param_name ,
	size_t                  param_value_size ,
	void *                  param_value ,
	size_t *                /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0
{

	switch (param_name)
	{
	case CL_DEVICE_NAME:
		{
			char deviceName[] = "CPU";
			unsigned int nameLen = strlen(deviceName)+1;
			assert(param_value_size>strlen(deviceName));
			if (nameLen < param_value_size)
			{
				const char* cpuName = "CPU";
				sprintf((char*)param_value,"%s",cpuName);
			} else
			{
				printf("error: param_value_size should be at least %d, but it is %d\n",nameLen,param_value_size);
			}
			break;
		}
	case CL_DEVICE_TYPE:
		{
			if (param_value_size>=sizeof(cl_device_type))
			{
				cl_device_type* deviceType = (cl_device_type*)param_value;
				*deviceType = CL_DEVICE_TYPE_CPU;
			} else
			{
				printf("error: param_value_size should be at least %d\n",sizeof(cl_device_type));
			}
			break;
		}
	case CL_DEVICE_MAX_COMPUTE_UNITS:
		{
			if (param_value_size>=sizeof(cl_uint))
			{
				cl_uint* numUnits = (cl_uint*)param_value;
				*numUnits= 4;
			} else
			{
				printf("error: param_value_size should be at least %d\n",sizeof(cl_uint));
			}

			break;
		}
	case CL_DEVICE_MAX_WORK_ITEM_SIZES:
		{
			size_t workitem_size[3];

			if (param_value_size>=sizeof(workitem_size))
			{
				size_t* workItemSize = (size_t*)param_value;
				workItemSize[0] = 64;
				workItemSize[1] = 24;
				workItemSize[2] = 16;
			} else
			{
				printf("error: param_value_size should be at least %d\n",sizeof(cl_uint));
			}
			break;
		}
	case CL_DEVICE_MAX_CLOCK_FREQUENCY:
		{
			 cl_uint* clock_frequency = (cl_uint*)param_value;
			 *clock_frequency = 3*1024;
			break;
		}
	default:
		{
			printf("error: unsupported param_name:%d\n",param_name);
		}
	}


	return 0;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseMemObject(cl_mem /* memobj */) CL_API_SUFFIX__VERSION_1_0
{
	return 0;
}



CL_API_ENTRY cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0
{
	return 0;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseProgram(cl_program /* program */) CL_API_SUFFIX__VERSION_1_0
{
	return 0;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseKernel(cl_kernel   /* kernel */) CL_API_SUFFIX__VERSION_1_0
{
	return 0;
}


// Enqueued Commands APIs
CL_API_ENTRY cl_int CL_API_CALL clEnqueueReadBuffer(cl_command_queue     command_queue ,
                    cl_mem               buffer ,
                    cl_bool             /* blocking_read */,
                    size_t               offset ,
                    size_t               cb , 
                    void *               ptr ,
                    cl_uint             /* num_events_in_wait_list */,
                    const cl_event *    /* event_wait_list */,
                    cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0
{
	MiniCLTaskScheduler* scheduler = (MiniCLTaskScheduler*) command_queue;

	///wait for all work items to be completed
	scheduler->flush();

	memcpy(ptr,(char*)buffer + offset,cb);
	return 0;
}


CL_API_ENTRY cl_int clGetProgramBuildInfo(cl_program            /* program */,
                      cl_device_id          /* device */,
                      cl_program_build_info /* param_name */,
                      size_t                /* param_value_size */,
                      void *                /* param_value */,
                      size_t *              /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0
{

	return 0;
}


// Program Object APIs
CL_API_ENTRY cl_program
clCreateProgramWithSource(cl_context         context ,
                          cl_uint           /* count */,
                          const char **     /* strings */,
                          const size_t *    /* lengths */,
                          cl_int *          errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
	*errcode_ret = CL_SUCCESS;
	return (cl_program)context;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueWriteBuffer(cl_command_queue     command_queue ,
                    cl_mem               buffer ,
                    cl_bool             /* blocking_read */,
                    size_t              offset,
                    size_t               cb , 
                    const void *         ptr ,
                    cl_uint             /* num_events_in_wait_list */,
                    const cl_event *    /* event_wait_list */,
                    cl_event *          /* event */) CL_API_SUFFIX__VERSION_1_0
{
	MiniCLTaskScheduler* scheduler = (MiniCLTaskScheduler*) command_queue;

	///wait for all work items to be completed
	scheduler->flush();

	memcpy((char*)buffer + offset, ptr,cb);
	return 0;
}

CL_API_ENTRY cl_int CL_API_CALL clFlush(cl_command_queue  command_queue)
{
	MiniCLTaskScheduler* scheduler = (MiniCLTaskScheduler*) command_queue;
	///wait for all work items to be completed
	scheduler->flush();
	return 0;
}


CL_API_ENTRY cl_int CL_API_CALL clEnqueueNDRangeKernel(cl_command_queue /* command_queue */,
                       cl_kernel         clKernel ,
                       cl_uint           work_dim ,
                       const size_t *   /* global_work_offset */,
                       const size_t *    global_work_size ,
                       const size_t *   /* local_work_size */,
                       cl_uint          /* num_events_in_wait_list */,
                       const cl_event * /* event_wait_list */,
                       cl_event *       /* event */) CL_API_SUFFIX__VERSION_1_0
{

	
	MiniCLKernel* kernel = (MiniCLKernel*) clKernel;
	for (unsigned int ii=0;ii<work_dim;ii++)
	{
		int maxTask = kernel->m_scheduler->getMaxNumOutstandingTasks();
		int numWorkItems = global_work_size[ii];

//		//at minimum 64 work items per task
//		int numWorkItemsPerTask = btMax(64,numWorkItems / maxTask);
		int numWorkItemsPerTask = numWorkItems / maxTask;
		if (!numWorkItemsPerTask) numWorkItemsPerTask = 1;

		for (int t=0;t<numWorkItems;)
		{
			//Performance Hint: tweak this number during benchmarking
			int endIndex = (t+numWorkItemsPerTask) < numWorkItems ? t+numWorkItemsPerTask : numWorkItems;
			kernel->m_scheduler->issueTask(t, endIndex, kernel);
			t = endIndex;
		}
	}
/*

	void* bla = 0;

	scheduler->issueTask(bla,2,3);
	scheduler->flush();

	*/

	return 0;
}

#define LOCAL_BUF_SIZE 32768
static int sLocalMemBuf[LOCAL_BUF_SIZE * 4 + 16];
static int* spLocalBufCurr = NULL;
static int sLocalBufUsed = LOCAL_BUF_SIZE; // so it will be reset at the first call
static void* localBufMalloc(int size)
{
	int size16 = (size + 15) >> 4; // in 16-byte units
	if((sLocalBufUsed + size16) > LOCAL_BUF_SIZE)
	{ // reset
		spLocalBufCurr = sLocalMemBuf;
		while((long)spLocalBufCurr & 0x0F) spLocalBufCurr++; // align to 16 bytes
		sLocalBufUsed = 0;
	}
	void* ret = spLocalBufCurr;
	spLocalBufCurr += size16 * 4;
	sLocalBufUsed += size;
	return ret;
}



CL_API_ENTRY cl_int CL_API_CALL clSetKernelArg(cl_kernel    clKernel ,
               cl_uint      arg_index ,
               size_t       arg_size ,
               const void *  arg_value ) CL_API_SUFFIX__VERSION_1_0
{
	MiniCLKernel* kernel = (MiniCLKernel* ) clKernel;
	btAssert(arg_size <= MINICL_MAX_ARGLENGTH);
	if (arg_index>MINI_CL_MAX_ARG)
	{
		printf("error: clSetKernelArg arg_index (%d) exceeds %d\n",arg_index,MINI_CL_MAX_ARG);
	} else
	{
//		if (arg_size>=MINICL_MAX_ARGLENGTH)
		if (arg_size != MINICL_MAX_ARGLENGTH)
		{
			printf("error: clSetKernelArg argdata too large: %d (maximum is %d)\n",arg_size,MINICL_MAX_ARGLENGTH);
		} 
		else
		{
			if(arg_value == NULL)
			{	// this is only for __local memory qualifier
				void* ptr = localBufMalloc(arg_size);
				kernel->m_argData[arg_index] = ptr;
			}
			else
			{
				memcpy(&(kernel->m_argData[arg_index]), arg_value, arg_size);
			}
			kernel->m_argSizes[arg_index] = arg_size;
			if(arg_index >= kernel->m_numArgs)
			{
				kernel->m_numArgs = arg_index + 1;
				kernel->updateLauncher();
			}
		}
	}
	return 0;
}

// Kernel Object APIs
CL_API_ENTRY cl_kernel CL_API_CALL clCreateKernel(cl_program       program ,
               const char *     kernel_name ,
               cl_int *         errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
	MiniCLTaskScheduler* scheduler = (MiniCLTaskScheduler*) program;
	MiniCLKernel* kernel = new MiniCLKernel();
	int nameLen = strlen(kernel_name);
	if(nameLen >= MINI_CL_MAX_KERNEL_NAME)
	{
		*errcode_ret = CL_INVALID_KERNEL_NAME;
		return NULL;
	}
	strcpy(kernel->m_name, kernel_name);
	kernel->m_numArgs = 0;

	//kernel->m_kernelProgramCommandId = scheduler->findProgramCommandIdByName(kernel_name);
	//if (kernel->m_kernelProgramCommandId>=0)
	//{
	//	*errcode_ret = CL_SUCCESS;
	//} else
	//{
	//	*errcode_ret = CL_INVALID_KERNEL_NAME;
	//}
	kernel->m_scheduler = scheduler;
	if(kernel->registerSelf() == NULL)
	{
		*errcode_ret = CL_INVALID_KERNEL_NAME;
		return NULL;
	}
	else
	{
		*errcode_ret = CL_SUCCESS;
	}

	return (cl_kernel)kernel;

}


CL_API_ENTRY cl_int CL_API_CALL clBuildProgram(cl_program           /* program */,
               cl_uint              /* num_devices */,
               const cl_device_id * /* device_list */,
               const char *         /* options */, 
               void (*pfn_notify)(cl_program /* program */, void * /* user_data */),
               void *               /* user_data */) CL_API_SUFFIX__VERSION_1_0
{
	return CL_SUCCESS;
}

CL_API_ENTRY cl_program CL_API_CALL clCreateProgramWithBinary(cl_context                     context ,
                          cl_uint                        /* num_devices */,
                          const cl_device_id *           /* device_list */,
                          const size_t *                 /* lengths */,
                          const unsigned char **         /* binaries */,
                          cl_int *                       /* binary_status */,
                          cl_int *                       /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0
{
	return (cl_program)context;
}


// Memory Object APIs
CL_API_ENTRY cl_mem CL_API_CALL clCreateBuffer(cl_context   /* context */,
               cl_mem_flags flags ,
               size_t       size,
               void *       host_ptr ,
               cl_int *     errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
	cl_mem buf = (cl_mem)malloc(size);
	if ((flags&CL_MEM_COPY_HOST_PTR) && host_ptr)
	{
		memcpy(buf,host_ptr,size);
	}
	*errcode_ret = 0;
	return buf;
}

// Command Queue APIs
CL_API_ENTRY cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context                      context , 
                     cl_device_id                   /* device */, 
                     cl_command_queue_properties    /* properties */,
                     cl_int *                        errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
	*errcode_ret = 0;
	return (cl_command_queue) context;
}

extern CL_API_ENTRY cl_int CL_API_CALL clGetContextInfo(cl_context         /* context */, 
                 cl_context_info    param_name , 
                 size_t             param_value_size , 
                 void *             param_value, 
                 size_t *           param_value_size_ret ) CL_API_SUFFIX__VERSION_1_0
{

	switch (param_name)
	{
	case CL_CONTEXT_DEVICES:
		{
			if (!param_value_size)
			{
				*param_value_size_ret = 13;
			} else
			{
				const char* testName = "MiniCL_Test.";
				sprintf((char*)param_value,"%s",testName);
			}
			break;
		};
	default:
		{
			printf("unsupported\n");
		}
	}
	
	return 0;
}

CL_API_ENTRY cl_context CL_API_CALL clCreateContextFromType(cl_context_properties * /* properties */,
                        cl_device_type          /* device_type */,
                        void (*pfn_notify)(const char *, const void *, size_t, void *) /* pfn_notify */,
                        void *                  /* user_data */,
                        cl_int *                 errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
	int maxNumOutstandingTasks = 4;
//	int maxNumOutstandingTasks = 2;
//	int maxNumOutstandingTasks = 1;
	gMiniCLNumOutstandingTasks = maxNumOutstandingTasks;
	const int maxNumOfThreadSupports = 8;
	static int sUniqueThreadSupportIndex = 0;
	static char* sUniqueThreadSupportName[maxNumOfThreadSupports] = 
	{
		"MiniCL_0", "MiniCL_1", "MiniCL_2", "MiniCL_3", "MiniCL_4", "MiniCL_5", "MiniCL_6", "MiniCL_7" 
	};

#ifdef DEBUG_MINICL_KERNELS
	SequentialThreadSupport::SequentialThreadConstructionInfo stc("MiniCL",processMiniCLTask,createMiniCLLocalStoreMemory);
	SequentialThreadSupport* threadSupport = new SequentialThreadSupport(stc);
#else

#if _WIN32
	btAssert(sUniqueThreadSupportIndex < maxNumOfThreadSupports);
	Win32ThreadSupport* threadSupport = new Win32ThreadSupport(Win32ThreadSupport::Win32ThreadConstructionInfo(
//								"MiniCL",
								sUniqueThreadSupportName[sUniqueThreadSupportIndex++],
								processMiniCLTask, //processCollisionTask,
								createMiniCLLocalStoreMemory,//createCollisionLocalStoreMemory,
								maxNumOutstandingTasks));
#else
	///todo: add posix thread support for other platforms
	SequentialThreadSupport::SequentialThreadConstructionInfo stc("MiniCL",processMiniCLTask,createMiniCLLocalStoreMemory);
	SequentialThreadSupport* threadSupport = new SequentialThreadSupport(stc);
#endif

#endif //DEBUG_MINICL_KERNELS
	
	
	MiniCLTaskScheduler* scheduler = new MiniCLTaskScheduler(threadSupport,maxNumOutstandingTasks);

	*errcode_ret = 0;
	return (cl_context)scheduler;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseContext(cl_context  context ) CL_API_SUFFIX__VERSION_1_0
{

	MiniCLTaskScheduler* scheduler = (MiniCLTaskScheduler*) context;
	
	btThreadSupportInterface* threadSupport = scheduler->getThreadSupportInterface();
	delete scheduler;
	delete threadSupport;
	
	return 0;
}
extern CL_API_ENTRY cl_int CL_API_CALL
clFinish(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0
{
	return CL_SUCCESS;
}


extern CL_API_ENTRY cl_int CL_API_CALL
clGetKernelWorkGroupInfo(cl_kernel                   kernel ,
                         cl_device_id               /* device */,
                         cl_kernel_work_group_info  wgi/* param_name */,
                         size_t   sz                  /* param_value_size */,
                         void *     ptr                /* param_value */,
                         size_t *                   /* param_value_size_ret */) CL_API_SUFFIX__VERSION_1_0
{
	if((wgi == CL_KERNEL_WORK_GROUP_SIZE)
	 &&(sz == sizeof(int))
	 &&(ptr != NULL))
	{
		MiniCLKernel* miniCLKernel = (MiniCLKernel*)kernel;
		MiniCLTaskScheduler* scheduler = miniCLKernel->m_scheduler;
		*((int*)ptr) = scheduler->getMaxNumOutstandingTasks();
		return CL_SUCCESS;
	}
	else
	{
		return CL_INVALID_VALUE;
	}
}
