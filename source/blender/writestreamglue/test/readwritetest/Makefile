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
# Streamglue read/write testing
#

DIR = $(OCGDIR)/blender/writestreamglue/readwritetest
ALLTARGETS = $(OBJS) $(DIR)/$(DEBUG_DIR)readwritetest

include nan_compile.mk

CFLAGS += -funsigned-char -g

# includes:
CPPFLAGS += -I../..
CPPFLAGS += -I../../../readstreamglue

# libs:
TESTLIBS += $(OCGDIR)/blender/readstreamglue/$(DEBUG_DIR)libreadstreamglue.a
TESTLIBS += $(OCGDIR)/blender/writestreamglue/$(DEBUG_DIR)libwritestreamglue.a
TESTLIBS += $(OCGDIR)/blender/inflate/$(DEBUG_DIR)libinflate.a
TESTLIBS += $(OCGDIR)/blender/deflate/$(DEBUG_DIR)libdeflate.a
TESTLIBS += $(OCGDIR)/blender/encrypt/$(DEBUG_DIR)libencrypt.a
TESTLIBS += $(OCGDIR)/blender/decrypt/$(DEBUG_DIR)libdecrypt.a
TESTLIBS += $(OCGDIR)/blender/sign/$(DEBUG_DIR)libsign.a
TESTLIBS += $(OCGDIR)/blender/verify/$(DEBUG_DIR)libverify.a
TESTLIBS += $(OCGDIR)/blender/writeblenfile/$(DEBUG_DIR)libwriteblenfile.a
TESTLIBS += $(OCGDIR)/blender/writestreamglue/$(DEBUG_DIR)libwritestreamglue.a
TESTLIBS += $(OCGDIR)/blender/readstreamglue/$(DEBUG_DIR)libreadstreamglue.a
TESTLIBS += $(NAN_BLENKEY)/lib/libblenkey.a
TESTLIBS += $(OCGDIR)/blender/readblenfile/$(DEBUG_DIR)libreadblenfile.a

ifeq ($(OS),$(findstring $(OS), "linux freebsd"))
    TESTLIBS += -lz
    TESTLIBS += $(NAN_OPENSSL)/lib/libcrypto.a
endif

ifeq ($(OS),windows)
    TESTLIBS += ws2_32.lib 
    TESTLIBS += $(NAN_ZLIB)/lib/libz.a
    TESTLIBS += $(NAN_OPENSSL)/lib/libeay32.lib
    TESTLIBS += kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
    TESTLIBS += advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
    TESTLIBS += winmm.lib opengl32.lib glu32.lib largeint.lib dxguid.lib
endif


$(DIR)/readwritetest: $(OBJS)
	$(CC) $(LDFLAGS) -o $(DIR)/readwritetest $(OBJS) $(TESTLIBS)

clean::
	$(RM) $(DIR)/readwritetest

test:: all
	$(DIR)/readwritetest $(NAN_TEST_VERBOSITY)
