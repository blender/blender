# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
#
# Contributor(s): Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	include
)

set(INC_SYS

)

set(SRC
	src/gtest.cc
	src/gtest-death-test.cc
	src/gtest-filepath.cc
	src/gtest-port.cc
	src/gtest-printers.cc
	src/gtest-test-part.cc
	src/gtest-typed-test.cc

	src/gtest-internal-inl.h
	include/gtest/gtest-death-test.h
	include/gtest/gtest.h
	include/gtest/gtest-message.h
	include/gtest/gtest-param-test.h
	include/gtest/gtest_pred_impl.h
	include/gtest/gtest-printers.h
	include/gtest/gtest_prod.h
	include/gtest/gtest-spi.h
	include/gtest/gtest-test-part.h
	include/gtest/gtest-typed-test.h
	include/gtest/internal/gtest-death-test-internal.h
	include/gtest/internal/gtest-filepath.h
	include/gtest/internal/gtest-internal.h
	include/gtest/internal/gtest-linked_ptr.h
	include/gtest/internal/gtest-param-util-generated.h
	include/gtest/internal/gtest-param-util.h
	include/gtest/internal/gtest-port.h
	include/gtest/internal/gtest-string.h
	include/gtest/internal/gtest-tuple.h
	include/gtest/internal/gtest-type-util.h
)

blender_add_lib(extern_gtest "${SRC}" "${INC}" "${INC_SYS}")
