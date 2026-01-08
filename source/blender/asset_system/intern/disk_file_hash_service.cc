/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <optional>

#include "AS_disk_file_hash_service.hh"

#include "BKE_idprop.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

#include "CLG_log.h"

static CLG_LogRef LOG = {"assets.disk_file_hash_service"};

namespace blender::asset_system {

std::unique_ptr<DiskFileHashService> disk_file_hash_service_get(const StringRef storage_path)
{
  return std::make_unique<DiskFileHashService>(storage_path);
}

DiskFileHashService::DiskFileHashService(const StringRef storage_path)
    : storage_path_(storage_path)
{
}

std::string DiskFileHashService::get_hash(bContext &C,
                                          const StringRef filepath,
                                          const StringRef hash_algorithm)
{
#ifdef WITH_PYTHON
  /* NOTE: this is a somewhat inefficient implementation for frequently-repeated calls, as each
   * call repeats the calls to `dfhs.get_service(Path(...))`. However, this does mean that the C++
   * wrapper does not have to retain any references to Python objects itself, avoiding reference
   * counting bugs. If the performance starts to matter, do the lookup of the service itself once,
   * and cache the result. */
  constexpr const char *SCRIPT = R"(
import _bpy_internal.disk_file_hash_service as dfhs
from pathlib import Path

service = dfhs.get_service(Path(storage_path))
_result = service.get_hash(Path(filepath), hash_algorithm)
)";

  /* Local variables for the script. */
  std::unique_ptr locals = bke::idprop::create_group("locals");
  IDP_AddToGroup(locals.get(), IDP_NewString(this->storage_path_, "storage_path"));
  IDP_AddToGroup(locals.get(), IDP_NewString(filepath, "filepath"));
  IDP_AddToGroup(locals.get(), IDP_NewString(hash_algorithm, "hash_algorithm"));

  /* Run the script.*/
  std::optional<IDProperty *> idprop_optptr = BPY_run_string_exec_with_locals_return_idprop(
      &C, SCRIPT, *locals, "_result");
  BLI_assert(idprop_optptr.has_value());
  IDProperty *hash_idprop = *idprop_optptr;

  /* Check the returned value. */
  if (hash_idprop->type != IDP_STRING) {
    IDP_FreeProperty(hash_idprop);
    const std::string filepath_str = filepath;
    CLOG_ERROR(&LOG,
               "Hash for file [%s] was not returned as string. Please report this as a bug.",
               filepath_str.c_str());
    return "";
  }

  const std::string hash_value(IDP_string_get(hash_idprop));
  IDP_FreeProperty(hash_idprop);

  return hash_value;
#else
  UNUSED_VARS(C, hash_algorithm);
  const std::string filepath_str = filepath;
  CLOG_ERROR(&LOG,
             "Blender was built without Python support, cannot compute hash for file [%s]",
             filepath_str.c_str());
  return "";
#endif
}

bool DiskFileHashService::file_matches(bContext &C,
                                       const StringRef filepath,
                                       const StringRef hash_algorithm,
                                       const StringRef hexhash,
                                       const int64_t size_in_bytes)
{
#ifdef WITH_PYTHON
  /* NOTE: this is a somewhat inefficient implementation for frequently-repeated calls, as each
   * call repeats the calls to `dfhs.get_service(Path(...))`. However, this does mean that the C++
   * wrapper does not have to retain any references to Python objects itself, avoiding reference
   * counting bugs. If the performance starts to matter, do the lookup of the service itself once,
   * and cache the result. */
  constexpr const char *SCRIPT = R"(
import _bpy_internal.disk_file_hash_service as dfhs
from pathlib import Path

# The '& 0xFFFFFFFF' makes Python interpret the values as unsigned ints.
size_in_bytes = ((size_in_bytes_high & 0xFFFFFFFF) << 32) | (size_in_bytes_low & 0xFFFFFFFF)

service = dfhs.get_service(Path(storage_path))
_result = service.file_matches(Path(filepath), hash_algorithm, hexhash, size_in_bytes);
)";

  /* Since IDProperties don't support 64-bit integers, split it up into two 32-bit integers, and do
   * bit shifting in Python to get the value back. */
  BLI_assert(size_in_bytes >= 0);
  const int size_in_bytes_high = int((size_in_bytes >> 32) & 0xFFFFFFFF);
  const int size_in_bytes_low = int(size_in_bytes & 0xFFFFFFFF);

  std::unique_ptr locals = bke::idprop::create_group("locals");
  IDP_AddToGroup(locals.get(), IDP_NewString(this->storage_path_, "storage_path"));
  IDP_AddToGroup(locals.get(), IDP_NewString(filepath, "filepath"));
  IDP_AddToGroup(locals.get(), IDP_NewString(hash_algorithm, "hash_algorithm"));
  IDP_AddToGroup(locals.get(), IDP_NewString(hexhash, "hexhash"));
  IDP_AddToGroup(locals.get(), IDP_NewInt(size_in_bytes_high, "size_in_bytes_high"));
  IDP_AddToGroup(locals.get(), IDP_NewInt(size_in_bytes_low, "size_in_bytes_low"));

  /* Run the script.*/
  std::optional<IDProperty *> idprop_optptr = BPY_run_string_exec_with_locals_return_idprop(
      &C, SCRIPT, *locals, "_result");
  BLI_assert(idprop_optptr.has_value());
  IDProperty *is_match_idprop = *idprop_optptr;

  /* Check the returned value. */
  if (is_match_idprop->type != IDP_BOOLEAN) {
    IDP_FreeProperty(is_match_idprop);
    const std::string filepath_str = filepath;
    CLOG_ERROR(
        &LOG,
        "Hash match check for file [%s] did not return a boolean. Please report this as a bug.",
        filepath_str.c_str());
    return false;
  }

  const bool is_match(IDP_bool_get(is_match_idprop));
  IDP_FreeProperty(is_match_idprop);

  return is_match;
#else
  UNUSED_VARS(C, hash_algorithm, hexhash, size_in_bytes);
  const std::string filepath_str = filepath;
  CLOG_ERROR(&LOG,
             "Blender was built without Python support, cannot check hash for file [%s]",
             filepath_str.c_str());
  return false;
#endif
}

}  // namespace blender::asset_system
