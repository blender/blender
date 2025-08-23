/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * 
 * RAII (Resource Acquisition Is Initialization) utilities for automatic
 * resource management in Blender. These wrappers ensure proper cleanup
 * of resources when they go out of scope, preventing memory leaks and
 * resource leaks.
 */

#pragma once

#include <memory>
#include <functional>
#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

namespace blender {

/**
 * Smart pointer for Blender's MEM_* allocated memory.
 * Automatically calls MEM_freeN when going out of scope.
 */
template<typename T>
class mem_ptr {
 private:
  T* ptr_ = nullptr;

 public:
  mem_ptr() = default;
  explicit mem_ptr(T* ptr) : ptr_(ptr) {}
  
  ~mem_ptr() {
    if (ptr_) {
      MEM_freeN(ptr_);
    }
  }
  
  // Delete copy constructor and assignment
  mem_ptr(const mem_ptr&) = delete;
  mem_ptr& operator=(const mem_ptr&) = delete;
  
  // Move constructor and assignment
  mem_ptr(mem_ptr&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }
  
  mem_ptr& operator=(mem_ptr&& other) noexcept {
    if (this != &other) {
      if (ptr_) {
        MEM_freeN(ptr_);
      }
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }
  
  T* get() const { return ptr_; }
  T* release() {
    T* tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
  }
  
  void reset(T* ptr = nullptr) {
    if (ptr_) {
      MEM_freeN(ptr_);
    }
    ptr_ = ptr;
  }
  
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }
};

/**
 * Create a MEM_* allocated object with automatic cleanup.
 */
template<typename T, typename... Args>
mem_ptr<T> make_mem(Args&&... args) {
  T* ptr = static_cast<T*>(MEM_mallocN(sizeof(T), __func__));
  if (ptr) {
    new (ptr) T(std::forward<Args>(args)...);
  }
  return mem_ptr<T>(ptr);
}

/**
 * RAII guard for generic cleanup operations.
 * Executes a cleanup function when going out of scope.
 */
class scope_guard {
 private:
  std::function<void()> cleanup_;
  bool active_ = true;

 public:
  explicit scope_guard(std::function<void()> cleanup) 
    : cleanup_(std::move(cleanup)) {}
  
  ~scope_guard() {
    if (active_ && cleanup_) {
      cleanup_();
    }
  }
  
  // Delete copy
  scope_guard(const scope_guard&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;
  
  // Move constructor
  scope_guard(scope_guard&& other) noexcept 
    : cleanup_(std::move(other.cleanup_)), active_(other.active_) {
    other.active_ = false;
  }
  
  void dismiss() { active_ = false; }
};

/**
 * Helper macro to create a scope guard with automatic naming.
 */
#define BLI_SCOPE_EXIT(...) \
  ::blender::scope_guard BLI_CONCAT(scope_guard_, __LINE__)(__VA_ARGS__)

/**
 * RAII wrapper for ListBase cleanup.
 */
class list_guard {
 private:
  ListBase* list_;
  bool owns_;

 public:
  explicit list_guard(ListBase* list, bool take_ownership = true)
    : list_(list), owns_(take_ownership) {}
  
  ~list_guard() {
    if (owns_ && list_) {
      BLI_freelistN(list_);
    }
  }
  
  // Delete copy
  list_guard(const list_guard&) = delete;
  list_guard& operator=(const list_guard&) = delete;
  
  // Move constructor
  list_guard(list_guard&& other) noexcept 
    : list_(other.list_), owns_(other.owns_) {
    other.owns_ = false;
  }
  
  ListBase* get() { return list_; }
  ListBase* release() {
    owns_ = false;
    return list_;
  }
  
  void dismiss() { owns_ = false; }
};

/* Note: curve_cache_guard removed - requires full CurveCache definition
 * which would create circular dependencies. Use scope_guard instead
 * for cleanup of complex structures. */

}  // namespace blender