def importDataStructures():
    from . struct import ANStruct

    from . meshes.mesh_data import MeshData
    from . lists.clist import CList
    from . lists.polygon_indices_list import PolygonIndicesList
    from . lists.base_lists import (
        Vector3DList, Matrix4x4List, EdgeIndicesList, EulerList, BooleanList,
        FloatList, DoubleList, LongList, IntegerList, UShortList, CharList,
        QuaternionList, UIntegerList, ShortList, UShortList)

    from . splines.base_spline import Spline
    from . splines.poly_spline import PolySpline
    from . splines.bezier_spline import BezierSpline
    from . default_lists.c_default_list import CDefaultList
    from . interpolation import Interpolation
    from . falloffs.falloff_base import Falloff, BaseFalloff, CompoundFalloff

    from . sounds.sound import Sound
    from . sounds.average_sound import AverageSound
    from . sounds.spectrum_sound import SpectrumSound

    return locals()

dataStructures = importDataStructures()
__all__ = list(dataStructures.keys())
globals().update(dataStructures)
