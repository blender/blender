# toplevel Makefile for blender

export NANBLENDERHOME=$(shell pwd)
export MAKEFLAGS="-I $(NANBLENDERHOME)/source --no-print-directory"

DIRS = extern intern source

all: $(DIRS)

$(DIRS):
	$(MAKE) -C $@

.PHONY: $(DIRS) all
