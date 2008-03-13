#
# $Id$
#
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
#

LIBNAME = ftgl
DIR = $(OCGDIR)/extern/$(LIBNAME)

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CSRCS = 
CCSRCS =  FTBitmapGlyph.cpp FTCharmap.cpp FTContour.cpp FTExtrdGlyph.cpp \
	FTFace.cpp FTFont.cpp FTGLBitmapFont.cpp FTGLExtrdFont.cpp \
        FTGLOutlineFont.cpp FTGLPixmapFont.cpp FTGLPolygonFont.cpp \
        FTGLTextureFont.cpp FTGlyph.cpp FTGlyphContainer.cpp FTLibrary.cpp \
        FTOutlineGlyph.cpp FTPixmapGlyph.cpp FTPoint.cpp FTPolyGlyph.cpp \
        FTSize.cpp FTTextureGlyph.cpp FTVectoriser.cpp

include nan_compile.mk 
CPPFLAGS += -I../include
CPPFLAGS += -I$(NAN_FREETYPE)/include -I$(NAN_FREETYPE)/include/freetype2
CPPFLAGS += -I$(OPENGL_HEADERS) 

install: all debug
	@[ -d $(NAN_FTGL) ] || mkdir -p $(NAN_FTGL)
	@[ -d $(NAN_FTGL)/include ] || mkdir -p $(NAN_FTGL)/include
	@[ -d $(NAN_FTGL)/lib ] || mkdir -p $(NAN_FTGL)/lib
	@[ -d $(NAN_FTGL)/lib/debug ] || mkdir -p $(NAN_FTGL)/lib/debug
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/lib$(LIBNAME).a $(NAN_FTGL)/lib/
#	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/debug/lib$(LIBNAME).a $(NAN_FTGL)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_FTGL)/lib/lib$(LIBNAME).a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh ../include/*.h $(NAN_FTGL)/include/

