/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "ED_asset_indexer.hh"

#include <memory>
#include <variant>

namespace blender {
struct AssetMetaData;
class StringRefNull;
namespace io::serialize {
class DictionaryValue;
class Value;
}  // namespace io::serialize
}  // namespace blender

namespace blender::ed::asset::index {

struct RemoteListingAssetEntry;

std::unique_ptr<io::serialize::Value> read_contents(StringRefNull filepath);

AssetMetaData *asset_metadata_from_dictionary(const io::serialize::DictionaryValue &entry);

/**
 * Result of reading a remote listing.
 *
 * Can be in any of these three states:
 *
 * type=Success:   has a 'success value' of type `T`, and a vector of warnings
 *                 (strings; may be empty).
 *
 * type=Failure:   has a 'failure message' (may be empty, but for good UX better
 *                 to always use).
 *
 * type=Cancelled: has no extra info, because this was in response to a user
 *                 cancelling an operation (and so this happening should be
 *                 expected).
 */
template<typename T = std::monostate> class ReadingResult {
 public:
  enum class Type {
    Success,
    Failure,
    Cancelled,
  };
  Type type;
  std::string failure_reason;
  std::optional<T> success_value;

  /**
   * Even when an operation was performed succesfully, there could have been
   * warnings. These are only intended to be used on success status; on failure,
   * only `failure_reason` is expected to be set. On cancellation, no reason
   * needs to be given (as it is in response to the user cancelling the
   * operation).
   *
   * \see ReadingResult::append_warning()
   */
  Vector<std::string> warnings;

  /**
   * Construct a valueless success result.
   * Only enabled if T == std::monostate.
   */
  template<typename U = T>
  static ReadingResult Success()
    requires(std::is_same_v<U, std::monostate>)
  {
    return ReadingResult(Type::Success);
  }

  /**
   * Construct a valued success result.
   * Only enabled if T != std::monostate.
   */
  template<typename U = T>
  static ReadingResult Success(T value)
    requires(!std::is_same_v<U, std::monostate>)
  {
    ReadingResult result(Type::Success);
    result.success_value = std::move(value);
    return result;
  }

  /**
   * Construct a failure result.
   * The ReadingResult copies the failure reason, so the StringRef can refer to temporary data.
   *
   * NOTE: Don't forget to wrap the string in N_(...) for translation tagging.
   */
  static ReadingResult Failure(const StringRef failure_reason)
  {
    ReadingResult result(Type::Failure);
    result.failure_reason = failure_reason;
    return result;
  }
  /**
   * Construct a cancelled result.
   *
   * Callback functions passed to `index::read_remote_listing()` can return
   * `false` to indicate the loading should be cancelled.
   */
  static ReadingResult Cancelled()
  {
    return ReadingResult(Type::Cancelled);
  }

  /**
   * Construct a ReadingResult with the given type.
   *
   * NOTE: Do not use this function, use one of the above functions instead. It's public only
   * because it's needed in internal code of this class, but across differently-templated versions
   * of this class (which C++ considers to be unrelated, and thus cannot access each other's
   * private members).
   */
  explicit ReadingResult(Type type) : type(type) {}

  /** Return whether this result indicates a success. */
  bool is_success() const
  {
    return this->type == Type::Success;
  }
  /** Return whether this result indicates a failure. */
  bool is_failure() const
  {
    return this->type == Type::Failure;
  }
  /** Return whether this result indicates cancellation. */
  bool is_cancelled() const
  {
    return this->type == Type::Cancelled;
  }

  bool has_warnings() const
  {
    return !this->warnings.is_empty();
  }

  /**
   * Move the warnings from another result into this one.
   */
  template<typename U> void move_warnings_from(ReadingResult<U> &other)
  {
    BLI_assert_msg(is_success(), "Attempted to move warnings into a non-success ReadingResult");
    for (std::string &warning : other.warnings) {
      this->warnings.append(std::move(warning));
    }
  }

  /**
   * Return this ReadingResult, but without its success value.
   *
   * The result type, failure message, and warnings are copied.
   */
  ReadingResult<> without_success_value() const
  {
    ReadingResult<> without_value(static_cast<ReadingResult<>::Type>(this->type));
    without_value.success_value.reset();
    without_value.failure_reason = this->failure_reason;
    without_value.warnings.extend(this->warnings);
    return without_value;
  }

  /**
   * Get a reference to the result's success value, similar to `std::optional<T>`.
   * Only valid if this result is successful and there is an actual success value.
   */
  template<typename U = T>
  T &operator*()
    requires(!std::is_same_v<U, std::monostate>)
  {
    BLI_assert_msg(is_success() || !success_value.has_value(),
                   "Attempted to access value of non-success ReadingResult");
    return *success_value;
  }

  /**
   * Get a reference to the result's success value, similar to `std::optional<T>`.
   * Only valid if this result is successful and there is an actual success value.
   */
  template<typename U = T>
  const T &operator*() const
    requires(!std::is_same_v<U, std::monostate>)
  {
    BLI_assert_msg(is_success() || !success_value.has_value(),
                   "Attempted to access value of non-success ReadingResult");
    return *success_value;
  }

  /**
   * Get a pointer to the result's success value, similar to `std::optional<T>`.
   * Only valid if this result is successful and there is an actual success value.
   */
  template<typename U = T>
  T *operator->()
    requires(!std::is_same_v<U, std::monostate>)
  {
    T &success_value = **this;
    return &success_value;
  }

  /**
   * Get a pointer to the result's success value, similar to `std::optional<T>`.
   * Only valid if this result is successful and there is an actual success value.
   */
  template<typename U = T>
  const T *operator->() const
    requires(!std::is_same_v<U, std::monostate>)
  {
    const T &success_value = **this;
    return &success_value;
  }

  /**
   * Conversion constructor from any other ReadingResult.
   */
  template<typename U> ReadingResult(const ReadingResult<U> &other)
  {
    this->type = static_cast<ReadingResult<T>::Type>(other.type);
    if (this->type == Type::Success) {
      // Only allow if U is std::monostate.
      static_assert(std::is_same<U, std::monostate>::value,
                    "Cannot convert a valued success to another type");
      success_value.reset();
    }
    else {
      // Failure or Cancelled can convert freely.
      failure_reason = other.failure_reason;
    }
  }
};

std::optional<bool> file_older_than_timestamp(const char *filepath, Timestamp timestamp);

/**
 * Reading of API schema version 1. See #read_remote_listing() on \a process_fn.
 * \param version_root_dirpath: Absolute path to the remote listing root directory.
 */
ReadingResult<> read_remote_listing_v1(
    StringRefNull listing_root_dirpath,
    RemoteListingEntryProcessFn process_fn,
    RemoteListingWaitForPagesFn wait_fn = nullptr,
    const std::optional<Timestamp> ignore_before_timestamp = std::nullopt);

}  // namespace blender::ed::asset::index
