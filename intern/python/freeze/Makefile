# $Id$
# This is the makefile for the bytecode freezing of all modules which
# the main file depends on (last argument in importer rule)

include nan_definitions.mk

SRCDIR = ../modules

TARGETDIR = $(OCGDIR)/blender/bpython/frozen

ifeq ($(FREE_WINDOWS),true)
	export NAN_PYTHON_BINARY = python
endif

PYFLAGS=-S -O

default: importer

# fake all and debug targets
all debug: default

install: importer

importer: 
ifeq ($(FREE_WINDOWS),true)
	cp $(NAN_PYTHON)/lib/python22.dll .
	dlltool.exe -v --output-lib $(NAN_PYTHON)/lib/freepy.a \
		--input-def $(NANBLENDERHOME)/dlltool/python.def \
		--dllname python22.dll
	ranlib $(NAN_PYTHON)/lib/freepy.a
	rm ./python22.dll
	@[ -d $(TARGETDIR) ] || mkdir -p $(TARGETDIR)
endif
	$(NAN_PYTHON_BINARY) $(PYFLAGS) freeze.py -d -x os -x pprint -x string -x copy -I $(SRCDIR) -o $(TARGETDIR) $(SRCDIR)/VRMLmain.py 
	$(MAKE) -C $(TARGETDIR)

clean:
	rm -f *.pyo

