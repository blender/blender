# $Id$
# This is the makefile for the bytecode freezing of all modules which
# the main file depends on (last argument in importer rule)

include nan_definitions.mk

SRCDIR = ../modules

TARGETDIR = $(OCGDIR)/blender/bpython/frozen

PYFLAGS=-S -O

default: importer

# fake all and debug targets
all debug: default

install: importer

importer: 
	@[ -d $(TARGETDIR) ] || mkdir -p $(TARGETDIR)
	$(NAN_PYTHON_BINARY) $(PYFLAGS) freeze.py -d -x os -x pprint -x string -x copy -I $(SRCDIR) -o $(TARGETDIR) $(SRCDIR)/VRMLmain.py 
	$(MAKE) -C $(TARGETDIR)

clean:
	rm -f *.pyo

