/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_utildefines.h"
#include "BLI_utildefines_variadic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Path Queries
 * \{ */

/**
 * Get an element of the path at an index, eg:
 * `/some/path/file.txt` where an index of:
 * - 0 or -3: `some`
 * - 1 or -2: `path`
 * - 2 or -1: `file.txt`
 *
 * Ignored elements in the path:
 * - Multiple slashes at any point in the path (including start/end).
 * - Single '.' in the path: `/./` except for the beginning of the path
 *   where it's used to signify a $PWD relative path.
 */
bool BLI_path_name_at_index(const char *__restrict path,
                            int index,
                            int *__restrict r_offset,
                            int *__restrict r_len) ATTR_NONNULL(1, 3, 4) ATTR_WARN_UNUSED_RESULT;

/**
 * Return true if the path is a UNC share.
 */
bool BLI_path_is_unc(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

bool BLI_path_is_win32_drive(const char *path);
bool BLI_path_is_win32_drive_only(const char *path);
bool BLI_path_is_win32_drive_with_slash(const char *path);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Parent Operations
 * \{ */

/**
 * Go back one directory.
 *
 * Replaces path with the path of its parent directory, returning true if
 * it was able to find a parent directory within the path.
 *
 * On success, the resulting path will always have a trailing slash.
 */
bool BLI_path_parent_dir(char *path) ATTR_NONNULL(1);
/**
 * Go back until the directory is found.
 *
 * Strips off nonexistent (or non-accessible) sub-directories from the end of `dir`,
 * leaving the path of the lowest-level directory that does exist and we can read.
 */
bool BLI_path_parent_dir_until_exists(char *path) ATTR_NONNULL(1);

/**
 * In the simple case this is similar to `BLI_path_slash_rfind(dirname)`
 * however it behaves differently when there are redundant characters:
 *
 * `/test///dir/./file`
 *             ^
 * `/test/dir/subdir//file`
 *                  ^
 * \return The position after the parent paths last character or NULL on failure.
 * Neither `path` or `&path[path_len - 1]` are ever returned.
 */
const char *BLI_path_parent_dir_end(const char *path, size_t path_len)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Make Safe / Sanitize
 * \{ */

/**
 * Make given name safe to be used in paths.
 *
 * \param allow_tokens: Permit the usage of '<' and '>' characters. This can be
 * leveraged by higher layers to support "virtual filenames" which contain
 * substitution markers delineated between the two characters.
 *
 * \return true if \a filename was changed, false otherwise.
 *
 * For now, simply replaces reserved chars (as listed in
 * https://en.wikipedia.org/wiki/Filename#Reserved_characters_and_words )
 * by underscores ('_').
 *
 * \note Space case ' ' is a bit of an edge case here - in theory it is allowed,
 * but again can be an issue in some cases, so we simply replace it by an underscore too
 * (good practice anyway).
 * REMOVED based on popular demand (see #45900).
 * Percent '%' char is a bit same case - not recommended to use it,
 * but supported by all decent file-systems/operating-systems around.
 *
 * \note On Windows, it also ensures there is no '.' (dot char) at the end of the file,
 * this can lead to issues.
 *
 * \note On Windows, it also checks for forbidden names
 * (see https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx ).
 */
bool BLI_path_make_safe_filename_ex(char *filename, bool allow_tokens) ATTR_NONNULL(1);
bool BLI_path_make_safe_filename(char *filename) ATTR_NONNULL(1);

/**
 * Make given path OS-safe.
 *
 * \return true if \a path was changed, false otherwise.
 */
bool BLI_path_make_safe(char *path) ATTR_NONNULL(1);

/**
 * Creates a display string from path to be used menus and the user interface.
 * Like `bpy.path.display_name()`.
 */
void BLI_path_to_display_name(char *display_name, int display_name_maxncpy, const char *name)
    ATTR_NONNULL(1, 3);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Normalize
 * \{ */

/**
 * Remove redundant characters from \a path.
 *
 * The following operations are performed:
 * - Redundant path components such as `//`, `/./` & `./` (prefix) are stripped.
 *   (with the exception of `//` prefix used for blend-file relative paths).
 * - `..` are resolved so `<parent>/../<child>/` resolves to `<child>/`.
 *   Note that the resulting path may begin with `..` if it's relative.
 *
 * Details:
 * - The slash direction is expected to be native (see #SEP).
 *   When calculating a canonical paths you may need to run #BLI_path_slash_native first.
 *   #BLI_path_cmp_normalized can be used for canonical path comparison.
 * - Trailing slashes are left intact (unlike Python which strips them).
 * - Handling paths beginning with `..` depends on them being absolute or relative.
 *   For absolute paths they are removed (e.g. `/../path` becomes `/path`).
 *   For relative paths they are kept as it's valid to reference paths above a relative location
 *   such as `//../parent` or `../parent`.
 *
 * \param path: The path to a file or directory which can be absolute or relative.
 * \return the length of `path`.
 */
int BLI_path_normalize(char *path) ATTR_NONNULL(1);

/**
 * A version of #BLI_path_normalize without special handling of `//` blend file relative prefix.
 *
 * \note On UNIX `//path` is a valid path which gets normalized to `/path`.
 *
 * \return the length of `path`.
 */
int BLI_path_normalize_native(char *path) ATTR_NONNULL(1);

/**
 * Cleanup file-path ensuring a trailing slash.
 *
 * \note Same as #BLI_path_normalize but adds a trailing slash.
 *
 * \return the length of `dir`.
 */
int BLI_path_normalize_dir(char *dir, size_t dir_maxncpy) ATTR_NONNULL(1);

#if defined(WIN32)
void BLI_path_normalize_unc_16(wchar_t *path_16);
void BLI_path_normalize_unc(char *path, int path_maxncpy);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Canonicalize
 * \{ */

/**
 * Convert `path` to a canonical representation.
 * This is intended for system paths (passed in as command-line arguments of via scripts)
 * which are valid in that they resolve to a file/directory and but could be `CWD` relative or
 * contain redundant slashes that cause absolute/relative conversion to fail.
 * (specifically the "//" prefix used by Blender).
 *
 * Perform the following operations:
 *
 * - Make absolute (relative to the current working directory).
 * - Convert slash direction (WIN32 only, as other systems may use back-slashes in filenames).
 * - Normalize redundant slashes.
 * - Strip trailing slashes.
 */
int BLI_path_canonicalize_native(char *path, int path_maxncpy);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path FileName Manipulation
 * \{ */

/**
 * Ensure `filepath` has a file component, adding `filename` when it's empty or ends with a slash.
 * \return true if the `filename` was appended to `filepath`.
 */
bool BLI_path_filename_ensure(char *filepath, size_t filepath_maxncpy, const char *filename)
    ATTR_NONNULL(1, 3);

/**
 * Appends a suffix to the `path`, fitting it before the extension
 *
 * path = `Foo.png`, suffix = `123`, separator = `_`.
 * `Foo.png` -> `Foo_123.png`.
 *
 * \param path: original (and final) string.
 * \param path_maxncpy: Maximum length of path.
 * \param suffix: String to append to the original path.
 * \param sep: Optional separator character.
 * \return true if succeeded.
 */
bool BLI_path_suffix(char *path, size_t path_maxncpy, const char *suffix, const char *sep)
    ATTR_NONNULL(1, 3, 4);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Slash Utilities
 * \{ */

/**
 * \return pointer to the leftmost path separator in path (or NULL when not found).
 */
const char *BLI_path_slash_find(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
/**
 * \return pointer to the rightmost path separator in path (or NULL when not found).
 */
const char *BLI_path_slash_rfind(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
/**
 * Appends a slash to path if there isn't one there already.
 * \param path_len: The length of `path`.
 * \return the new length of the path.
 */
int BLI_path_slash_ensure_ex(char *path, size_t path_maxncpy, const size_t path_len)
    ATTR_NONNULL(1);
/**
 * Appends a slash to path if there isn't one there already.
 * \return the new length of the path.
 */
int BLI_path_slash_ensure(char *path, size_t path_maxncpy) ATTR_NONNULL(1);
/**
 * Removes the last slash and everything after it to the end of path, if there is one.
 */
void BLI_path_slash_rstrip(char *path) ATTR_NONNULL(1);
/**
 * Changes to the path separators to the native ones for this OS.
 */
void BLI_path_slash_native(char *path) ATTR_NONNULL(1);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Directory/FileName Split
 * \{ */

/**
 * Copies directory and file components from `filepath` into `dir` and `file`, e.g.
 * `/foo/bar.txt` to `/foo/` and `bar.txt`
 */
void BLI_path_split_dir_file(const char *filepath,
                             char *dir,
                             size_t dir_maxncpy,
                             char *file,
                             size_t file_maxncpy) ATTR_NONNULL(1, 2, 4);
/**
 * Copies the parent directory part of `filepath` into `dir`, max length `dir_maxncpy`.
 */
void BLI_path_split_dir_part(const char *filepath, char *dir, size_t dir_maxncpy)
    ATTR_NONNULL(1, 2);
/**
 * Copies the leaf filename part of `filepath` into `file`, max length `file_maxncpy`.
 *
 * \note If there is no need to make a copy the path, #BLI_path_basename can be used instead.
 */
void BLI_path_split_file_part(const char *filepath, char *file, size_t file_maxncpy)
    ATTR_NONNULL(1, 2);

/**
 * Like Python's `os.path.basename()`
 *
 * \return The pointer into \a path string immediately after last slash,
 * or start of \a path if none found.
 */
const char *BLI_path_basename(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Append
 * \{ */

/**
 * Append a filename to a dir, ensuring slash separates.
 * \return The new length of `dst`.
 */
size_t BLI_path_append(char *__restrict dst, size_t dst_maxncpy, const char *__restrict file)
    ATTR_NONNULL(1, 3);
/**
 * A version of #BLI_path_append that ensures a trailing slash if there is space in `dst`.
 * \return The new length of `dst`.
 */
size_t BLI_path_append_dir(char *__restrict dst, size_t dst_maxncpy, const char *__restrict dir)
    ATTR_NONNULL(1, 3);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Join
 * \{ */

/**
 * See #BLI_path_join doc-string.
 */
size_t BLI_path_join_array(char *__restrict dst,
                           const size_t dst_maxncpy,
                           const char *path_array[],
                           const int path_array_num) ATTR_NONNULL(1, 3);

/**
 * Join multiple strings into a path, ensuring only a single path separator between each,
 * and trailing slash is kept.
 *
 * \param path: The first patch which has special treatment,
 * allowing `//` prefix which is kept intact unlike double-slashes which are stripped
 * from the bounds of all other paths passed in.
 * Passing in the following paths all result in the same output (`//a/b/c`):
 * - `"//", "a", "b", "c"`.
 * - `"//", "/a/", "/b/", "/c"`.
 * - `"//a", "b/c"`.
 *
 * \note If you want a trailing slash, add `SEP_STR` as the last path argument,
 * duplicate slashes will be cleaned up.
 */
#define BLI_path_join(...) VA_NARGS_CALL_OVERLOAD(_BLI_path_join_, __VA_ARGS__)

#define _BLI_PATH_JOIN_ARGS_1 char *__restrict dst, size_t dst_maxncpy, const char *a
#define _BLI_PATH_JOIN_ARGS_2 _BLI_PATH_JOIN_ARGS_1, const char *b
#define _BLI_PATH_JOIN_ARGS_3 _BLI_PATH_JOIN_ARGS_2, const char *c
#define _BLI_PATH_JOIN_ARGS_4 _BLI_PATH_JOIN_ARGS_3, const char *d
#define _BLI_PATH_JOIN_ARGS_5 _BLI_PATH_JOIN_ARGS_4, const char *e
#define _BLI_PATH_JOIN_ARGS_6 _BLI_PATH_JOIN_ARGS_5, const char *f
#define _BLI_PATH_JOIN_ARGS_7 _BLI_PATH_JOIN_ARGS_6, const char *g
#define _BLI_PATH_JOIN_ARGS_8 _BLI_PATH_JOIN_ARGS_7, const char *h
#define _BLI_PATH_JOIN_ARGS_9 _BLI_PATH_JOIN_ARGS_8, const char *i
#define _BLI_PATH_JOIN_ARGS_10 _BLI_PATH_JOIN_ARGS_9, const char *j

BLI_INLINE size_t _BLI_path_join_3(_BLI_PATH_JOIN_ARGS_1) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_4(_BLI_PATH_JOIN_ARGS_2) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_5(_BLI_PATH_JOIN_ARGS_3) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_6(_BLI_PATH_JOIN_ARGS_4) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_7(_BLI_PATH_JOIN_ARGS_5) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_8(_BLI_PATH_JOIN_ARGS_6) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_9(_BLI_PATH_JOIN_ARGS_7) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_10(_BLI_PATH_JOIN_ARGS_8) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_11(_BLI_PATH_JOIN_ARGS_9) ATTR_NONNULL();
BLI_INLINE size_t _BLI_path_join_12(_BLI_PATH_JOIN_ARGS_10) ATTR_NONNULL();

BLI_INLINE size_t _BLI_path_join_3(_BLI_PATH_JOIN_ARGS_1)
{
  const char *path_array[] = {a};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_4(_BLI_PATH_JOIN_ARGS_2)
{
  const char *path_array[] = {a, b};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_5(_BLI_PATH_JOIN_ARGS_3)
{
  const char *path_array[] = {a, b, c};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_6(_BLI_PATH_JOIN_ARGS_4)
{
  const char *path_array[] = {a, b, c, d};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_7(_BLI_PATH_JOIN_ARGS_5)
{
  const char *path_array[] = {a, b, c, d, e};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_8(_BLI_PATH_JOIN_ARGS_6)
{
  const char *path_array[] = {a, b, c, d, e, f};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_9(_BLI_PATH_JOIN_ARGS_7)
{
  const char *path_array[] = {a, b, c, d, e, f, g};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_10(_BLI_PATH_JOIN_ARGS_8)
{
  const char *path_array[] = {a, b, c, d, e, f, g, h};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_11(_BLI_PATH_JOIN_ARGS_9)
{
  const char *path_array[] = {a, b, c, d, e, f, g, h, i};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}
BLI_INLINE size_t _BLI_path_join_12(_BLI_PATH_JOIN_ARGS_10)
{
  const char *path_array[] = {a, b, c, d, e, f, g, h, i, j};
  return BLI_path_join_array(dst, dst_maxncpy, path_array, ARRAY_SIZE(path_array));
}

#undef _BLI_PATH_JOIN_ARGS_1
#undef _BLI_PATH_JOIN_ARGS_2
#undef _BLI_PATH_JOIN_ARGS_3
#undef _BLI_PATH_JOIN_ARGS_4
#undef _BLI_PATH_JOIN_ARGS_5
#undef _BLI_PATH_JOIN_ARGS_6
#undef _BLI_PATH_JOIN_ARGS_7
#undef _BLI_PATH_JOIN_ARGS_8
#undef _BLI_PATH_JOIN_ARGS_9
#undef _BLI_PATH_JOIN_ARGS_10

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path File Extensions
 * \{ */

/**
 * Returns a pointer to the last extension (e.g. the position of the last period).
 * Returns a pointer to the nil byte when no extension is found.
 */
const char *BLI_path_extension_or_end(const char *filepath)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
/**
 * Returns a pointer to the last extension (e.g. the position of the last period).
 * Returns NULL if there is no extension.
 */
const char *BLI_path_extension(const char *filepath) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/**
 * \return true when `path` end with `ext` (case insensitive).
 */
bool BLI_path_extension_check(const char *path, const char *ext)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
bool BLI_path_extension_check_n(const char *path, ...) ATTR_NONNULL(1) ATTR_SENTINEL(0);
/**
 * \return true when `path` ends with any of the suffixes in `ext_array`.
 */
bool BLI_path_extension_check_array(const char *path, const char **ext_array)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
/**
 * Semicolon separated wildcards, eg: `*.zip;*.py;*.exe`
 * does `path` match any of the semicolon-separated glob patterns in #fnmatch.
 */
bool BLI_path_extension_check_glob(const char *path, const char *ext_fnmatch)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;
/**
 * Does basic validation of the given glob string, to prevent common issues from string
 * truncation.
 *
 * For now, only forbids last group to be a wildcard-only one, if there are more than one group
 * (i.e. things like `*.txt;*.cpp;*` are changed to `*.txt;*.cpp;`)
 *
 * \returns true if it had to modify given \a ext_fnmatch pattern.
 */
bool BLI_path_extension_glob_validate(char *ext_fnmatch) ATTR_NONNULL(1);
/**
 * Removes any existing extension on the end of \a path and appends \a ext.
 * \return false if there was no room.
 */
bool BLI_path_extension_replace(char *path, size_t path_maxncpy, const char *ext)
    ATTR_NONNULL(1, 3);
/**
 * Remove the file extension.
 * \return true if a change was made to `path`.
 */
bool BLI_path_extension_strip(char *path) ATTR_NONNULL(1);
/**
 * Strip's trailing '.'s and adds the extension only when needed
 */
bool BLI_path_extension_ensure(char *path, size_t path_maxncpy, const char *ext)
    ATTR_NONNULL(1, 3);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Comparison / Contains
 * \{ */

/* Path string comparisons: case-insensitive for Windows, case-sensitive otherwise. */
#if defined(WIN32)
#  define BLI_path_cmp BLI_strcasecmp
#  define BLI_path_ncmp BLI_strncasecmp
#else
#  define BLI_path_cmp strcmp
#  define BLI_path_ncmp strncmp
#endif

/**
 * Returns the result of #BLI_path_cmp with both paths normalized and slashes made native.
 *
 * \note #BLI_path_cmp is used for Blender's internal logic to consider paths to be the same
 * #BLI_path_cmp_normalized may be used in when handling other kinds of paths
 * (e.g. importers/exporters) but should be used consistently.
 *
 * Checking the normalized paths is not a guarantee the paths reference different files.
 * An equivalent to Python's `os.path.samefile` could be supported for checking if paths
 * point to the same location on the file-system (following symbolic-links).
 */
int BLI_path_cmp_normalized(const char *p1, const char *p2)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

/** Return true only if #containee_path is contained in #container_path. */
bool BLI_path_contains(const char *container_path, const char *containee_path)
    ATTR_NONNULL(1, 2) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Program Specific Path Functions
 * \{ */

#ifdef _WIN32
bool BLI_path_program_extensions_add_win32(char *program_name, size_t program_name_maxncpy);
#endif
/**
 * Search for a binary (executable)
 */
bool BLI_path_program_search(char *program_filepath,
                             size_t program_filepath_maxncpy,
                             const char *program_name) ATTR_NONNULL(1, 3);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blender Specific Frame Sequence Encode/Decode
 * \{ */

/**
 * Returns in area pointed to by `path` a string of the form `<head><pic><tail>`,
 * where pic is formatted as `numlen` digits with leading zeroes.
 */
void BLI_path_sequence_encode(char *path,
                              size_t path_maxncpy,
                              const char *head,
                              const char *tail,
                              unsigned short numlen,
                              int pic);

/**
 * Looks for a sequence of decimal digits in `path`, preceding any filename extension,
 * returning the integer value if found, or 0 if not.
 *
 * \param path: String to scan.
 * \param head: Optional area to return copy of part of `path` prior to digits,
 * or before dot if no digits.
 * \param tail: Optional area to return copy of part of `path` following digits,
 * or from dot if no digits.
 * \param r_digits_len: Optional to return number of digits found.
 */
int BLI_path_sequence_decode(const char *path,
                             char *head,
                             size_t head_maxncpy,
                             char *tail,
                             size_t tail_maxncpy,
                             unsigned short *r_digits_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blender Specific Frame Number Apply/Strip
 * \{ */

/**
 * Replaces "#" character sequence in last slash-separated component of `path`
 * with frame as decimal integer, with leading zeroes as necessary, to make digits.
 */
bool BLI_path_frame(char *path, size_t path_maxncpy, int frame, int digits) ATTR_NONNULL(1);
/**
 * Replaces "#" character sequence in last slash-separated component of `path`
 * with sta and end as decimal integers, with leading zeroes as necessary, to make digits
 * digits each, with a hyphen in-between.
 */
bool BLI_path_frame_range(char *path, size_t path_maxncpy, int sta, int end, int digits)
    ATTR_NONNULL(1);
/**
 * Get the frame from a filename formatted by blender's frame scheme
 */
bool BLI_path_frame_get(const char *path, int *r_frame, int *r_digits_len) ATTR_NONNULL(1, 2, 3);
/**
 * Given a `path` with digits representing frame numbers, replace the digits with the '#'
 * character and extract the extension.
 * So:      `/some/path_123.jpeg`
 * Becomes: `/some/path_###` with `r_ext` set to `.jpeg`.
 */
void BLI_path_frame_strip(char *path, char *r_ext, size_t ext_maxncpy) ATTR_NONNULL(1, 2);
/**
 * Check if we have '#' chars, usable for #BLI_path_frame, #BLI_path_frame_range
 */
bool BLI_path_frame_check_chars(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blender Specific File Relative Paths
 * \{ */

/**
 * These values need to be hard-coded in structs, DNA does not recognize defines
 * (also defined in `DNA_space_types.h`).
 *
 * \note In general path functions should *not* depend on these hard coded limits,
 * there is an exception for:
 * - #BLI_path_abs
 * - #BLI_path_rel
 * These functions deal specifically with `.blend` file paths,
 * where #FILE_MAX assumed to be the limit of all paths passes into these functions.
 *
 * Some parts of the API which use #FILE_MAX which aren't specifically handling blend file paths,
 * in most cases these can be updated to use #PATH_MAX or a platform specific limit.
 */
#ifndef FILE_MAXDIR
#  define FILE_MAXDIR 768
#  define FILE_MAXFILE 256
#  define FILE_MAX 1024
#endif

/**
 * If path begins with "//", strips that and replaces it with `basepath` directory.
 *
 * \note Also converts drive-letter prefix to something more sensible
 * if this is a non-drive-letter-based system.
 *
 * \param path: The path to convert.
 * \param basepath: The directory to base relative paths with.
 * \return true if the path was relative (started with "//").
 */
bool BLI_path_abs(char path[FILE_MAX], const char *basepath) ATTR_NONNULL(1, 2);
/**
 * Replaces `path` with a relative version (prefixed by "//") such that #BLI_path_abs, given
 * the same `basepath`, will convert it back to its original value.
 */
void BLI_path_rel(char path[FILE_MAX], const char *basepath) ATTR_NONNULL(1);

/**
 * Does path begin with the special "//" prefix that Blender uses to indicate
 * a path relative to the .blend file.
 */
bool BLI_path_is_rel(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Current Working Directory Specific Paths
 * \{ */

/**
 * Checks for a relative path (ignoring Blender's "//") prefix
 * (unlike `!BLI_path_is_rel(path)`).
 * When false, #BLI_path_abs_from_cwd would expand the absolute path.
 */
bool BLI_path_is_abs_from_cwd(const char *path) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
/**
 * Checks for relative path, expanding them relative to the current working directory.
 * \returns true if the expansion was performed.
 *
 * \note Should only be called with command line paths.
 * This is _not_ something Blender's internal paths support, instead they use the "//" prefix.
 * In most cases #BLI_path_abs should be used instead.
 */
bool BLI_path_abs_from_cwd(char *path, size_t path_maxncpy) ATTR_NONNULL(1);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Native Slash Defines & Checks
 * \{ */

#ifdef WIN32
#  define SEP '\\'
#  define ALTSEP '/'
#  define SEP_STR "\\"
#  define ALTSEP_STR "/"
#else
#  define SEP '/'
#  define ALTSEP '\\'
#  define SEP_STR "/"
#  define ALTSEP_STR "\\"
#endif

/**
 * Return true if the slash can be used as a separator on this platform.
 */
BLI_INLINE bool BLI_path_slash_is_native_compat(const char ch)
{
  /* On UNIX it only makes sense to treat `/` as a path separator.
   * On WIN32 either may be used. */
  if (ch == SEP) {
    return true;
  }
#ifdef WIN32
  if (ch == ALTSEP) {
    return true;
  }
#endif
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OS Level Wrappers
 *
 * TODO: move these to a different module, they are not path functions.
 * \{ */

/**
 * Sets the specified environment variable to the specified value,
 * and clears it if `val == NULL`.
 */
void BLI_setenv(const char *env, const char *val) ATTR_NONNULL(1);
/**
 * Only set an environment variable if already not there.
 * Like Unix `setenv(env, val, 0);`
 *
 * (not used anywhere).
 */
void BLI_setenv_if_new(const char *env, const char *val) ATTR_NONNULL(1);
/**
 * Get an environment variable, result has to be used immediately.
 *
 * On windows #getenv gets its variables from a static copy of the environment variables taken at
 * process start-up, causing it to not pick up on environment variables created during runtime.
 * This function uses an alternative method to get environment variables that does pick up on
 * runtime environment variables. The result will be UTF-8 encoded.
 */
const char *BLI_getenv(const char *env) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Current & Parent Directory Defines/Macros
 * \{ */

/* Parent and current dir helpers. */
#define FILENAME_PARENT ".."
#define FILENAME_CURRENT "."

/* Avoid calling `strcmp` on one or two chars! */
#define FILENAME_IS_PARENT(_n) (((_n)[0] == '.') && ((_n)[1] == '.') && ((_n)[2] == '\0'))
#define FILENAME_IS_CURRENT(_n) (((_n)[0] == '.') && ((_n)[1] == '\0'))
#define FILENAME_IS_CURRPAR(_n) \
  (((_n)[0] == '.') && (((_n)[1] == '\0') || (((_n)[1] == '.') && ((_n)[2] == '\0'))))

/** \} */

#ifdef __cplusplus
}
#endif
