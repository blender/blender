# SPDX-FileCopyrightText: 2016 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  src
  ../gflags/src
)

set(INC_SYS
)

set(SRC
  src/demangle.cc
  src/logging.cc
  src/raw_logging.cc
  src/signalhandler.cc
  src/symbolize.cc
  src/utilities.cc
  src/vlog_is_on.cc

  src/demangle.h
  src/symbolize.h
  src/utilities.h

  src/config.h
  src/config_freebsd.h
  src/config_haiku.h
  src/config_hurd.h
  src/config_linux.h
  src/config_mac.h

  src/base/commandlineflags.h
  src/base/googleinit.h
  src/base/mutex.h

  src/stacktrace.h
  src/stacktrace_generic-inl.h
  src/stacktrace_libunwind-inl.h
  src/stacktrace_powerpc-inl.h
  src/stacktrace_x86_64-inl.h
  src/stacktrace_x86-inl.h
  src/stacktrace_windows-inl.h
)

set(LIB
)

if(NOT WITH_SYSTEM_GFLAGS)
  list(APPEND LIB
    extern_gflags
  )
endif()

if(WIN32)
  list(APPEND SRC
    src/windows/port.cc

    src/windows/glog/raw_logging.h
    src/windows/glog/vlog_is_on.h
    src/windows/glog/logging.h
    src/windows/glog/log_severity.h
    src/windows/port.h
    src/windows/config.h
  )

  list(APPEND INC
    src/windows
  )
  if(MSVC)
    # Suppress warning about google::LogMessageFatal::~LogMessageFatal
    # not returning.
    add_definitions("/wd4722")
  endif()
else()
  list(APPEND INC
    include
  )
  list(APPEND SRC
    include/glog/logging.h
    include/glog/log_severity.h
    include/glog/raw_logging.h
    include/glog/vlog_is_on.h
  )
endif()

add_definitions(${GFLAGS_DEFINES})
add_definitions(${GLOG_DEFINES})

blender_add_lib(extern_glog "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
