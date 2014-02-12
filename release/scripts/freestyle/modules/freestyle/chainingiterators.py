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
from freestyle.utils import ContextFunctions as CF

import bpy


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
        winner = None
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for i in range(len(natures)):
                currentNature = self.current_edge.nature
                if (natures[i] & currentNature) != 0:
                    count=0
                    while not it.is_end:
                        visitNext = 0
                        oNature = it.object.nature
                        if (oNature & natures[i]) != 0:
                            if natures[i] != oNature:
                                for j in range(i):
                                    if (natures[j] & oNature) != 0:
                                        visitNext = 1
                                        break
                                if visitNext != 0:
                                    break
                            count = count+1
                            winner = it.object
                        it.increment()
                    if count != 1:
                        winner = None
                    break
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
        winner = None
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for i in range(len(natures)):
                currentNature = self.current_edge.nature
                if (natures[i] & currentNature) != 0:
                    count=0
                    while not it.is_end:
                        visitNext = 0
                        oNature = it.object.nature
                        ve = it.object
                        if ve.id == self.current_edge.id:
                            it.increment()
                            continue
                        if (oNature & natures[i]) != 0:
                            if natures[i] != oNature:
                                for j in range(i):
                                    if (natures[j] & oNature) != 0:
                                        visitNext = 1
                                        break
                                if visitNext != 0:
                                    break
                            count = count+1
                            winner = ve
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        return winner


class pyExternalContourChainingIterator(ChainingIterator):
    """Chains by external contour"""

    def __init__(self):
        ChainingIterator.__init__(self, False, True, None, True)
        self._isExternalContour = ExternalContourUP1D()

    def init(self):
        self._nEdges = 0
        self._isInSelection = 1

    def checkViewEdge(self, ve, orientation):
        if orientation != 0:
            vertex = ve.second_svertex()
        else:
            vertex = ve.first_svertex()
        it = AdjacencyIterator(vertex,1,1)
        while not it.is_end:
            ave = it.object
            if self._isExternalContour(ave):
                return True
            it.increment()
        if bpy.app.debug_freestyle:
            print("pyExternalContourChainingIterator : didn't find next edge")
        return False

    def traverse(self, iter):
        winner = None
        it = AdjacencyIterator(iter)
        while not it.is_end:
            ve = it.object
            if self._isExternalContour(ve):
                if ve.time_stamp == CF.get_time_stamp():
                    winner = ve
            it.increment()

        self._nEdges = self._nEdges+1
        if winner is None:
            orient = 1
            it = AdjacencyIterator(iter)
            while not it.is_end:
                ve = it.object
                if it.is_incoming:
                    orient = 0
                good = self.checkViewEdge(ve,orient)
                if good != 0:
                    winner = ve
                it.increment()
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
        self._timeStamp = CF.get_time_stamp()+nRounds
        self._nRounds = nRounds

    def init(self):
        self._timeStamp = CF.get_time_stamp()+self._nRounds

    def traverse(self, iter):
        winner = None
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for i in range(len(natures)):
                currentNature = self.current_edge.nature
                if (natures[i] & currentNature) != 0:
                    count=0
                    while not it.is_end:
                        visitNext = 0
                        oNature = it.object.nature
                        ve = it.object
                        if ve.id == self.current_edge.id:
                            it.increment()
                            continue
                        if (oNature & natures[i]) != 0:
                            if (natures[i] != oNature) != 0:
                                for j in range(i):
                                    if (natures[j] & oNature) != 0:
                                        visitNext = 1
                                        break
                                if visitNext != 0:
                                    break
                            count = count+1
                            winner = ve
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        if winner is None:
            winner = self.current_edge
        if winner.chaining_time_stamp == self._timeStamp:
            winner = None
        return winner


class pySketchyChainingIterator(ChainingIterator):
    """Chaining iterator designed for sketchy style

    It chaines the same ViewEdge several times in order to produce
    multiple strokes per ViewEdge.
    """
    def __init__(self, nRounds=3, stayInSelection=True):
        ChainingIterator.__init__(self, stayInSelection, False, None, True)
        self._timeStamp = CF.get_time_stamp()+nRounds
        self._nRounds = nRounds

    def init(self):
        self._timeStamp = CF.get_time_stamp()+self._nRounds

    def traverse(self, iter):
        winner = None
        found = False
        it = AdjacencyIterator(iter)
        while not it.is_end:
            ve = it.object
            if ve.id == self.current_edge.id:
                found = True
                it.increment()
                continue
            winner = ve
            it.increment()
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
        self._length = 0
        self._percent = float(percent)

    def init(self):
        # A chain's length should preferably be evaluated only once.
        # Therefore, the chain length is reset here.
        self._length = 0

    def traverse(self, iter):
        winner = None

        winnerOrientation = False
        winnerOrientation = 0
        #print(self.current_edge.id.first, self.current_edge.id.second)
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    winnerOrientation = not it.is_incoming
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for nat in natures:
                if (self.current_edge.nature & nat) != 0:
                    count=0
                    while not it.is_end:
                        ve = it.object
                        if (ve.nature & nat) != 0:
                            count = count+1
                            winner = ve
                            winnerOrientation = not it.is_incoming
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        if winner is not None:
            # check whether this edge was part of the selection
            if winner.time_stamp != CF.get_time_stamp():
                #print("---", winner.id.first, winner.id.second)
                # if not, let's check whether it's short enough with
                # respect to the chain made without staying in the selection
                #------------------------------------------------------------
                # Did we compute the prospective chain length already ?
                if self._length == 0:
                    #if not, let's do it
                    _it = pyChainSilhouetteGenericIterator(False, False)
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    _it.init()
                    while not _it.is_end:
                        ve = _it.object
                        #print("--------", ve.id.first, ve.id.second)
                        self._length = self._length + ve.length_2d
                        _it.increment()
                        if _it.is_begin:
                            break;
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    if not _it.is_begin:
                        _it.decrement()
                        while (not _it.is_end) and (not _it.is_begin):
                            ve = _it.object
                            #print("--------", ve.id.first, ve.id.second)
                            self._length = self._length + ve.length_2d
                            _it.decrement()

                # let's do the comparison:
                # nw let's compute the length of this connex non selected part:
                connexl = 0

                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while _cit.is_end == 0 and _cit.object.time_stamp != CF.get_time_stamp():
                    ve = _cit.object
                    #print("-------- --------", ve.id.first, ve.id.second)
                    connexl = connexl + ve.length_2d
                    _cit.increment()
                if connexl > self._percent * self._length:
                    winner = None
        return winner


class pyFillOcclusionsAbsoluteChainingIterator(ChainingIterator):
    """Chaining iterator that fills small occlusions

    :arg size: The maximum length of the occluded part in pixels.
    :type size: int
    """
    def __init__(self, length):
        ChainingIterator.__init__(self, False, True, None, True)
        self._length = float(length)

    def init(self):
        pass

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        #print(self.current_edge.id.first, self.current_edge.id.second)
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    winnerOrientation = not it.is_incoming
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for nat in natures:
                if (self.current_edge.nature & nat) != 0:
                    count=0
                    while not it.is_end:
                        ve = it.object
                        if (ve.nature & nat) != 0:
                            count = count+1
                            winner = ve
                            winnerOrientation = not it.is_incoming
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        if winner is not None:
            # check whether this edge was part of the selection
            if winner.time_stamp != CF.get_time_stamp():
                #print("---", winner.id.first, winner.id.second)
                # nw let's compute the length of this connex non selected part:
                connexl = 0
                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while _cit.is_end == 0 and _cit.object.time_stamp != CF.get_time_stamp():
                    ve = _cit.object
                    #print("-------- --------", ve.id.first, ve.id.second)
                    connexl = connexl + ve.length_2d
                    _cit.increment()
                if connexl > self._length:
                    winner = None
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
        self._length = 0
        self._absLength = l
        self._percent = float(percent)

    def init(self):
        # each time we're evaluating a chain length
        # we try to do it once. Thus we reinit
        # the chain length here:
        self._length = 0

    def traverse(self, iter):
        winner = None
        winnerOrientation = False
        #print(self.current_edge.id.first, self.current_edge.id.second)
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    winnerOrientation = not it.is_incoming
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for nat in natures:
                if (self.current_edge.nature & nat) != 0:
                    count=0
                    while not it.is_end:
                        ve = it.object
                        if (ve.nature & nat) != 0:
                            count = count+1
                            winner = ve
                            winnerOrientation = not it.is_incoming
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        if winner is not None:
            # check whether this edge was part of the selection
            if winner.time_stamp != CF.get_time_stamp():
                #print("---", winner.id.first, winner.id.second)
                # if not, let's check whether it's short enough with
                # respect to the chain made without staying in the selection
                #------------------------------------------------------------
                # Did we compute the prospective chain length already ?
                if self._length == 0:
                    #if not, let's do it
                    _it = pyChainSilhouetteGenericIterator(False, False)
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    _it.init()
                    while not _it.is_end:
                        ve = _it.object
                        #print("--------", ve.id.first, ve.id.second)
                        self._length = self._length + ve.length_2d
                        _it.increment()
                        if _it.is_begin:
                            break;
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    if not _it.is_begin:
                        _it.decrement()
                        while (not _it.is_end) and (not _it.is_begin):
                            ve = _it.object
                            #print("--------", ve.id.first, ve.id.second)
                            self._length = self._length + ve.length_2d
                            _it.decrement()

                # let's do the comparison:
                # nw let's compute the length of this connex non selected part:
                connexl = 0
                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while _cit.is_end == 0 and _cit.object.time_stamp != CF.get_time_stamp():
                    ve = _cit.object
                    #print("-------- --------", ve.id.first, ve.id.second)
                    connexl = connexl + ve.length_2d
                    _cit.increment()
                if (connexl > self._percent * self._length) or (connexl > self._absLength):
                    winner = None
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
        self._length = 0
        self._absLength = l
        self._percent = float(percent)

    def init(self):
        # A chain's length should preverably be evaluated only once.
        # Therefore, the chain length is reset here.
        self._length = 0

    def traverse(self, iter):
        winner = None
        winnerOrientation = False

        #print(self.current_edge.id.first, self.current_edge.id.second)
        it = AdjacencyIterator(iter)
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                if ve.id == mateVE.id:
                    winner = ve
                    winnerOrientation = not it.is_incoming
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for nat in natures:
                if (self.current_edge.nature & nat) != 0:
                    count=0
                    while not it.is_end:
                        ve = it.object
                        if (ve.nature & nat) != 0:
                            count = count+1
                            winner = ve
                            winnerOrientation = not it.is_incoming
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        if winner is not None:
            # check whether this edge was part of the selection
            if winner.qi != 0:
                #print("---", winner.id.first, winner.id.second)
                # if not, let's check whether it's short enough with
                # respect to the chain made without staying in the selection
                #------------------------------------------------------------
                # Did we compute the prospective chain length already ?
                if self._length == 0:
                    #if not, let's do it
                    _it = pyChainSilhouetteGenericIterator(False, False)
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    _it.init()
                    while not _it.is_end:
                        ve = _it.object
                        #print("--------", ve.id.first, ve.id.second)
                        self._length = self._length + ve.length_2d
                        _it.increment()
                        if _it.is_begin:
                            break;
                    _it.begin = winner
                    _it.current_edge = winner
                    _it.orientation = winnerOrientation
                    if not _it.is_begin:
                        _it.decrement()
                        while (not _it.is_end) and (not _it.is_begin):
                            ve = _it.object
                            #print("--------", ve.id.first, ve.id.second)
                            self._length = self._length + ve.length_2d
                            _it.decrement()

                # let's do the comparison:
                # nw let's compute the length of this connex non selected part:
                connexl = 0
                _cit = pyChainSilhouetteGenericIterator(False, False)
                _cit.begin = winner
                _cit.current_edge = winner
                _cit.orientation = winnerOrientation
                _cit.init()
                while not _cit.is_end and _cit.object.qi != 0:
                    ve = _cit.object
                    #print("-------- --------", ve.id.first, ve.id.second)
                    connexl = connexl + ve.length_2d
                    _cit.increment()
                if (connexl > self._percent * self._length) or (connexl > self._absLength):
                    winner = None
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
        tvertex = self.next_vertex
        if type(tvertex) is TVertex:
            mateVE = tvertex.get_mate(self.current_edge)
            while not it.is_end:
                ve = it.object
                feB = self.current_edge.last_fedge
                feA = ve.first_fedge
                vB = feB.second_svertex
                vA = feA.first_svertex
                if vA.id.first == vB.id.first:
                    winner = ve
                    break
                feA = self.current_edge.first_fedge
                feB = ve.last_fedge
                vB = feB.second_svertex
                vA = feA.first_svertex
                if vA.id.first == vB.id.first:
                    winner = ve
                    break
                feA = self.current_edge.last_fedge
                feB = ve.last_fedge
                vB = feB.second_svertex
                vA = feA.second_svertex
                if vA.id.first == vB.id.first:
                    winner = ve
                    break
                feA = self.current_edge.first_fedge
                feB = ve.first_fedge
                vB = feB.first_svertex
                vA = feA.first_svertex
                if vA.id.first == vB.id.first:
                    winner = ve
                    break
                it.increment()
        else:
            ## case of NonTVertex
            natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.MATERIAL_BOUNDARY,Nature.EDGE_MARK,
                       Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
            for i in range(len(natures)):
                currentNature = self.current_edge.nature
                if (natures[i] & currentNature) != 0:
                    count=0
                    while not it.is_end:
                        visitNext = 0
                        oNature = it.object.nature
                        if (oNature & natures[i]) != 0:
                            if natures[i] != oNature:
                                for j in range(i):
                                    if (natures[j] & oNature) != 0:
                                        visitNext = 1
                                        break
                                if visitNext != 0:
                                    break
                            count = count+1
                            winner = it.object
                        it.increment()
                    if count != 1:
                        winner = None
                    break
        return winner
