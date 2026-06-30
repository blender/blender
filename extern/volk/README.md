# 🐺 volk [![Build Status](https://github.com/zeux/volk/workflows/build/badge.svg)](https://github.com/zeux/volk/actions) 

## Purpose

volk is a meta-loader for Vulkan. It allows you to dynamically load entrypoints required to use Vulkan
without linking to vulkan-1.dll or statically linking Vulkan loader. Additionally, volk simplifies the use of Vulkan extensions by automatically loading all associated entrypoints. Finally, volk enables loading
Vulkan entrypoints directly from the driver which can increase performance by skipping loader dispatch overhead.

volk is written in C89 and supports Windows, Linux, Android and macOS (via MoltenVK).

## Building

There are multiple ways to use volk in your project:

1. You can add `volk.c` to your build system. Note that the usual preprocessor defines that enable Vulkan's platform-specific functions (VK_USE_PLATFORM_WIN32_KHR, VK_USE_PLATFORM_XLIB_KHR, VK_USE_PLATFORM_MACOS_MVK, etc) must be passed as desired to the compiler when building `volk.c`.
2. You can use provided CMake files, with the usage detailed below.
3. You can use volk in header-only fashion. Include `volk.h` wherever you want to use Vulkan functions. In exactly one source file, define `VOLK_IMPLEMENTATION` before including `volk.h`. Do not build `volk.c` at all in this case - however, `volk.c` must still be in the same directory as `volk.h`. This method of integrating volk makes it possible to set the platform defines mentioned above with arbitrary (preprocessor) logic in your code.

## Basic usage

To use volk, you have to include `volk.h` instead of `vulkan/vulkan.h`; this is necessary to use function definitions from volk.

If some files in your application include `vulkan/vulkan.h` and don't include `volk.h`, this can result in symbol conflicts; consider defining `VK_NO_PROTOTYPES` when compiling code that uses Vulkan to make sure this doesn't happen. It's also important to make sure that `vulkan-1` is not linked into the application, as this results in symbol name conflicts as well.

To initialize volk, call this function first:

```c++
VkResult volkInitialize();
```

This will attempt to load Vulkan loader from the system; if this function returns `VK_SUCCESS` you can proceed to create Vulkan instance.
If this function fails, this means Vulkan loader isn't installed on your system.

After creating the Vulkan instance using Vulkan API, call this function:

```c++
void volkLoadInstance(VkInstance instance);
```

This function will load all required Vulkan entrypoints, including all extensions; you can use Vulkan from here on as usual.

## Optimizing device calls

If you use volk as described in the previous section, all device-related function calls, such as `vkCmdDraw`, will go through Vulkan loader dispatch code.
This allows you to transparently support multiple VkDevice objects in the same application, but comes at a price of dispatch overhead which can be as high as 7% depending on the driver and application.

To avoid this, you have two options:

1. For applications that use just one VkDevice object, load device-related Vulkan entrypoints directly from the driver with this function:

```c++
void volkLoadDevice(VkDevice device);
```

2. For applications that use multiple VkDevice objects, load device-related Vulkan entrypoints into a table:

```c++
void volkLoadDeviceTable(struct VolkDeviceTable* table, VkDevice device);
```

The second option requires you to change the application code to store one `VolkDeviceTable` per `VkDevice` and call functions from this table instead.

Device entrypoints are loaded using `vkGetDeviceProcAddr`; when no layers are present, this commonly results in most function pointers pointing directly at the driver functions, minimizing the call overhead. When layers are loaded, the entrypoints will point at the implementations in the first applicable layer, so this is compatible with any layers including validation layers.

Since `volkLoadDevice` overwrites some function pointers with device-specific versions, you can choose to use `volkLoadInstanceOnly` instead of `volkLoadInstance`; when using table-based interface this can also help enforce the usage of the function tables as `volkLoadInstanceOnly` will leave device-specific functions as `NULL`.

## CMake support

If your project uses CMake, volk provides you with targets corresponding to the different use cases:

1. Target `volk` is a static library. Any platform defines can be passed to the compiler by setting `VOLK_STATIC_DEFINES`. Example:
```cmake
if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif()
   ...
endif()
add_subdirectory(volk)
target_link_library(my_application PRIVATE volk)
```
2. Target `volk_headers` is an interface target for the header-only style. Example:
```cmake
add_subdirectory(volk)
target_link_library(my_application PRIVATE volk_headers)
```
and in the code:
```c
/* ...any logic setting VK_USE_PLATFORM_WIN32_KHR and friends... */
#define VOLK_IMPLEMENTATION
#include "volk.h"
```

The above example use `add_subdirectory` to include volk into CMake's build tree. This is a good choice if you copy the volk files into your project tree or as a git submodule.

volk also supports installation and config-file packages. Installation is disabled by default (so as to not pollute user projects with install rules), and can be enabled by passing `-DVOLK_INSTALL=ON` to CMake. Once installed, do something like `find_package(volk CONFIG REQUIRED)` in your project's CMakeLists.txt. The imported volk targets are called `volk::volk` and `volk::volk_headers`.

## Configuration

By default, volk is compiled as a C library and exposes all Vulkan function pointers as globals. This can result in symbol conflicts if some libraries in the application are still linking to Vulkan libraries directly. While generally speaking it's desirable to not mix & match volk with direct usage of Vulkan - for example, mixed usage means the application still links directly to Vulkan libraries and will fail to launch if Vulkan is not available on the user's system - it's possible to enable `VOLK_NAMESPACE` CMake option (or `VOLK_NAMESPACE` define when building volk manually), which places all volk symbols into `volk::` namespace. This requires compiling `volk.c` in C++ mode, which happens automatically when using CMake, but doesn'trequire any other changes.

Device level functions can be hidden by defining `VOLK_NO_DEVICE_PROTOTYPES`. When using `volkLoadInstanceOnly` and `volkLoadDeviceTable` the device level functions are never loaded and when not used correctly would trigger a runtime error. By hiding the device prototypes mistakes can be checked by the compiler.

## License

This library is available to anybody free of charge, under the terms of MIT License (see LICENSE.md).
