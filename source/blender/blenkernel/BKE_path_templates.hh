/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 *
 * \brief Functions and classes for evaluating template expressions in
 * filepaths.
 *
 * To add support for path templates to a path property:
 *
 * 1. Enable #PROP_PATH_SUPPORTS_TEMPLATES in its RNA property flags.
 * 2. Optionally set its RNA path template type (#PropertyPathTemplateType) via
 *    #RNA_def_property_path_template_type(), if you want it to have access to
 *    any purpose-specific variables (see further below).
 * 3. Wherever the evaluated path is needed, generate an appropriate
 *    #VariableMap for it via the #BKE_add_template_variables_*() functions,
 *    and use that to evaluate the path via #BKE_path_apply_template().
 *
 * An example of what step 3 might look like:
 *
 * \code{.cc}
 * VariableMap template_variables;
 * BKE_add_template_variables_general(template_variables, owner_id);
 * BKE_add_template_variables_for_render_path(template_variables, scene);
 * BKE_add_template_variables_for_node(template_variables, owner_node);
 *
 * BKE_path_apply_template(filepath, FILE_MAX, template_variables);
 * \endcode
 *
 * This calls three functions to build the #VariableMap, one for each "kind" of
 * variable (see below).
 *
 * Currently the path template system has three kinds of variables that can be
 * used in expressions:
 *
 * - General variables, which are made available to all paths that support path
 *   templates. For example, the name of the current blend file.
 * - Purpose-specific variables, which are determined by the path property's
 *   #PropertyPathTemplateType flag. For example, render output paths will be
 *   marked as #PROP_VARIABLES_RENDER_OUTPUT, and will therefore get access to
 *   variables like `fps`, which are rendering-specific.
 * - Type-specific variables, which are variables made available to all
 *   path-template paths owned by a particular type of struct. For example,
 *   paths owned by a #bNode will have access to the `node_name` variable,
 *   which provides the name of the owning node.
 *
 * At the moment there is no strict code structure that enforces this, just the
 * following conventions:
 *
 * - All general variables are added by #BKE_add_template_variables_general().
 * - Purpose-specific variables are organized into multiple functions: one
 *   function per variant in #PropertyPathTemplateType, with all variables for
 *   a variant going into the same function. Example:
 *   #BKE_add_template_variables_for_render_path()
 * - Type-specific variables are organized into multiple functions: one per
 *   struct type, with all variables for a struct type going into the same
 *   function. Example: #BKE_add_template_variables_for_node()
 *
 * When adding new #PropertyPathTemplateType variants or adding support for new
 * owning struct types, make sure that you also call their variable-adding
 * functions from #BKE_build_template_variables_for_prop(), which is used for
 * highlighting template path errors in the UI for users.
 */
#pragma once

#include <cmath>
#include <optional>

#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"

#include "BKE_report.hh"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

namespace blender {

struct bContext;
struct PointerRNA;
struct PropertyRNA;

namespace bke::path_templates {

/**
 * Variables (names and associated values) for use in template substitution.
 *
 * Note that this is not intended to be persistent storage, but rather is
 * transient for collecting data that is relevant/available in a given
 * templating context.
 *
 * There are currently four supported variable types:
 *
 * - String
 * - Filepath
 * - Integer
 * - Float
 *
 * Names must be unique across all variable types: you can't have a string *and*
 * integer both with the name "bob".
 *
 * A filepath variable can contain either a full or partial filepath. The
 * distinction between string and filepath variables exists because non-path
 * strings may include phrases like `and/or` or `A:Left`, which shouldn't be
 * interpreted with path semantics. When used in path templating, the contents
 * of string variables are therefore sanitized (replacing `/`, etc.), but the
 * contents of filepath variables are left as-is.
 */
class VariableMap {
  Map<std::string, std::string> strings_;
  Map<std::string, std::string> filepaths_;
  Map<std::string, int64_t> integers_;
  Map<std::string, double> floats_;

 public:
  /**
   * Check if a variable of the given name exists.
   */
  bool contains(StringRef name) const;

  /**
   * Remove the variable with the given name.
   *
   * \return True if the variable existed and was removed, false if it didn't
   * exist in the first place.
   */
  bool remove(StringRef name);

  /**
   * Add a string variable with the given name and value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_string(StringRef name, StringRef value);

  /**
   * Add a filepath variable with the given name and value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_filepath(StringRef name, StringRef value);

  /**
   * Add an integer variable with the given name and value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_integer(StringRef name, int64_t value);

  /**
   * Add a float variable with the given name and value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_float(StringRef name, double value);

  /**
   * Fetch the value of the string variable with the given name.
   *
   * \return The value if a string variable with that name exists,
   * #std::nullopt otherwise.
   */
  std::optional<StringRefNull> get_string(StringRef name) const;

  /**
   * Fetch the value of the filepath variable with the given name.
   *
   * \return The value if a filepath variable with that name exists,
   * #std::nullopt otherwise.
   */
  std::optional<StringRefNull> get_filepath(StringRef name) const;

  /**
   * Fetch the value of the integer variable with the given name.
   *
   * \return The value if a integer variable with that name exists,
   * #std::nullopt otherwise.
   */
  std::optional<int64_t> get_integer(StringRef name) const;

  /**
   * Fetch the value of the float variable with the given name.
   *
   * \return The value if a float variable with that name exists,
   * #std::nullopt otherwise.
   */
  std::optional<double> get_float(StringRef name) const;

  /* ------------------------------------------------------------------
   * Convenience methods, to aid in consistency across different uses. */

  /**
   * Add the filename (sans file extension) from the given path as a variable.
   *
   * For example, if the full path is `/home/bob/project_joe/scene_3.blend`,
   * then `scene_3` is the value of the added variable.
   *
   * If the path doesn't contain a filename, then `fallback` is used for the
   * variable value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_filename_only(StringRef var_name, StringRefNull full_path, StringRef fallback);

  /**
   * Add the path up-to-but-not-including the filename as a variable.
   *
   * For example, if the full path is `/home/bob/project_joe/scene_3.blend`,
   * then `/home/bob/project_joe/` is the value of the added variable.
   *
   * If the path lacks either a filename or a path leading up to that filename,
   * then `fallback` is used for the variable value.
   *
   * If there is already a variable with that name, regardless of type, the new
   * variable is *not* added (no overwriting).
   *
   * \return True if the variable was successfully added, false if there was
   * already a variable with that name.
   */
  bool add_path_up_to_file(StringRef var_name, StringRefNull full_path, StringRef fallback);
};

enum class ErrorType {
  UNESCAPED_CURLY_BRACE,
  VARIABLE_SYNTAX,
  FORMAT_SPECIFIER,
  UNKNOWN_VARIABLE,
};

struct Error {
  ErrorType type;
  IndexRange byte_range;
};

bool operator==(const Error &left, const Error &right);

}  // namespace bke::path_templates

/**
 * Build a template variable map for the passed RNA property.
 *
 * \param C: the context to use for building some variables. This is needed in
 * some cases when the property and its owner do not provide the data needed for
 * a variable. This parameter can be null, but the variables it's needed for
 * will then be absent in the returned variable map.
 *
 * \return On success, returns the template variables for the property. If no
 * property is provided or if the property doesn't support path templates,
 * returns #std::nullopt.
 */
std::optional<bke::path_templates::VariableMap> BKE_build_template_variables_for_prop(
    const bContext *C, PointerRNA *ptr, PropertyRNA *prop);

/**
 * Add the general variables that should be available for all path templates.
 *
 * This is typically used when building a variable map to pass to
 * #BKE_path_apply_template().
 *
 * \param path_owner_id: the ID that owns the path property that will be
 * evaluated with the produced variable map. Passing a nullptr is allowed, but
 * doing so has semantic meaning: it means that there *is no* owning ID. Only
 * pass nullptr when that is actually true, not just out of convenience, because
 * it alters the produced variables.
 *
 * \see #BKE_path_apply_template()
 */
void BKE_add_template_variables_general(bke::path_templates::VariableMap &variables,
                                        const ID *path_owner_id);

/**
 * Add the variables that should be available for render output paths.
 *
 * Corresponds to #PropertyPathTemplateType::PROP_VARIABLES_RENDER_OUTPUT.
 *
 * This is typically used when building a variable map to pass to
 * #BKE_path_apply_template().
 *
 * \param scene: scene to use to get the variable values. Note for the future:
 * when we add a "current frame number" variable it should *not* come from this
 * parameter, but be passed separately. This is because the callers of this
 * function sometimes have the current frame defined separately from the
 * available RenderData (see e.g. #do_makepicstring()).
 *
 * \see #BKE_path_apply_template()
 */
void BKE_add_template_variables_for_render_path(bke::path_templates::VariableMap &variables,
                                                const Scene &scene);

/**
 * Add the variables that should be available for paths owned by a node.
 *
 * This is typically used when building a variable map to pass to
 * #BKE_path_apply_template().
 *
 * \param owning_node: the node that owns the path property that will be
 * evaluated with the produced variable map.
 *
 * \see BKE_path_apply_template()
 */
void BKE_add_template_variables_for_node(bke::path_templates::VariableMap &variables,
                                         const bNode &owning_node);

/**
 * Check if a path contains any templating syntax at all.
 *
 * This is primarily intended to be used as a pre-check in performance-sensitive
 * code to skip path template processing when it's not needed.
 *
 * \return False if the path contains no templating syntax (no template
 * processing is needed). True if the path does contain templating syntax
 * (template processing *is* needed).
 */
bool BKE_path_contains_template_syntax(StringRef path);

/**
 * Validate the templating in the given path.
 *
 * This produces identical errors as #BKE_path_apply_template(), but
 * without modifying the path on success.
 *
 * \return An empty vector if the templating in the path is valid, or a vector
 * of the errors if invalid.
 *
 * \see BKE_path_apply_template()
 */
Vector<bke::path_templates::Error> BKE_path_validate_template(
    StringRef path, const bke::path_templates::VariableMap &template_variables);

/**
 * Perform variable substitution and escaping on the given path.
 *
 * This mutates the path in-place. `path` must be a null-terminated string.
 *
 * The syntax for template expressions is `{variable_name}` or
 * `{variable_name:format_spec}`. The format specification syntax currently only
 * applies to numerical values (integer or float), and uses hash symbols (#) to
 * indicate the number of digits to print the number with. It can be in any of
 * the following forms:
 *
 * - `####`: format as an integer with at least 4 digits, padding with zeros as
 *   needed.
 * - `.###`: format as a float with precisely 3 fractional digits.
 * - `##.###`: format as a float with at least 2 integer-part digits (padded
 *   with zeros as necessary) and precisely 3 fractional-part digits.
 *
 * This function also processes a simple escape sequence for writing literal `{`
 * and `}`: like Python format strings, double braces `{{` and `}}` are treated
 * as escape sequences for `{` and `}`, and are substituted appropriately. Note
 * that this substitution only happens *outside* of the variable syntax, and
 * therefore cannot e.g. be used inside variable names.
 *
 * If any errors are encountered, the path is left unaltered and a list of all
 * errors encountered is returned. Errors include:
 *
 * - Variable expression syntax errors.
 * - Unescaped curly braces.
 * - Referenced variables that cannot be found.
 * - Format specifications that don't apply to the type of variable they're
 *   paired with.
 *
 * \param path_maxncpy: The maximum length that template expansion is allowed
 * to make the template-expanded path (in bytes), including the null terminator.
 * In general, this should be the size of the underlying allocation of `path`.
 *
 * \return On success, an empty vector. If there are errors, a vector of all
 * errors encountered.
 */
Vector<bke::path_templates::Error> BKE_path_apply_template(
    char *path, int path_maxncpy, const bke::path_templates::VariableMap &template_variables);

/**
 * Like `BKE_path_apply_template()`, but takes a heap-allocated path and may
 * reallocate it to make room to expand the template expressions.
 *
 * NOTE: this function takes ownership of the path string, which MUST be heap
 * allocated. Specifically, this function may free that string's memory in the
 * process of resizing the string buffer.
 *
 * \param path: pointer to the path's `char *` pointer. Both this pointer and
 * the pointer it points to MUST be non-null.
 *
 * \see `BKE_path_apply_template()`
 */
Vector<bke::path_templates::Error> BKE_path_apply_template_alloc(
    char **path, int path_maxncpy, const bke::path_templates::VariableMap &template_variables);

/**
 * Produces a human-readable error message for the given template error.
 */
std::string BKE_path_template_error_to_string(const bke::path_templates::Error &error,
                                              StringRef path);

/**
 * Logs a report for the given template errors, with human-readable error
 * messages.
 */
void BKE_report_path_template_errors(ReportList *reports,
                                     eReportType report_type,
                                     StringRef path,
                                     Span<bke::path_templates::Error> errors);

/**
 * Format the given floating point value with the provided format specifier. The format specifier
 * is e.g. the `##.###` in `{name:##.###}`.
 *
 * \return #std::nullopt if the format specifier is invalid.
 */
std::optional<std::string> BKE_path_template_format_float(StringRef format_specifier,
                                                          double value);

/** Same as #BKE_path_template_format_float but for formatting an integer value. */
std::optional<std::string> BKE_path_template_format_int(StringRef format_specifier, int64_t value);

}  // namespace blender
