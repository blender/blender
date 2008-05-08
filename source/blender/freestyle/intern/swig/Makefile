# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                             W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

CXX	  = g++
RM	  = rm -f
INSTALL	  = install
SWIG	  = swig

PYTHON_VERSION_MAJ = 2
PYTHON_VERSION_MIN = 4

BUILD     = debug
PLATEFORM = linux-g++
MODNAME   = Freestyle

LIBNAME   = _${MODNAME}.so
LIBDIR    = ../../build/${PLATEFORM}/${BUILD}/lib/python
IPATH     = -I../geometry -I../image -I../scene_graph -I../stroke \
            -I../system -I../view_map -I../winged_edge \
            -I/usr/include/python${PYTHON_VERSION_MAJ}.${PYTHON_VERSION_MIN}
# options to compile with static python : -lpthread -lm -lutil -shared -Xlinker -x -export-dynamic
#OPTI      = -lpthread -lm -lutil -shared -Xlinker -x -export-dynamic #-O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
DBUG      = #-ggdb #-pg
WARN	  = -w #-W -Wall #-pedantic -ansi

CFLAGS    = ${OPTI} ${DBUG} ${WARN} ${IPATH}
LDFLAGS   = ${OPTI} ${DBUG} ${WARN} ${LPATH}

SWIG_SRC  = Freestyle.i
PY_SRC    = ${SWIG_SRC:.i=.py}
CXX_SRC	  = ModuleWrapper.cpp
CXX_INC	  = ${CXX_SRC:.cpp=.h}
CXX_OBJ	  = ${CXX_SRC:.cpp=.o}

ifndef DBUG
  STRIP   = strip --strip-all ${LIBDIR}/${LIBNAME}
endif

# # # # # # # # # # # # # # # #   R u l e s   # # # # # # # # # # # # # # # # #

all	: swig cxx

.PHONY	: clean re

clean	: swig_clean cxx_clean

re	: swig_re cxx_re

# # # # # # # # # # # # #   S W I G   r u l e s   # # # # # # # # # # # # # # #

.SUFFIXES:
.SUFFIXES: .i .py .cpp .h .o

.IGNORE	:

swig	: ${CXX_INC} ${CXX_SRC} ${PY_SRC}

.i.py	: ${SWIG_SRC}
	${SWIG} -c++ -python -o ${CXX_SRC} $<

.PHONY	: swig_clean swig_re

swig_clean	:
	${RM} *~ \#* .\#* : ${CXX_INC} ${CXX_SRC} ${PY_SRC}

swig_re	: swig_clean swig

# # # # # # # # #   P y t h o n   &   C + +   r u l e s   # # # # # # # # # # #

cxx	: ${CXX_OBJ} ${PY_SRC}
	${INSTALL} -d ${LIBDIR}
	${CC} -shared -o ${LIBDIR}/${LIBNAME} ${CXX_OBJ}
	${INSTALL} ${PY_SRC} ${LIBDIR}
	@${STRIP}

.cpp.o	: ${CXX_INC} ${CXX_SRC}
	${CXX} ${CFLAGS} -c $< -o $@

${CXX_SRC}	: ${PY_SRC}

${CXX_INC}	: ${PY_SRC}

.PHONY	: cxx_clean cxx_re

cxx_clean	:
	${RM} *~ \#* .\#* *.core gmon.out ${CXX_OBJ} 

cxx_re	: cxx_clean cxx_all
