# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Uses the NatureUP1D predicate to select the lines
#             of a given type (among Nature.SILHOUETTE, Nature.CREASE, Nature.SUGGESTIVE_CONTOURS,
#             Nature.BORDERS).
#             The suggestive contours must have been enabled in the
#             options dialog to appear in the View Map.

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    TrueUP1D,
    pyNatureUP1D,
)
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
)
from freestyle.types import Operators, Nature


Operators.select(pyNatureUP1D(Nature.SILHOUETTE))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(pyNatureUP1D(Nature.SILHOUETTE)))
shaders_list = [
    IncreasingThicknessShader(3, 10),
    IncreasingColorShader(0.0, 0.0, 0.0, 1, 0.8, 0, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
