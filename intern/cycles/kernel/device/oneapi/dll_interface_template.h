/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 Intel Corporation */

/* device_capabilities() returns a C string that must be free'd with oneapi_free(). */
DLL_INTERFACE_CALL(oneapi_device_capabilities, char *)
DLL_INTERFACE_CALL(oneapi_free, void, void *)
DLL_INTERFACE_CALL(oneapi_get_memcapacity, size_t, SyclQueue *queue)

DLL_INTERFACE_CALL(oneapi_get_compute_units_amount, size_t, SyclQueue *queue)
DLL_INTERFACE_CALL(oneapi_iterate_devices, void, OneAPIDeviceIteratorCallback cb, void *user_ptr)
DLL_INTERFACE_CALL(oneapi_set_error_cb, void, OneAPIErrorCallback, void *user_ptr)

DLL_INTERFACE_CALL(oneapi_create_queue, bool, SyclQueue *&external_queue, int device_index)
DLL_INTERFACE_CALL(oneapi_free_queue, void, SyclQueue *queue)
DLL_INTERFACE_CALL(
    oneapi_usm_aligned_alloc_host, void *, SyclQueue *queue, size_t memory_size, size_t alignment)
DLL_INTERFACE_CALL(oneapi_usm_alloc_device, void *, SyclQueue *queue, size_t memory_size)
DLL_INTERFACE_CALL(oneapi_usm_free, void, SyclQueue *queue, void *usm_ptr)

DLL_INTERFACE_CALL(
    oneapi_usm_memcpy, bool, SyclQueue *queue, void *dest, void *src, size_t num_bytes)
DLL_INTERFACE_CALL(oneapi_queue_synchronize, bool, SyclQueue *queue)
DLL_INTERFACE_CALL(oneapi_usm_memset,
                   bool,
                   SyclQueue *queue,
                   void *usm_ptr,
                   unsigned char value,
                   size_t num_bytes)

DLL_INTERFACE_CALL(oneapi_run_test_kernel, bool, SyclQueue *queue)

/* Operation with Kernel globals structure - map of global/constant allocation - filled before
 * render/kernel execution As we don't know in cycles `sizeof` this - Cycles will manage just as
 * pointer. */
DLL_INTERFACE_CALL(oneapi_kernel_globals_size, bool, SyclQueue *queue, size_t &kernel_global_size)
DLL_INTERFACE_CALL(oneapi_set_global_memory,
                   void,
                   SyclQueue *queue,
                   void *kernel_globals,
                   const char *memory_name,
                   void *memory_device_pointer)

DLL_INTERFACE_CALL(oneapi_kernel_preferred_local_size,
                   size_t,
                   SyclQueue *queue,
                   const DeviceKernel kernel,
                   const size_t kernel_global_size)
DLL_INTERFACE_CALL(oneapi_enqueue_kernel,
                   bool,
                   KernelContext *context,
                   int kernel,
                   size_t global_size,
                   void **args)
