#!/bin/bash

SDL="SDL2"
INCLUDE_DIR="/usr/include/${SDL}"
SCRIPT=`realpath -s $0`
DIR=`dirname $SCRIPT`
DIR=`dirname $DIR`

mkdir -p $DIR/include/${SDL}
mkdir -p $DIR/src

rm -rf $DIR/include/${SDL}/*.h
rm -rf $DIR/src/sdlew.c

echo "Generating sdlew headers..."

UNSUPPORTED="SDL_MemoryBarrierRelease SDL_MemoryBarrierAcquire SDL_AtomicCAS SDL_AtomicCASPtr \
  SDL_iPhoneSetAnimationCallback SDL_iPhoneSetEventPump SDL_AndroidGetJNIEnv SDL_AndroidGetActivity \
  SDL_AndroidGetActivity SDL_AndroidGetInternalStoragePath SDL_AndroidGetExternalStorageState \
  SDL_AndroidGetExternalStoragePath SDL_CreateShapedWindow SDL_IsShapedWindow tSDL_SetWindowShape \
  SDL_GetShapedWindowMode"

for header in $INCLUDE_DIR/*; do
  filename=`basename $header`
  cat $header \
    | sed -r 's/extern DECLSPEC ((const )?[a-z0-9_]+(\s\*)?)\s?SDLCALL /typedef \1 SDLCALL t/i' \
    > $DIR/include/${SDL}/$filename

  line_num=`cat $DIR/include/${SDL}/$filename | grep -n "Ends C function" | cut -d : -f 1`
  if [ ! -z "$line_num" ]; then
    functions=`grep -E 'typedef [A-Za-z0-9_ \*]+ SDLCALL' $DIR/include/${SDL}/$filename \
      | sed -r 's/typedef [A-Za-z0-9_ \*]+ SDLCALL t([a-z0-9_]+).*/extern t\1 *\1;/i'`
    functions=`echo "${functions}" | sed -e 's/[\/&]/\\\&/g'`
    echo "$functions" | while read function; do
      if [ -z "$function" ]; then
        continue;
      fi
      func_name=`echo $function | cut -d '*' -f 2 | sed -r 's/;//'`
      if [ ! -z "`echo "$UNSUPPORTED" | grep $func_name`" ]; then
        continue;
      fi
      if [ "$func_name" == "SDL_memcpy" ]; then
        line_num=`cat $DIR/include/${SDL}/$filename | grep -n "SDL_memcpy4" | cut -d : -f 1`
        sed -ri "${line_num}s/(.*)/${function}\n\1/" $DIR/include/${SDL}/$filename
      else
        sed -ri "${line_num}s/(.*)/${function}\n\1/" $DIR/include/${SDL}/$filename
      fi
      line_num=`cat $DIR/include/${SDL}/$filename | grep -n "Ends C function" | cut -d : -f 1`
    done
    line_num=`cat $DIR/include/${SDL}/$filename | grep -n "Ends C function" | cut -d : -f 1`
    sed -ri "${line_num}s/(.*)/\n\1/" $DIR/include/${SDL}/$filename
  fi

  if [ $filename == "SDL_stdinc.h"  ]; then
    cat $header | grep -E '#if(def)? (defined\()?HAVE_' | sed -r 's/#if(def)? //' | while read check; do
      func_names=`cat $DIR/include/${SDL}/$filename \
                   | grep -A 8 "$check\$" \
                   | grep -v struct \
                   | grep 'typedef' \
                   | sed -r 's/typedef [a-z0-9_ \*]+ SDLCALL ([a-z0-9_]+).*/\1/i'`
      full_check=`echo "${check}" | sed -e 's/[\/&]/\\\&/g'`
      if [ ! -z "`echo $full_check | grep defined`"  ]; then
        full_check="#if !($full_check)"
      else
        full_check="#ifndef $full_check"
      fi
      for func_name in $func_names; do
         line_num=`grep -n "extern ${func_name} \*" $DIR/include/${SDL}/$filename | cut -d : -f 1`
         let prev_num=line_num-1
         if [ -z "`cat $DIR/include/${SDL}/$filename | head -n $prev_num | tail -n 1 | grep '#if'`" ]; then
           sed -ri "${line_num}s/(.*)/$full_check \/* GEN_CHECK_MARKER *\/\n\1\n#endif \/* GEN_CHECK_MARKER *\//" $DIR/include/${SDL}/$filename
         fi
      done
    done
  fi
done

cat << EOF > $DIR/include/sdlew.h
/*
 * Copyright 2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#ifndef __SDL_EW_H__
#define __SDL_EW_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
  SDLEW_SUCCESS = 0,
  SDLEW_ERROR_OPEN_FAILED = -1,
  SDLEW_ERROR_ATEXIT_FAILED = -2,
  SDLEW_ERROR_VERSION = -3,
};

int sdlewInit(void);

#ifdef __cplusplus
}
#endif

#endif  /* __SDL_EW_H__ */
EOF

echo "Generating sdlew sources..."

cat << EOF > $DIR/src/sdlew.c
/*
 * Copyright 2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#ifdef _MSC_VER
#  define snprintf _snprintf
#  define popen _popen
#  define pclose _pclose
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "sdlew.h"

#include "${SDL}/SDL.h"
#include "${SDL}/SDL_syswm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define VC_EXTRALEAN
#  include <windows.h>

/* Utility macros. */

typedef HMODULE DynamicLibrary;

#  define dynamic_library_open(path)         LoadLibrary(path)
#  define dynamic_library_close(lib)         FreeLibrary(lib)
#  define dynamic_library_find(lib, symbol)  GetProcAddress(lib, symbol)
#else
#  include <dlfcn.h>

typedef void* DynamicLibrary;

#  define dynamic_library_open(path)         dlopen(path, RTLD_NOW)
#  define dynamic_library_close(lib)         dlclose(lib)
#  define dynamic_library_find(lib, symbol)  dlsym(lib, symbol)
#endif

#define SDL_LIBRARY_FIND_CHECKED(name) \
        name = (t##name *)dynamic_library_find(lib, #name); \
        assert(name);

#define SDL_LIBRARY_FIND(name) \
        name = (t##name *)dynamic_library_find(lib, #name);

static DynamicLibrary lib;

EOF

content=`grep --no-filename -ER "extern tSDL|GEN_CHECK_MARKER" $DIR/include/${SDL}/`

echo "$content" | sed -r 's/extern t([a-z0-9_]+).*/t\1 *\1;/gi' >> $DIR/src/sdlew.c

cat << EOF >> $DIR/src/sdlew.c

static void sdlewExit(void) {
  if(lib != NULL) {
    /*  Ignore errors. */
    dynamic_library_close(lib);
    lib = NULL;
  }
}

/* Implementation function. */
int sdlewInit(void) {
  /* Library paths. */
#ifdef _WIN32
  /* Expected in c:/windows/system or similar, no path needed. */
  const char *path = "SDL2.dll";
#elif defined(__APPLE__)
  /* Default installation path. */
  const char *path = "/usr/local/cuda/lib/libSDL2.dylib";
#else
  const char *path = "libSDL2.so";
#endif
  static int initialized = 0;
  static int result = 0;
  int error;

  if (initialized) {
    return result;
  }

  initialized = 1;

  error = atexit(sdlewExit);
  if (error) {
    result = SDLEW_ERROR_ATEXIT_FAILED;
    return result;
  }

  /* Load library. */
  lib = dynamic_library_open(path);

  if (lib == NULL) {
    result = SDLEW_ERROR_OPEN_FAILED;
    return result;
  }

EOF

echo "$content" | sed -r 's/extern t([a-z0-9_]+).*/  SDL_LIBRARY_FIND(\1);/gi' >> $DIR/src/sdlew.c

cat << EOF >> $DIR/src/sdlew.c

  result = SDLEW_SUCCESS;

  return result;
}
EOF

sed -i 's/\s\/\* GEN_CHECK_MARKER \*\///g' $DIR/src/sdlew.c
sed -i 's/\s\/\* GEN_CHECK_MARKER \*\///g' $DIR/include/${SDL}/SDL_stdinc.h
