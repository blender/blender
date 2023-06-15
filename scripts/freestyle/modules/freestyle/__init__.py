# SPDX-FileCopyrightText: 2009-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module provides data types of view map components (0D and 1D
elements), base classes for defining line stylization rules
(predicates, functions, chaining iterators, and stroke shaders),
as well as helper functions for style module writing.
"""


# module members
from . import chainingiterators, functions, predicates, shaders, types, utils
