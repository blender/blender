# toplevel Makefile for blender

export NANBLENDERHOME=$(shell pwd)
MAKEFLAGS=-I$(NANBLENDERHOME)/source --no-print-directory

SOURCEDIR = blender
DIRS = extern intern source
include source/nan_subdirs.mk

.PHONY: release
release:
	@echo "====> $(MAKE) $@ in $(SOURCEDIR)/$@" ;\
	    $(MAKE) -C $@ $@ || exit 1;
