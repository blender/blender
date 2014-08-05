#!/usr/bin/env python3
#
# Copyright 2014 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License

# This script generates either header or implementation file from
# a CUDA header files.
#
# Usage: cuew hdr|impl [/path/to/cuda/includes]
#  - hdr means header file will be generated and printed to stdout.
#  - impl means implementation file will be generated and printed to stdout.
#  - /path/to/cuda/includes is a path to a folder with cuda.h and cudaGL.h
#    for which wrangler will be generated.

import os
import sys
from cuda_errors import CUDA_ERRORS
from pycparser import c_parser, c_ast, parse_file
from subprocess import Popen, PIPE

INCLUDE_DIR = "/usr/include"
LIB = "CUEW"
REAL_LIB = "CUDA"
VERSION_MAJOR = "1"
VERSION_MINOR = "2"
COPYRIGHT = """/*
 * Copyright 2011-2014 Blender Foundation
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
 */"""
FILES = ["cuda.h", "cudaGL.h"]

TYPEDEFS = []
FUNC_TYPEDEFS = []
SYMBOLS = []
DEFINES = []
DEFINES_V2 = []
ERRORS = []


class FuncDefVisitor(c_ast.NodeVisitor):
    indent = 0
    prev_complex = False
    dummy_typedefs = ['size_t', 'CUdeviceptr']

    def _get_quals_string(self, node):
        if node.quals:
            return ' '.join(node.quals) + ' '
        return ''

    def _get_ident_type(self, node):
        if isinstance(node, c_ast.PtrDecl):
            return self._get_ident_type(node.type.type) + '*'
        if isinstance(node, c_ast.ArrayDecl):
            return self._get_ident_type(node.type)
        elif isinstance(node, c_ast.Struct):
            if node.name:
                return 'struct ' + node.name
            else:
                self.indent += 1
                struct = self._stringify_struct(node)
                self.indent -= 1
                return "struct {\n" + \
                       struct + ("  " * self.indent) + "}"
        elif isinstance(node, c_ast.Union):
            self.indent += 1
            union = self._stringify_struct(node)
            self.indent -= 1
            return "union {\n" + union + ("  " * self.indent) + "}"
        elif isinstance(node, c_ast.Enum):
            return 'enum ' + node.name
        elif isinstance(node, c_ast.TypeDecl):
            return self._get_ident_type(node.type)
        else:
            return node.names[0]

    def _stringify_param(self, param):
        param_type = param.type
        result = self._get_quals_string(param)
        result += self._get_ident_type(param_type)
        if param.name:
            result += ' ' + param.name
        if isinstance(param_type, c_ast.ArrayDecl):
            # TODO(sergey): Workaround to deal with the
            # preprocessed file where array size got
            # substituded.
            dim = param_type.dim.value
            if param.name == "reserved" and dim == "64":
                dim = "CU_IPC_HANDLE_SIZE"
            result += '[' + dim + ']'
        return result

    def _stringify_params(self, params):
        result = []
        for param in params:
            result.append(self._stringify_param(param))
        return ', '.join(result)

    def _stringify_struct(self, node):
        result = ""
        children = node.children()
        for child in children:
            member = self._stringify_param(child[1])
            result += ("  " * self.indent) + member + ";\n"
        return result

    def _stringify_enum(self, node):
        result = ""
        children = node.children()
        for child in children:
            if isinstance(child[1], c_ast.EnumeratorList):
                enumerators = child[1].enumerators
                for enumerator in enumerators:
                    result += ("  " * self.indent) + enumerator.name
                    if enumerator.value:
                        result += " = " + enumerator.value.value
                    result += ",\n"
                    if enumerator.name.startswith("CUDA_ERROR_"):
                        ERRORS.append(enumerator.name)
        return result

    def visit_Decl(self, node):
        if node.type.__class__.__name__ == 'FuncDecl':
            if isinstance(node.type, c_ast.FuncDecl):
                func_decl = node.type
                func_decl_type = func_decl.type

                typedef = 'typedef '
                symbol_name = None

                if isinstance(func_decl_type, c_ast.TypeDecl):
                    symbol_name = func_decl_type.declname
                    typedef += self._get_quals_string(func_decl_type)
                    typedef += self._get_ident_type(func_decl_type.type)
                    typedef += ' CUDAAPI'
                    typedef += ' t' + symbol_name
                elif isinstance(func_decl_type, c_ast.PtrDecl):
                    ptr_type = func_decl_type.type
                    symbol_name = ptr_type.declname
                    typedef += self._get_quals_string(ptr_type)
                    typedef += self._get_ident_type(func_decl_type)
                    typedef += ' CUDAAPI'
                    typedef += ' t' + symbol_name

                typedef += '(' + \
                    self._stringify_params(func_decl.args.params) + \
                    ');'

                SYMBOLS.append(symbol_name)
                FUNC_TYPEDEFS.append(typedef)

    def visit_Typedef(self, node):
        if node.name in self.dummy_typedefs:
            return

        complex = False
        type = self._get_ident_type(node.type)
        quals = self._get_quals_string(node)

        if isinstance(node.type.type, c_ast.Struct):
            self.indent += 1
            struct = self._stringify_struct(node.type.type)
            self.indent -= 1
            typedef = quals + type + " {\n" + struct + "} " + node.name
            complex = True
        elif isinstance(node.type.type, c_ast.Enum):
            self.indent += 1
            enum = self._stringify_enum(node.type.type)
            self.indent -= 1
            typedef = quals + type + " {\n" + enum + "} " + node.name
            complex = True
        else:
            typedef = quals + type + " " + node.name
        if complex or self.prev_complex:
            typedef = "\ntypedef " + typedef + ";"
        else:
            typedef = "typedef " + typedef + ";"

        TYPEDEFS.append(typedef)

        self.prev_complex = complex


def get_latest_cpp():
    path_prefix = "/usr/bin"
    for cpp_version in ["9", "8", "7", "6", "5", "4"]:
        test_cpp = os.path.join(path_prefix, "cpp-4." + cpp_version)
        if os.path.exists(test_cpp):
            return test_cpp
    return None


def preprocess_file(filename, cpp_path):
    args = [cpp_path, "-I./"]
    if filename.endswith("GL.h"):
        args.append("-DCUDAAPI= ")
    args.append(filename)

    try:
        pipe = Popen(args,
                     stdout=PIPE,
                     universal_newlines=True)
        text = pipe.communicate()[0]
    except OSError as e:
        raise RuntimeError("Unable to invoke 'cpp'.  " +
            'Make sure its path was passed correctly\n' +
            ('Original error: %s' % e))

    return text


def parse_files():
    parser = c_parser.CParser()
    cpp_path = get_latest_cpp()

    for filename in FILES:
        filepath = os.path.join(INCLUDE_DIR, filename)
        dummy_typedefs = {}
        text = preprocess_file(filepath, cpp_path)

        if filepath.endswith("GL.h"):
            dummy_typedefs = {
                "CUresult": "int",
                "CUgraphicsResource": "void *",
                "CUdevice": "void *",
                "CUcontext": "void *",
                "CUdeviceptr": "void *",
                "CUstream": "void *"
                }

            text = "typedef int GLint;\n" + text
            text = "typedef unsigned int GLuint;\n" + text
            text = "typedef unsigned int GLenum;\n" + text
            text = "typedef long size_t;\n" + text

        for typedef in sorted(dummy_typedefs):
            text = "typedef " + dummy_typedefs[typedef] + " " + \
                typedef + ";\n" + text

        ast = parser.parse(text, filepath)

        with open(filepath) as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith("#define"):
                    line = line[8:-1]
                    token = line.split()
                    if token[0] not in ("__cuda_cuda_h__",
                                        "CUDA_CB",
                                        "CUDAAPI"):
                        DEFINES.append(token)

            for line in lines:
                # TODO(sergey): Use better matching rule for _v2 symbols.
                if line[0].isspace() and line.lstrip().startswith("#define"):
                    line = line[12:-1]
                    token = line.split()
                    if len(token) == 2 and token[1].endswith("_v2"):
                        DEFINES_V2.append(token)

        v = FuncDefVisitor()
        for typedef in dummy_typedefs:
            v.dummy_typedefs.append(typedef)
        v.visit(ast)

        FUNC_TYPEDEFS.append('')
        SYMBOLS.append('')


def print_copyright():
    print(COPYRIGHT)
    print("")


def open_header_guard():
    print("#ifndef __%s_H__" % (LIB))
    print("#define __%s_H__" % (LIB))
    print("")
    print("#ifdef __cplusplus")
    print("extern \"C\" {")
    print("#endif")
    print("")


def close_header_guard():
    print("")
    print("#ifdef __cplusplus")
    print("}")
    print("#endif")
    print("")
    print("#endif  /* __%s_H__ */" % (LIB))


def print_header():
    print_copyright()
    open_header_guard()

    # Fot size_t.
    print("#include <stdlib.h>")
    print("")

    print("/* Defines. */")
    print("#define %s_VERSION_MAJOR %s" % (LIB, VERSION_MAJOR))
    print("#define %s_VERSION_MINOR %s" % (LIB, VERSION_MINOR))
    print("")
    for define in DEFINES:
        print('#define %s' % (' '.join(define)))
    print("")

    print("""/* Functions which changed 3.1 -> 3.2 for 64 bit stuff,
 * the cuda library has both the old ones for compatibility and new
 * ones with _v2 postfix,
 */""")
    for define in DEFINES_V2:
        print('#define %s' % (' '.join(define)))
    print("")

    print("/* Types. */")

    # We handle this specially because of the file is
    # getting preprocessed.
    print("""#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif
""")

    for typedef in TYPEDEFS:
        print('%s' % (typedef))

    # TDO(sergey): This is only specific to CUDA wrapper.
    print("""
#ifdef _WIN32
#  define CUDAAPI __stdcall
#  define CUDA_CB __stdcall
#else
#  define CUDAAPI
#  define CUDA_CB
#endif
""")

    print("/* Function types. */")
    for func_typedef in FUNC_TYPEDEFS:
        print('%s' % (func_typedef))
    print("")

    print("/* Function declarations. */")
    for symbol in SYMBOLS:
        if symbol:
            print('extern t%s *%s;' % (symbol, symbol))
        else:
            print("")

    print("")
    print("enum {")
    print("  CUEW_SUCCESS = 0,")
    print("  CUEW_ERROR_OPEN_FAILED = -1,")
    print("  CUEW_ERROR_ATEXIT_FAILED = -2,")
    print("};")
    print("")
    print("int %sInit(void);" % (LIB.lower()))
    # TODO(sergey): Get rid of hardcoded CUresult.
    print("const char *%sErrorString(CUresult result);" % (LIB.lower()))
    print("const char *cuewCompilerPath(void);")
    print("int cuewCompilerVersion(void);")

    close_header_guard()


def print_dl_wrapper():
    print("""#ifdef _WIN32
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
""")


def print_dl_helper_macro():
    print("""#define %s_LIBRARY_FIND_CHECKED(name) \\
        name = (t##name *)dynamic_library_find(lib, #name);

#define %s_LIBRARY_FIND(name) \\
        name = (t##name *)dynamic_library_find(lib, #name); \\
        assert(name);

static DynamicLibrary lib;""" % (REAL_LIB, REAL_LIB))
    print("")


def print_dl_close():
    print("""static void %sExit(void) {
  if(lib != NULL) {
    /*  Ignore errors. */
    dynamic_library_close(lib);
    lib = NULL;
  }
}""" % (LIB.lower()))
    print("")


def print_lib_path():
    # TODO(sergey): get rid of hardcoded libraries.
    print("""#ifdef _WIN32
  /* Expected in c:/windows/system or similar, no path needed. */
  const char *path = "nvcuda.dll";
#elif defined(__APPLE__)
  /* Default installation path. */
  const char *path = "/usr/local/cuda/lib/libcuda.dylib";
#else
  const char *path = "libcuda.so";
#endif""")


def print_init_guard():
    print("""  static int initialized = 0;
  static int result = 0;
  int error, driver_version;

  if (initialized) {
    return result;
  }

  initialized = 1;

  error = atexit(cuewExit);
  if (error) {
    result = CUEW_ERROR_ATEXIT_FAILED;
    return result;
  }

  /* Load library. */
  lib = dynamic_library_open(path);

  if (lib == NULL) {
    result = CUEW_ERROR_OPEN_FAILED;
    return result;
  }""")
    print("")


def print_driver_version_guard():
    # TODO(sergey): Currently it's hardcoded for CUDA only.
    print("""  /* Detect driver version. */
  driver_version = 1000;

  %s_LIBRARY_FIND_CHECKED(cuDriverGetVersion);
  if (cuDriverGetVersion) {
    cuDriverGetVersion(&driver_version);
  }

  /* We require version 4.0. */
  if (driver_version < 4000) {
    result = CUEW_ERROR_OPEN_FAILED;
    return result;
  }""" % (REAL_LIB))


def print_dl_init():
    print("int %sInit(void) {" % (LIB.lower()))

    print("  /* Library paths. */")
    print_lib_path()
    print_init_guard()
    print_driver_version_guard()

    print("  /* Fetch all function pointers. */")
    for symbol in SYMBOLS:
        if symbol:
            print("  %s_LIBRARY_FIND(%s);" % (REAL_LIB, symbol))
        else:
            print("")

    print("")
    print("  result = CUEW_SUCCESS;")
    print("  return result;")

    print("}")


def print_implementation():
    print_copyright()

    # TODO(sergey): Get rid of hardcoded header.
    print("""#ifdef _MSC_VER
#  define snprintf _snprintf
#  define popen _popen
#  define pclose _pclose
#  define _CRT_SECURE_NO_WARNINGS
#endif
""")
    print("#include <cuew.h>")
    print("#include <assert.h>")
    print("#include <stdio.h>")
    print("#include <string.h>")
    print("#include <sys/stat.h>")
    print("")

    print_dl_wrapper()
    print_dl_helper_macro()

    print("/* Function definitions. */")
    for symbol in SYMBOLS:
        if symbol:
            print('t%s *%s;' % (symbol, symbol))
        else:
            print("")
    print("")

    print_dl_close()

    print("/* Implementation function. */")
    print_dl_init()

    print("")
    # TODO(sergey): Get rid of hardcoded CUresult.
    print("const char *%sErrorString(CUresult result) {" % (LIB.lower()))
    print("  switch(result) {")
    print("    case CUDA_SUCCESS: return \"No errors\";")

    for error in ERRORS:
        if error in CUDA_ERRORS:
            str = CUDA_ERRORS[error]
        else:
            str = error[11:]
        print("    case %s: return \"%s\";" % (error, str))

    print("    default: return \"Unknown CUDA error value\";")
    print("  }")
    print("}")

    from cuda_extra import extra_code
    print(extra_code)

if __name__ == "__main__":

    if len(sys.argv) != 2 and len(sys.argv) != 3:
        print("Usage: %s hdr|impl [/path/to/cuda/toolkit/include]" %
              (sys.argv[0]))
        exit(1)

    if len(sys.argv) == 3:
        INCLUDE_DIR = sys.argv[2]

    parse_files()

    if sys.argv[1] == "hdr":
        print_header()
    elif sys.argv[1] == "impl":
        print_implementation()
    else:
        print("Unknown command %s" % (sys.argv[1]))
        exit(1)
