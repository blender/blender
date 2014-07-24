# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

"""
Chaining iterators used for the chaining operation to construct long
strokes by concatenating feature edges according to selected chaining
rules.  Also intended to be a collection of examples for defining
chaining iterators in Python
"""

__all__ = (
    "ChainPredicateIterator",
    "ChainSilhouetteIterator",
    "pyChainSilhouetteIterator",
    "pyChainSilhouetteGenericIterator",
    "pyExternalContourChainingIterator",
    "pySketchyChainSilhouetteIterator",
    "pySketchyChainingIterator",
    "pyFillOcclusionsRelativeChainingIterator",
    "pyFillOcclusionsAbsoluteChainingIterator",
    "pyFillOcclusionsAbsoluteAndRelativeChainingIterator",
    "pyFillQi0AbsoluteAndRelativeChainingIterator",
    "pyNoIdChainSilhouetteIterator",
    )


# module members
from _freestyle import (
    ChainPredicateIterator,
    ChainSilhouetteIterator,
    )

# constructs for predicate definition in Python
from freestyle.types import (
    AdjacencyIterator,
    ChainingIterator,
    Nature,
    TVertex,
    )
from freestyle.predicates import (
    ExternalContourUP1D,
    )
from freestyle.utils import (
    ContextFunctions as CF,
    get_chain_length,
    find_matching_vertex,
    )

import bpy


NATURES = (
    Nature.SILHOUETTE,
    Nature.BORDER,
    Nature.CREASE,
    Nature.MATERIAL_BOUNDARY,
    Nature.EDGE_MARK,
    Nature.SUGGESTIVE_CONTOUR,
    Nature.VALLEY,
    Nature.RIDGE
    )


def nature_in_preceding(nature, index):
    """Returns True if given nature appears before index, else False."""
    return any(nature & nat for nat in NATURES[:index])


class pyChainSilhouetteIterator(ChainingIterator):
    """Natural chaining iterator

    Follows the edges of the same nature following the topology of
    objects, with decreasing priority for silhouettes, then borders,
    then suggestive contours, then all other edge types.  A ViewEdge
    is only chained once.
    """
    def __init__(self, stayInSelection=True):
        ChainingIterator.__init__(self, stayInSelection, True, None, True)

    def init(self):
        pass

    def traverse(self, iter):
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            return find_matching_vertex(mate.id, it)
        ## case of NonTVertex
        winner = None
        for i, nat in enumerate(NATURES):
            if (nat & self.current_edge.nature):
                for ve in it:
                    ve_nat = ve.nature
                    if (ve_nat & nat):
                        # search for matches in previous natures. if match -> break
                        if nat != ve_nat and nature_in_preceding(ve_nat, index=i):
                            break
                        # a second match must be an error
                        if winner is not None:
                            return None
                        # assign winner
                        winner = ve
                return winner


class pyChainSilhouetteGenericIterator(ChainingIterator):
    """Natural chaining iterator

    Follows the edges of the same nature following the topology of
    objects, with decreasing priority for silhouettes, then borders,
    then suggestive contours, then all other edge types.

    :arg stayInSelection: True if it is allowed to go out of the selection
    :type stayInSelection: bool
    :arg stayInUnvisited: May the same ViewEdge be chained twice
    :type stayInUnvisited: bool
    """

    def __init__(self, stayInSelection=True, stayInUnvisited=True):
        ChainingIterator.__init__(self, stayInSelection, stayInUnvisited, None, True)

    def init(self):
        pass

    def traverse(self, iter):
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            return find_matching_vertex(mate.id, it)
        ## case of NonTVertex
        winner = None
        for i, nat in enumerate(NATURES):
            if (nat & self.current_edge.nature):
                for ve in it:
                    ve_nat = ve.nature
                    if ve.id == self.current_edge.id:
                        continue
                    if (ve_nat & nat):
                        if nat != ve_nat and nature_in_preceding(ve_nat, index=i):
                            break

                        if winner is not None:
                            return None

                        winner = ve
                return winner
        return None


class pyExternalContourChainingIterator(ChainingIterator):
    """Chains by external contour"""

    def __init__(self):
        ChainingIterator.__init__(self, False, True, None, True)
        self.ExternalContour = ExternalContourUP1D()

    def init(self):
        self._nEdges = 0

    def checkViewEdge(self, ve, orientation):
        vertex = (ve.first_viewvertex if orientation else
                  ve.last_viewvertex)

        it = AdjacencyIterator(vertex, True, True)
        result = any(self.ExternalContour(ave) for ave in it)
        # report if there is no result (that's bad)
        if not result and bpy.app.debug_freestyle:
            print("pyExternalContourChainingIterator : didn't find next edge")

        return result

    def traverse(self, iter):
        winner = None
        self._nEdges += 1

        it = AdjacencyIterator(iter)
        time_stamp = CF.get_time_stamp()

        for ve in it:
            if self.ExternalContour(ve) and ve.time_stamp == time_stamp:
                winner = ve

        if winner is None:
            it = AdjacencyIterator(iter)
            for ve in it:
                if self.checkViewEdge(ve, not it.is_incoming):
                    winner = ve

        return winner


class pySketchyChainSilhouetteIterator(ChainingIterator):
    """Natural chaining iterator with a sketchy multiple touch

    Chains the same ViewEdge multiple times to achieve a sketchy effect.

    :arg rounds: Number of times every Viewedge is chained.
    :type rounds: int
    :arg stayInSelection: if False, edges outside of the selection can be chained.
    :type stayInSelection: bool
    """

    def __init__(self, nRounds=3,stayInSelection=True):
        ChainingIterator.__init__(self, stayInSelection, False, None, True)
        self._timeStamp = CF.get_time_stamp() + nRounds
        self._nRounds = nRounds

    def init(self):
        self._timeStamp = CF.get_time_stamp() + self._nRounds

    # keeping this local saves passing a reference to 'self' around
    def make_sketchy(self, ve):
        """
        Creates the skeychy effect by causing the chain to run from
        the start again. (loop over itself again)
        """
        if ve is None:
            ve = self.current_edge
        if ve.chaining_time_stamp == self._timeStamp:
            return None
        return ve

    def traverse(self, iter):
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            return self.make_sketchy(find_matching_vertex(mate.id, it))
        ## case of NonTVertex
        winner = None
        for i, nat in enumerate(NATURES):
            if (nat & self.current_edge.nature):
                for ve in it:
                    if ve.id == self.current_edge.id:
                        continue
                    ve_nat = ve.nature
                    if (ve_nat & nat):
                        if nat != ve_nat and nature_in_preceding(ve_nat, i):
                            break

                        if winner is not None:
                            return self.make_sketchy(None)

                        winner = ve
                break
        return self.make_sketchy(winner)


class pySketchyChainingIterator(ChainingIterator):
    """Chaining iterator designed for sketchy style

    It chaines the same ViewEdge several times in order to produce
    multiple strokes per ViewEdge.
    """
    def __init__(self, nRounds=3, stayInSelection=True):
        ChainingIterator.__init__(self, stayInSelection, False, None, True)
        self._timeStamp = CF.get_time_stamp() + nRounds
        self._nRounds = nRounds
        self.t = False

    def init(self):
        self._timeStamp = CF.get_time_stamp() + self._nRounds

    def traverse(self, iter):
        winner = None
        found = False

        for ve in AdjacencyIterator(iter):
            if self.current_edge.id == ve.id:
                found = True
                continue
            winner = ve

        if not found:
            # This is a fatal error condition: self.current_edge must be found
            # among the edges seen by the AdjacencyIterator [bug #35695].
            if bpy.app.debug_freestyle:
                print('pySketchyChainingIterator: current edge not found')
            return None

        if winner is None:
            winner = self.current_edge
        if winner.chaining_time_stamp == self._timeStamp:
            return None
        return winner


class pyFillOcclusionsRelativeChainingIterator(ChainingIterator):
    """Chaining iterator that fills small occlusions

    :arg percent: The maximul length of the occluded part, expressed
        in a percentage of the total chain length.
    :type percent: float
    """

    def __init__(self, percent):
        ChainingIterator.__init__(self, False, True, None, True)
        self._length = 0.0
        self._percent = float(percent)
        self.timestamp = CF.get_time_stamp()

    def init(self):
        # A chain's length should preferably be evaluated only once.
        # Therefore, the chain length is reset here.
        self._length = 0.0

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            winner = find_matching_vertex(mate.id, it)
            winnerOrientation = not it.is_incoming if not it.is_end else False
        ## case of NonTVertex
        else:
            for nat in NATURES:
                if (self.current_edge.nature & nat):
                    for ve in it:
                        if (ve.nature & nat):
                            if winner is not None:
                                return None
                            winner = ve
                            winnerOrientation = not it.is_incoming
                    break

        # check timestamp to see if this edge was part of the selection
        if winner is not None and winner.time_stamp != self.timestamp:
            # if the edge wasn't part of the selection, let's see
            # whether it's short enough (with respect to self.percent)
            # to be included.
            if self._length == 0.0:
                self._length = get_chain_length(winner, winnerOrientation)

            # check if the gap can be bridged
            connexl = 0.0
            _cit = pyChainSilhouetteGenericIterator(False, False)
            _cit.begin = winner
            _cit.current_edge = winner
            _cit.orientation = winnerOrientation
            _cit.init()

            while (not _cit.is_end) and _cit.object.time_stamp != self.timestamp:
                connexl += _cit.object.length_2d
                _cit.increment()
                if _cit.is_begin: break

            if connexl > self._percent * self._length:
                return None

        return winner


class pyFillOcclusionsAbsoluteChainingIterator(ChainingIterator):
    """Chaining iterator that fills small occlusions

    :arg size: The maximum length of the occluded part in pixels.
    :type size: int
    """
    def __init__(self, length):
        ChainingIterator.__init__(self, False, True, None, True)
        self._length = float(length)
        self.timestamp = CF.get_time_stamp()

    def init(self):
        pass

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            winner = find_matching_vertex(mate.id, it)
            winnerOrientation = not it.is_incoming if not it.is_end else False
        ## case of NonTVertex
        else:
            for nat in NATURES:
                if (self.current_edge.nature & nat):
                    for ve in it:
                        if (ve.nature & nat):
                            if winner is not None:
                                return None
                            winner = ve
                            winnerOrientation = not it.is_incoming
                    break

        if winner is not None and winner.time_stamp != self.timestamp:
            connexl = 0.0
            _cit = pyChainSilhouetteGenericIterator(False, False)
            _cit.begin = winner
            _cit.current_edge = winner
            _cit.orientation = winnerOrientation
            _cit.init()

            while (not _cit.is_end) and _cit.object.time_stamp != self.timestamp:
                connexl += _cit.object.length_2d
                _cit.increment()
                if _cit.is_begin: break

            if connexl > self._length:
                return None

        return winner


class pyFillOcclusionsAbsoluteAndRelativeChainingIterator(ChainingIterator):
    """Chaining iterator that fills small occlusions regardless of the
    selection

    :arg percent: The maximul length of the occluded part as a
        percentage of the total chain length.
    :type percent: float
    """
    def __init__(self, percent, l):
        ChainingIterator.__init__(self, False, True, None, True)
        self._length = 0.0
        self._absLength = l
        self._percent = float(percent)

    def init(self):
        # each time we're evaluating a chain length
        # we try to do it once. Thus we reinit
        # the chain length here:
        self._length = 0.0

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            winner = find_matching_vertex(mate.id, it)
            winnerOrientation = not it.is_incoming if not it.is_end else False
        ## case of NonTVertex
        else:
            for nat in NATURES:
                if (self.current_edge.nature & nat):
                    for ve in it:
                        if (ve.nature & nat):
                            if winner is not None:
                                return None
                            winner = ve
                            winnerOrientation = not it.is_incoming
                    break

        if winner is not None and winner.time_stamp != CF.get_time_stamp():

                if self._length == 0.0:
                    self._length = get_chain_length(winner, winnerOrientation)

                connexl = 0.0
                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while (not _cit.is_end) and _cit.object.time_stamp != CF.get_time_stamp():
                    connexl += _cit.object.length_2d
                    _cit.increment()
                    if _cit.is_begin: break

                if (connexl > self._percent * self._length) or (connexl > self._absLength):
                    return None
        return winner


class pyFillQi0AbsoluteAndRelativeChainingIterator(ChainingIterator):
    """Chaining iterator that fills small occlusions regardless of the
    selection

    :arg percent: The maximul length of the occluded part as a
        percentage of the total chain length.
    :type percent: float
    """
    def __init__(self, percent, l):
        ChainingIterator.__init__(self, False, True, None, True)
        self._length = 0.0
        self._absLength = l
        self._percent = percent

    def init(self):
        # A chain's length should preverably be evaluated only once.
        # Therefore, the chain length is reset here.
        self._length = 0.0

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        it = AdjacencyIterator(iter)
        ## case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            mate = vertex.get_mate(self.current_edge)
            winner = find_matching_vertex(mate.id, it)
            winnerOrientation = not it.is_incoming if not it.is_end else False
        ## case of NonTVertex
        else:
            for nat in NATURES:
                if (self.current_edge.nature & nat):
                    for ve in it:
                        if (ve.nature & nat):
                            if winner is not None:
                                return None
                            winner = ve
                            winnerOrientation = not it.is_incoming
                    break

        if winner is not None and winner.qi:


                if self._length == 0.0:
                    self._length = get_chain_length(winner, winnerOrientation)

                connexl = 0
                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while (not _cit.is_end) and _cit.object.qi != 0:
                    connexl += _cit.object.length_2d
                    _cit.increment()
                    if _cit.is_begin: break
                if (connexl > self._percent * self._length) or (connexl > self._absLength):
                    return None
        return winner


class pyNoIdChainSilhouetteIterator(ChainingIterator):
    """Natural chaining iterator

    Follows the edges of the same nature following the topology of
    objects, with decreasing priority for silhouettes, then borders,
    then suggestive contours, then all other edge types.  It won't
    chain the same ViewEdge twice.

    :arg stayInSelection: True if it is allowed to go out of the selection
    :type stayInSelection: bool
    """

    def __init__(self, stayInSelection=True):
        ChainingIterator.__init__(self, stayInSelection, True, None, True)

    def init(self):
        pass

    def traverse(self, iter):
        winner = None
        it = AdjacencyIterator(iter)
        # case of TVertex
        vertex = self.next_vertex
        if type(vertex) is TVertex:
            for ve in it:
                # case one
                vA = self.current_edge.last_fedge.second_svertex
                vB = ve.first_fedge.first_svertex
                if vA.id.first == vB.id.first:
                    return ve
                # case two
                vA = self.current_edge.first_fedge.first_svertex
                vB = ve.last_fedge.second_svertex
                if vA.id.first == vB.id.first:
                    return ve
                # case three
                vA = self.current_edge.last_fedge.second_svertex
                vB = ve.last_fedge.second_svertex
                if vA.id.first == vB.id.first:
                    return ve
                # case four
                vA = self.current_edge.first_fedge.first_svertex
                vB = ve.first_fedge.first_svertex
                if vA.id.first == vB.id.first:
                    return ve
            return None
        ## case of NonTVertex
        else:
            for i, nat in enumerate(NATURES):
                 if (nat & self.current_edge.nature):
                    for ve in it:
                        ve_nat = ve.nature
                        if (ve_nat & nat):
                            if (nat != ve_nat) and any(n & ve_nat for n in NATURES[:i]):
                                break

                            if winner is not None:
                                return

                            winner = ve
                    return winner
            return None
