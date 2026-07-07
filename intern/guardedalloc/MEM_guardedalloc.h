/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 *
 * \brief Read \ref MEMPage
 *
 * \page MEMPage Blender memory allocation and freeing
 *
 * \section aboutmem About the MEM allocator module
 *
 * \subsection Guarded memory allocation
 *
 * MEM provides guarded memory management when using the --debug-memory option. All allocated
 * memory is then enclosed by pads, to detect out-of-bound writes. All allocations are named
 * to detect the source of memory leaks and print memory usage at runtime with the
 * "Memory Statistics" operator.
 *
 * \subsection How to use the MEM API
 *
 * MEM provides C++ template versions of the `new`/`delete` operators (#MEM_new and #MEM_delete),
 * which are the preferred way to create and delete data in new code.
 *
 * It also provides #MEM_new_uninitialized and #MEM_new_zeroed functions that behave like malloc
 * and calloc respectively. These provide improved type safety, ensure that the allocated types
 * are trivial, and reduce the casting verbosity by directly returning a pointer of the expected
 * type. This memory should be freed with either #MEM_delete or #MEM_delete_void.
 *
 * There are a few reasons to use these functions:
 * - Performance: When allocating large arrays like image buffers or mesh attributes that will
 *   be initialized soon after, doing an uninitialized memory allocation is faster.
 * - Low level code: Core data structures and other low level code that directly allocate untyped
 *   memory buffers and provide their own type safety.
 * - Legacy: Code that has not yet been updated to follow current conventions.
 *
 * \subsection memdependencies Dependencies
 * - `stdlib`
 * - `stdio`
 *
 * \subsection memdocs API Documentation
 * See \ref MEM_guardedalloc.h
 */

#ifndef __MEM_GUARDEDALLOC_H__
#define __MEM_GUARDEDALLOC_H__

/* Needed for uintptr_t and attributes, exception, don't use BLI anywhere else in `MEM_*` */
#include "../../source/blender/blenlib/BLI_compiler_attrs.h"
#include "../../source/blender/blenlib/BLI_sys_types.h"

#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/**
 * \name Untyped Allocation API.
 *
 * Defines the 'C-style' part of the API, where memory management is fully untyped (i.e. done with
 * void pointers and explicit size values).
 *
 * This API should usually not be used anymore in C++ code, unless some form of raw memory
 * management is necessary (e.g. for allocation of various ID types based on their
 * #IDTypeInfo::struct_size data).
 *
 * \{ */

/**
 * Returns the length of the allocated memory segment pointed at
 * by vmemh. If the pointer was not previously allocated by this
 * module, the result is undefined.
 */
extern size_t (*MEM_allocN_len)(const void *vmemh) ATTR_WARN_UNUSED_RESULT;

/**
 * Release memory previously allocated by the C-style functions of this module.
 *
 * It is illegal to call this function with data allocated by #MEM_new.
 */
void MEM_delete_void(void *vmemh);

#if 0 /* UNUSED */
/**
 * Return zero if memory is not in allocated list
 */
extern short (*MEM_testN)(void *vmemh);
#endif

/**
 * Duplicates a block of memory, and returns a pointer to the
 * newly allocated block.
 * NULL-safe; will return NULL when receiving a NULL pointer.
 *
 * Use this version only for void pointers. */
void *MEM_dupalloc_void(const void *vmemh) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT;

/**
 * Reallocates a block of memory, and returns pointer to the newly
 * allocated block, the old one is freed. this is not as optimized
 * as a system realloc but just makes a new allocation and copies
 * over from existing memory. */
extern void *(*MEM_realloc_uninitialized_id)(
    void *vmemh, size_t len, const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(2);

/**
 * A variant of realloc which zeros new bytes
 */
extern void *(*MEM_realloc_zeroed_id)(void *vmemh,
                                      size_t len,
                                      const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(2);

#define MEM_realloc_uninitialized(vmemh, len) MEM_realloc_uninitialized_id(vmemh, len, __func__)
#define MEM_realloc_zeroed(vmemh, len) MEM_realloc_zeroed_id(vmemh, len, __func__)

/**
 * Allocate a block of memory of size len, with tag name str. The
 * memory is cleared. The name must be static, because only a
 * pointer to it is stored!
 */
void *MEM_new_zeroed(size_t len, const char *str) ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1)
    ATTR_NONNULL(2);

/**
 * Allocate a block of memory of size (len * size), with tag name
 * str, aborting in case of integer overflows to prevent vulnerabilities.
 * The memory is cleared. The name must be static, because only a
 * pointer to it is stored! */
void *MEM_new_array_zeroed(size_t len,
                           size_t size,
                           const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1, 2) ATTR_NONNULL(3);

/**
 * Allocate a block of memory of size len, with tag name str. The
 * name must be a static, because only a pointer to it is stored!
 */
void *MEM_new_uninitialized(size_t len, const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);

/**
 * Allocate a block of memory of size (len * size), with tag name str,
 * aborting in case of integer overflow to prevent vulnerabilities. The
 * name must be a static, because only a pointer to it is stored!
 */
void *MEM_new_array_uninitialized(size_t len,
                                  size_t size,
                                  const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1, 2) ATTR_NONNULL(3);

/**
 * Allocate an aligned block of memory of size len, with tag name str. The
 * name must be a static, because only a pointer to it is stored!
 */
void *MEM_new_uninitialized_aligned(size_t len,
                                    size_t alignment,
                                    const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1) ATTR_NONNULL(3);

/**
 * Allocate an aligned block of memory that remains uninitialized.
 */
extern void *(*MEM_new_array_uninitialized_aligned)(
    size_t len,
    size_t size,
    size_t alignment,
    const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1, 2)
    ATTR_NONNULL(4);

/**
 * Allocate an aligned block of memory that is initialized with zeros.
 */
extern void *(*MEM_new_array_zeroed_aligned)(
    size_t len,
    size_t size,
    size_t alignment,
    const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1, 2)
    ATTR_NONNULL(4);

/** \} */

/* -------------------------------------------------------------------- */
/**
 * \name Various Helpers.
 *
 * These functions allow to control the behavior of the guarded allocator, and to retrieve (debug)
 * information about allocated memory.
 */

/**
 * Print a list of the names and sizes of all allocated memory
 * blocks. as a python dict for easy investigation.
 */
extern void (*MEM_printmemlist_pydict)();

/**
 * Print a list of the names and sizes of all allocated memory blocks.
 */
extern void (*MEM_printmemlist)();

/** calls the function on all allocated memory blocks. */
extern void (*MEM_callbackmemlist)(void (*func)(void *));

/** Print statistics about memory usage */
extern void (*MEM_printmemlist_stats)();

/** Set the callback function for error output. */
extern void (*MEM_set_error_callback)(void (*func)(const char *));

/**
 * Are the start/end block markers still correct ?
 *
 * \retval true for correct memory, false for corrupted memory.
 */
extern bool (*MEM_consistency_check)();

/** Attempt to enforce OSX (or other OS's) to have malloc and stack nonzero */
extern void (*MEM_set_memory_debug)();

/** Memory usage stats. */
extern size_t (*MEM_get_memory_in_use)();
/** Get amount of memory blocks in use. */
extern unsigned int (*MEM_get_memory_blocks_in_use)();

/** Reset the peak memory statistic to zero. */
extern void (*MEM_reset_peak_memory)();

/** Get the peak memory usage in bytes, including `mmap` allocations. */
extern size_t (*MEM_get_peak_memory)() ATTR_WARN_UNUSED_RESULT;

/** Overhead for lockfree allocator (use to avoid slop-space). */
#define MEM_SIZE_OVERHEAD sizeof(size_t)
#define MEM_SIZE_OPTIMAL(size) ((size) - MEM_SIZE_OVERHEAD)

#ifndef NDEBUG
extern const char *(*MEM_name_ptr)(void *vmemh);
/**
 * Change the debugging name/string assigned to the memory allocated at \a vmemh. Only affects the
 * guarded allocator. The name must be a static string, because only a pointer to it is stored!
 *
 * Handy when debugging leaking memory allocated by some often called, generic function with a
 * unspecific name. A caller with more info can set a more specific name, and see which call to the
 * generic function allocates the leaking memory.
 */
extern void (*MEM_name_ptr_set)(void *vmemh, const char *str) ATTR_NONNULL();
#endif

/**
 * This should be called as early as possible in the program. When it has been called, information
 * about memory leaks will be printed on exit.
 */
void MEM_init_memleak_detection(void);

/**
 * When this has been called and memory leaks have been detected, the process will have an exit
 * code that indicates failure. This can be used for when checking for memory leaks with automated
 * tests.
 */
void MEM_enable_fail_on_memleak(void);

/**
 * Switch allocator to fast mode, with less tracking.
 *
 * Use in the production code where performance is the priority, and exact details about allocation
 * is not. This allocator keeps track of number of allocation and amount of allocated bytes, but it
 * does not track of names of allocated blocks.
 *
 * \note The switch between allocator types can only happen before any allocation did happen.
 */
void MEM_use_lockfree_allocator(void);

/**
 * Switch allocator to slow fully guarded mode.
 *
 * Use for debug purposes. This allocator contains lock section around every allocator call, which
 * makes it slow. What is gained with this is the ability to have list of allocated blocks (in an
 * addition to the tracking of number of allocations and amount of allocated bytes).
 *
 * \note The switch between allocator types can only happen before any allocation did happen.
 */
void MEM_use_guarded_allocator(void);

/** \} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#  include <any>
#  include <memory>
#  include <new>
#  include <type_traits>
#  include <utility>

#  include "intern/mallocn_intern_function_pointers.hh"

/**
 * Conservative value of memory alignment returned by non-aligned OS-level memory allocation
 * functions. For alignments smaller than this value, using non-aligned versions of allocator API
 * functions is okay, allowing use of `calloc`, for example.
 */
#  define MEM_MIN_CPP_ALIGNMENT \
    (__STDCPP_DEFAULT_NEW_ALIGNMENT__ < alignof(void *) ? __STDCPP_DEFAULT_NEW_ALIGNMENT__ : \
                                                          alignof(void *))

/* -------------------------------------------------------------------- */
/**
 * \name Type-aware allocation & construction API.
 *
 * Defines some `new`/`delete`-like helpers, which allocate/free memory using `MEM_guardedalloc`,
 * and construct/destruct the objects.
 *
 * When possible, it is preferred to use these, even on trivial types, as it makes potential
 * future changes to these types less disruptive, and is overall closer to standard C++ data
 * creation and destruction.
 *
 * However, if the type is trivial, `MEM_[cm]allocN<T>` and related functions can be used to
 * allocate an object that will be managed by external historic code still using C-style
 * allocation/duplication/freeing.
 *
 * \{ */

namespace mem_guarded::internal {
/* Note that we intentionally don't care about a non-trivial default constructor here. */
template<typename T>
constexpr bool is_trivial_after_construction = std::is_trivially_copyable_v<T> &&
                                               std::is_trivially_destructible_v<T>;
}  // namespace mem_guarded::internal

/**
 * Allocate new memory for an object of type #T, and construct it.
 * #MEM_delete must be used to delete the object. Calling #MEM_delete_void on it is illegal.
 *
 * Do not assume that this ever zero-initializes memory (even when it does), explicitly initialize.
 *
 * Although calling this without arguments will cause zero-initialization for many types, simple
 * changes to the type can break this. Basic explanation:
 * With no arguments, this will initialize using `T()` (value initialization) not `T` (default
 * initialization). Details are involved, but for "C-style" structs ("Plain old Data" structs or
 * structs with a compiler generated constructor) memory will be zero-initialized. A change like
 * simply adding a custom default constructor would change initialization behavior.
 * See: https://stackoverflow.com/a/4982720, https://stackoverflow.com/a/620402
 */
template<typename T, typename... Args>
inline T *MEM_new(const char *allocation_name, Args &&...args)
{
  void *buffer = mem_guarded::internal::mem_mallocN_aligned_ex(
      sizeof(T),
      alignof(T),
      allocation_name,
      std::is_trivially_destructible_v<T> ? mem_guarded::internal::DestructorType::Trivial :
                                            mem_guarded::internal::DestructorType::NonTrivial);
  return new (buffer) T(std::forward<Args>(args)...);
}

/**
 * Allocate new memory for an array of objects with type #T, and construct them.
 *
 * See #MEM_new for initialization logic. Unlike #MEM_new this is only supported for trivially
 * destructible types. This makes it safe to use #MEM_delete on arrays without the need for an
 * equivalent of the delete [] operator.
 *
 * In new code it is preferred to use data structures like Vector instead, whenever possible.
 */
template<typename T> inline T *MEM_new_array(const size_t length, const char *allocation_name)
{
#  ifdef _MSC_VER
  static_assert(
      std::is_trivially_destructible_v<T>,
      "For non-trivially copyable and destructible types, use higher level types like Vector.");
#  else
  static_assert(
      mem_guarded::internal::is_trivial_after_construction<T>,
      "For non-trivially copyable and destructible types, use higher level types like Vector.");
#  endif
  T *buffer = static_cast<T *>(
      MEM_new_array_uninitialized_aligned(length, sizeof(T), alignof(T), allocation_name));
  for (size_t i = 0; i < length; i++) {
    new (buffer + i) T();
  }
  return buffer;
}

/**
 * Destruct and deallocate an object previously allocated and constructed with #MEM_new, or some
 * type-overloaded `new` operators using MEM_guardedalloc as backend.
 *
 * As with the `delete` C++ operator, passing in `nullptr` is allowed and does nothing.
 *
 * It is illegal to call this function with data allocated by the C-style allocation functions of
 * this module.
 */
template<typename T> inline void MEM_delete(const T *ptr)
{
  static_assert(
      !std::is_void_v<T>,
      "MEM_delete on a void pointer is not possible, `static_cast` it to the correct type");
  if (ptr == nullptr) {
    return;
  }
  const void *complete_ptr = [ptr]() {
    if constexpr (std::is_polymorphic_v<T>) {
      /* Polymorphic objects lifetime can be managed with pointers to their most derived type or
       * with pointers to any of their ancestor types in their hierarchy tree that define a virtual
       * destructor, however ancestor pointers may differ in a offset from the same derived object.
       * For freeing the correct memory allocated with #MEM_new, we need to ensure that the given
       * pointer is equal to the pointer to the most derived object, which can be obtained with
       * `dynamic_cast<void *>(ptr)`. */
      return dynamic_cast<const void *>(ptr);
    }
    else {
      return static_cast<const void *>(ptr);
    }
  }();
  /* Explicitly don't call destructor when not needed, also because it doesn't work
   * for pointers like float (*x)[2]. */
  if constexpr (!std::is_trivially_destructible_v<T>) {
    ptr->~T();
  }
  /* C++ allows destruction of `const` objects, so the pointer is allowed to be `const`. */
  mem_guarded::internal::mem_freeN_ex(const_cast<void *>(complete_ptr),
                                      mem_guarded::internal::DestructorType::NonTrivial);
}

/**
 * Helper shortcut to #MEM_delete, that also ensures that the target pointer is set to nullptr
 * after deleting it.
 */
#  define MEM_SAFE_DELETE(v) \
    do { \
      if (v) { \
        MEM_delete(v); \
        (v) = nullptr; \
      } \
    } while (0)

/**
 * Helper shortcut to #MEM_delete_void, that also ensures that the target pointer is set to nullptr
 * after deleting it.
 */
#  define MEM_SAFE_DELETE_VOID(v) \
    do { \
      if (v) { \
        MEM_delete_void(v); \
        (v) = nullptr; \
      } \
    } while (0)

/** Wrapper for MEM_SAFE_DELETE<() as deallocator for std::unique_ptr. */
template<typename T> struct MEM_smart_ptr_deleter {
  void operator()(T *pointer) const noexcept
  {
    MEM_SAFE_DELETE(pointer);
  }
};

/** Define overloaded new/delete operators for C++ types. */
#  define MEM_CXX_CLASS_ALLOC_FUNCS(_id) \
   public: \
    void *operator new(size_t num_bytes) \
    { \
      return mem_guarded::internal::mem_mallocN_aligned_ex( \
          num_bytes, \
          __STDCPP_DEFAULT_NEW_ALIGNMENT__, \
          _id, \
          mem_guarded::internal::DestructorType::NonTrivial); \
    } \
    void *operator new(size_t num_bytes, std::align_val_t alignment) \
    { \
      return mem_guarded::internal::mem_mallocN_aligned_ex( \
          num_bytes, size_t(alignment), _id, mem_guarded::internal::DestructorType::NonTrivial); \
    } \
    void operator delete(void *mem) \
    { \
      if (mem) { \
        mem_guarded::internal::mem_freeN_ex(mem, \
                                            mem_guarded::internal::DestructorType::NonTrivial); \
      } \
    } \
    void *operator new[](size_t num_bytes) \
    { \
      return mem_guarded::internal::mem_mallocN_aligned_ex( \
          num_bytes, \
          __STDCPP_DEFAULT_NEW_ALIGNMENT__, \
          _id "[]", \
          mem_guarded::internal::DestructorType::NonTrivial); \
    } \
    void *operator new[](size_t num_bytes, std::align_val_t alignment) \
    { \
      return mem_guarded::internal::mem_mallocN_aligned_ex( \
          num_bytes, \
          size_t(alignment), \
          _id "[]", \
          mem_guarded::internal::DestructorType::NonTrivial); \
    } \
    void operator delete[](void *mem) \
    { \
      if (mem) { \
        mem_guarded::internal::mem_freeN_ex(mem, \
                                            mem_guarded::internal::DestructorType::NonTrivial); \
      } \
    } \
    void *operator new(size_t /*count*/, void *ptr) \
    { \
      return ptr; \
    } \
    /** \
     * This is the matching delete operator to the placement-new operator above. \
     * Both parameters \
     * will have the same value. Without this, we get the warning C4291 on windows. \
     */ \
    void operator delete(void * /*ptr_to_free*/, void * /*ptr*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/**
 * \name Type-aware allocation API.
 *
 * Templated, type-safe versions of C-style allocation & freeing API.
 *
 * These functions only allocate or free memory, without any calls to constructors or destructors.
 *
 * \note MSVC considers C-style types using the #DNA_DEFINE_CXX_METHODS as non-trivial (more
 * specifically, non-trivially copyable, likely because the default copy constructors are
 * deleted by this macro). GCC and clang (both on linux, OSX, and clang-cl on Windows on Arm) do
 * not. So for now, `MEM_[cm]allocN<T>` and related templates use slightly more relaxed checks on
 * MSVC. These should still catch most of the real-life invalid cases.
 *
 * \{ */

/**
 * Allocate zero-initialized memory for an object of type #T. The constructor of #T is not called,
 * therefore this must only be used with trivial types (like all C types).
 *
 * When allocating an enforced specific amount of bytes, the C version of this function should be
 * used instead. While this should be avoided in C++ code, it is still required in some cases, e.g.
 * for ID allocation based on #IDTypeInfo::struct_size.
 *
 * #MEM_delete must be used to free a pointer returned by this call. In legacy code,
 * #MEM_delete_void may also be used if the type is trivially destructible.
 */
template<typename T> inline T *MEM_new_zeroed(const char *allocation_name)
{
#  ifdef _MSC_VER
  static_assert(std::is_trivially_constructible_v<T>,
                "For non-trivial types, MEM_new must be used.");
#  else
  static_assert(std::is_trivial_v<T>, "For non-trivial types, MEM_new must be used.");
#  endif
  return static_cast<T *>(MEM_new_array_zeroed_aligned(1, sizeof(T), alignof(T), allocation_name));
}

/**
 * Type-safe version of #MEM_new_array_zeroed/#MEM_new_array_zeroed_aligned.
 *
 * It has the same restrictions and limitations as the type-safe version of #MEM_new_zeroed<T>.
 */
template<typename T>
inline T *MEM_new_array_zeroed(const size_t length, const char *allocation_name)
{
#  ifdef _MSC_VER
  static_assert(std::is_trivially_constructible_v<T>,
                "For non-trivial types, MEM_new must be used.");
#  else
  static_assert(std::is_trivial_v<T>, "For non-trivial types, MEM_new must be used.");
#  endif
  return static_cast<T *>(
      MEM_new_array_zeroed_aligned(length, sizeof(T), alignof(T), allocation_name));
}

/**
 * Allocate uninitialized memory for an object of type #T. The constructor of #T is not called,
 * therefore this must only be used with trivial types (like all C types).
 *
 * When allocating an enforced specific amount of bytes, the C version of this function should be
 * used instead. While this should be avoided in C++ code, it is still required in some cases, e.g.
 * for ID allocation based on #IDTypeInfo::struct_size.
 *
 * #MEM_delete_void must be used to free a pointer returned by this call. Calling #MEM_delete on it
 * is illegal.
 */
template<typename T> inline T *MEM_new_uninitialized(const char *allocation_name)
{
#  ifdef _MSC_VER
  static_assert(std::is_trivially_constructible_v<T>,
                "For non-trivial types, MEM_new must be used.");
#  else
  static_assert(std::is_trivial_v<T>, "For non-trivial types, MEM_new must be used.");
#  endif
  return static_cast<T *>(
      MEM_new_array_uninitialized_aligned(1, sizeof(T), alignof(T), allocation_name));
}

/**
 * Type-safe version of #MEM_new_array_uninitialized/#MEM_new_uninitialized_aligned.
 *
 * It has the same restrictions and limitations as the type-safe version of
 * #MEM_new_uninitialized<T>.
 */
template<typename T>
inline T *MEM_new_array_uninitialized(const size_t length, const char *allocation_name)
{
#  ifdef _MSC_VER
  static_assert(std::is_trivially_constructible_v<T>,
                "For non-trivial types, MEM_new must be used.");
#  else
  static_assert(std::is_trivial_v<T>, "For non-trivial types, MEM_new must be used.");
#  endif
  return static_cast<T *>(
      MEM_new_array_uninitialized_aligned(length, sizeof(T), alignof(T), allocation_name));
}

/**
 * Duplicates a block of memory, and returns a pointer to the newly allocated block.
 * NULL-safe; will return NULL when receiving a NULL pointer.
 *
 * Only supported for trivially copyable types, use #MEM_new for other types.
 * */
template<typename T> inline T *MEM_dupalloc(const T *other)
{
#  ifdef _MSC_VER
  /* TODO: Add back is_trivially_copyable_v condition, temporarily disabled
   * because of build error on MSVC. */
  static_assert(/*std::is_trivially_copyable_v<T> &&*/ std::is_trivially_destructible_v<T>,
                "MEM_dupalloc can only duplicate types that are trivially copyable and "
                "destructible, use MEM_new instead.");
#  else
  static_assert(std::is_trivially_copyable_v<T> &&
                    mem_guarded::internal::is_trivial_after_construction<T>,
                "MEM_dupalloc can only duplicate types that are trivially copyable and "
                "destructible, use MEM_new instead.");
#  endif

  static_assert(!std::is_void_v<T>);

  return static_cast<T *>(MEM_dupalloc_void(other));
}

template<typename T> inline void MEM_delete_void(T *ptr)
{
  static_assert(std::is_void_v<T>,
                "MEM_delete_void only supported for void pointer, use MEM_delete instead");
  mem_guarded::internal::mem_freeN_ex(const_cast<void *>(static_cast<const void *>(ptr)),
                                      mem_guarded::internal::DestructorType::Trivial);
}

/** \} */

/**
 * Construct a T that will only be destructed after leak detection is run.
 *
 * This call is thread-safe. Calling code should typically keep a reference to that data as a
 * `static thread_local` variable, or use some lock, to prevent concurrent accesses.
 *
 * The returned value should not own any memory allocated with `MEM_*` functions, since these would
 * then be detected as leaked.
 */
template<typename T, typename... Args> T &MEM_construct_leak_detection_data(Args &&...args)
{
  std::shared_ptr<T> data = std::make_shared<T>(std::forward<Args>(args)...);
  std::any any_data = std::make_any<std::shared_ptr<T>>(data);
  mem_guarded::internal::add_memleak_data(any_data);
  return *data;
}

#endif /* __cplusplus */

#endif /* __MEM_GUARDEDALLOC_H__ */
