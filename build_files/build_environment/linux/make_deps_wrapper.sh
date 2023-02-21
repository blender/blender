#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

# This script ensures:
# - One dependency is built at a time.
# - That dependency uses all available cores.
#
# Without this, simply calling `make -j$(nproc)` from the `${CMAKE_BUILD_DIR}/deps/`
# directory will build many projects at once.
#
# This is undesirable for the following reasons:
#
# - The output from projects is mixed together,
#   making it difficult to track down the cause of a build failure.
#
# - Larger dependencies such as LLVM can bottleneck the build process,
#   making it necessary to cancel the build and manually run build commands in each directory.
#
# - Building many projects at once means canceling (Control-C) can lead to the build being in an undefined state.
#   It's possible canceling happens as a patch is being applied or files are being copied.
#   (steps that aren't part of the compilation process where it's typically safe to cancel).

if [[ -z "$MY_MAKE_CALL_LEVEL" ]]; then
  export MY_MAKE_CALL_LEVEL=0
  export MY_MAKEFLAGS=$MAKEFLAGS

  # Extract the jobs argument (`-jN`, `-j N`, `--jobs=N`).
  add_next=0
  for i in "$@"; do
    case $i in
      -j*)
        export MY_JOBS_ARG=$i
        if [ "$MY_JOBS_ARG" = "-j" ]; then
          add_next=1
        fi
        ;;
      --jobs=*)
        shift # past argument=value
        MY_JOBS_ARG=$i
        ;;
      *)
        if (( add_next == 1 )); then
          MY_JOBS_ARG="$MY_JOBS_ARG $i"
          add_next=0
        fi
        ;;
    esac
  done
  unset i add_next

  if [[ -z "$MY_JOBS_ARG" ]]; then
    MY_JOBS_ARG="-j$(nproc)"
  fi
  export MY_JOBS_ARG
  # Support user defined `MAKEFLAGS`.
  export MAKEFLAGS="$MY_MAKEFLAGS -j1"
else
  export MY_MAKE_CALL_LEVEL=$(( MY_MAKE_CALL_LEVEL + 1 ))
  if (( MY_MAKE_CALL_LEVEL == 1 )); then
    # Important to set jobs to 1, otherwise user defined jobs argument is used.
    export MAKEFLAGS="$MY_MAKEFLAGS -j1"
  elif (( MY_MAKE_CALL_LEVEL == 2 )); then
    # This is the level used by each sub-project.
    export MAKEFLAGS="$MY_MAKEFLAGS $MY_JOBS_ARG"
  fi
  # Else leave `MY_MAKEFLAGS` flags as-is, avoids setting a high number of jobs on recursive
  # calls (which may easily run out of memory). Let the job-server handle the rest.
fi

# Useful for troubleshooting the wrapper.
# echo "Call level: $MY_MAKE_CALL_LEVEL, args=$@".

# Call actual make but ensure recursive calls run via this script.
exec make MAKE="$0" "$@"
