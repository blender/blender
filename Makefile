# toplevel Makefile for blender

export NANBLENDERHOME=$(shell pwd)
MAKEFLAGS=-I$(NANBLENDERHOME)/source --no-print-directory
include source/nan_subdirs.mk

SOURCEDIR = blender
DIRS = extern intern source
