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
# Make a little test for the sys_types replacement.
#

DIR = $(OCGDIR)/blender/deflate/test
ALLTARGETS = $(DIR)/stubbed_test

include nan_compile.mk

CFLAGS += -funsigned-char

# this module's header
CPPFLAGS += -I../..

# external headers
CPPFLAGS += -I../../../readstreamglue
CPPFLAGS += -I../../../writestreamglue

#################################################################
# libs
TESTLIBS  = $(OCGDIR)/blender/deflate/$(DEBUG_DIR)libdeflate.a
TESTLIBS += $(OCGDIR)/blender/writestreamglueSTUB/$(DEBUG_DIR)libwritestreamglueSTUB.a
TESTLIBS += $(OCGDIR)/kernel/gen_messaging/$(DEBUG_DIR)libgen_messaging.a

ifeq ($(OS),$(findstring $(OS), "freebsd linux"))
    TESTLIBS += -lz
endif

ifeq ($(OS),windows)
    TESTLIBS += ws2_32.lib 
    TESTLIBS += $(NAN_ZLIB)/lib/libz.a
endif

# There's hardly anything to debug here, so forget about it.
$(DIR)/stubbed_test: $(OBJS)
	$(CC) $(OBJS) $(TESTLIBS) -o $(DIR)/stubbed_test

clean::
	$(RM) $(DIR)/stubbed_test

test:: $(DIR)/stubbed_test
	$(DIR)/stubbed_test $(NAN_TEST_VERBOSITY)
