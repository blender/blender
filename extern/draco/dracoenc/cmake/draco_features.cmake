if (NOT DRACO_CMAKE_DRACO_FEATURES_CMAKE_)
set(DRACO_CMAKE_DRACO_FEATURES_CMAKE_ 1)

set(draco_features_file_name "${draco_build_dir}/draco/draco_features.h")
set(draco_features_list)

# Macro that handles tracking of Draco preprocessor symbols for the purpose of
# producing draco_features.h.
#
# draco_enable_feature(FEATURE <feature_name> [TARGETS <target_name>])
#   FEATURE is required. It should be a Draco preprocessor symbol.
#   TARGETS is optional. It can be one or more draco targets.
#
# When the TARGETS argument is not present the preproc symbol is added to
# draco_features.h. When it is draco_features.h is unchanged, and
# target_compile_options() is called for each target specified.
macro (draco_enable_feature)
  set(def_flags)
  set(def_single_arg_opts FEATURE)
  set(def_multi_arg_opts TARGETS)
  cmake_parse_arguments(DEF "${def_flags}" "${def_single_arg_opts}"
                        "${def_multi_arg_opts}" ${ARGN})
  if ("${DEF_FEATURE}" STREQUAL "")
    message(FATAL_ERROR "Empty FEATURE passed to draco_enable_feature().")
  endif ()

  # Do nothing/return early if $DEF_FEATURE is already in the list.
  list(FIND draco_features_list ${DEF_FEATURE} df_index)
  if (NOT df_index EQUAL -1)
    return ()
  endif ()

  list(LENGTH DEF_TARGETS df_targets_list_length)
  if (${df_targets_list_length} EQUAL 0)
    list(APPEND draco_features_list ${DEF_FEATURE})
  else ()
    foreach (target ${DEF_TARGETS})
      target_compile_definitions(${target} PRIVATE ${DEF_FEATURE})
    endforeach ()
  endif ()
endmacro ()

# Function for generating draco_features.h.
function (draco_generate_features_h)
  file(WRITE "${draco_features_file_name}"
       "// GENERATED FILE -- DO NOT EDIT\n\n"
       "#ifndef DRACO_FEATURES_H_\n"
       "#define DRACO_FEATURES_H_\n\n")

  foreach (feature ${draco_features_list})
    file(APPEND "${draco_features_file_name}" "#define ${feature}\n")
  endforeach ()

  file(APPEND "${draco_features_file_name}" "\n#endif  // DRACO_FEATURES_H_")
endfunction ()

endif ()  # DRACO_CMAKE_DRACO_FEATURES_CMAKE_
