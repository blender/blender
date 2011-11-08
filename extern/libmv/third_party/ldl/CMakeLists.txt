include_directories(../ufconfig)
include_directories(Include)
add_library(ldl Source/ldl.c)

LIBMV_INSTALL_THIRD_PARTY_LIB(ldl)
