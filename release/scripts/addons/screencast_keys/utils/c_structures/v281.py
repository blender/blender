from ctypes import (
    c_void_p, c_char, c_short, c_int, c_int8,
    addressof, cast, pointer,
    Structure,
    POINTER,
)


class eWM_EventHandlerType:
    WM_HANDLER_TYPE_UI = 2
    WM_HANDLER_TYPE_OP = 3


class Link(Structure):
    """Defined in source/blender/makesdna/DNA_listBase.h"""

Link._fields_ = [
    ("next", POINTER(Link)),      # struct Link
    ("prev", POINTER(Link)),      # struct Link
]


class ListBase(Structure):
    """Defined in source/blender/makesdna/DNA_listBase.h"""

    def remove(self, vlink):
        """Ref: BLI_remlink"""

        link = vlink
        if not vlink:
            return

        if link.next:
            link.next.contents.prev = link.prev
        if link.prev:
            link.prev.contents.next = link.next

        if self.last == addressof(link):
            self.last = cast(link.prev, c_void_p)
        if self.first == addressof(link):
            self.first = cast(link.next, c_void_p)

    def find(self, number):
        """Ref: BLI_findlink"""

        link = None
        if number >= 0:
            link = cast(c_void_p(self.first), POINTER(Link))
            while link and number != 0:
                number -= 1
                link = link.contents.next
        return link.contents if link else None

    def insert_after(self, vprevlink, vnewlink):
        """Ref: BLI_insertlinkafter"""

        prevlink = vprevlink
        newlink = vnewlink

        if not newlink:
            return

        def gen_ptr(link):
            if isinstance(link, (int, type(None))):
                return cast(c_void_p(link), POINTER(Link))
            else:
                return pointer(link)

        if not self.first:
            self.first = self.last = addressof(newlink)
            return

        if not prevlink:
            newlink.prev = None
            newlink.next = gen_ptr(self.first)
            newlink.next.contents.prev = gen_ptr(newlink)
            self.first = addressof(newlink)
            return

        if self.last == addressof(prevlink):
            self.last = addressof(newlink)

        newlink.next = prevlink.next
        newlink.prev = gen_ptr(prevlink)
        prevlink.next = gen_ptr(newlink)
        if newlink.next:
            newlink.next.prev = gen_ptr(newlink)

ListBase._fields_ = [
    ("first", c_void_p),
    ("last", c_void_p),
]


class ScrAreaMap(Structure):
    """Defined in source/blender/makesdna/DNA_screen_types.h"""

ScrAreaMap._fields_ = [
    ("vertbase", ListBase),
    ("edgebase", ListBase),
    ("areabase", ListBase),
]


class wmWindow(Structure):
    """Defined in source/blender/makesdna/DNA_windowmanager_types.h"""

wmWindow._fields_ = [
    ("next", POINTER(wmWindow)),
    ("prev", POINTER(wmWindow)),

    ("ghostwin", c_void_p),
    ("gpuctx", c_void_p),

    ("parent", POINTER(wmWindow)),

    ("scene", c_void_p),                    # struct Scene
    ("new_scene", c_void_p),                # struct Scene
    ("view_layer_name", c_char*64),

    ("workspace_hook", c_void_p),           # struct WorkSpaceInstanceHook

    ("global_areas", ScrAreaMap),

    ("screen", c_void_p),                   # struct bScreen

    ("posx", c_short),
    ("posy", c_short),
    ("sizex", c_short),
    ("sizey", c_short),
    ("windowstate", c_short),
    ("monitor", c_short),
    ("active", c_short),
    ("cursor", c_short),
    ("lastcursor", c_short),
    ("modalcursor", c_short),
    ("grabcursor", c_short),
    ("addmousemove", c_short),

    ("winid", c_int),

    ("lock_pie_event", c_short),
    ("last_pie_event", c_short),

    ("eventstate", c_void_p),        # struct wmEvent

    ("tweak", c_void_p),             # struct wmGesture
    ("ime_data", c_void_p),          # struct wmIMEData

    ("queue", ListBase),
    ("handlers", ListBase),
    ("modalhandlers", ListBase),

    ("gesture", ListBase),

    ("stereo3d_format", c_void_p),   # struct Stereo3dFormat

    ("drawcalls", ListBase),

    ("cursor_keymap_status", c_void_p),
]


class wmOperator(Structure):
    """Defined in source/blender/makesdna/DNA_windowmanager_types.h"""

wmOperator._fields_ = [
    ("next", POINTER(wmOperator)),
    ("prev", POINTER(wmOperator)),

    ("idname", c_char*64),
    ("properties", c_void_p),               # IDProperty

    ("type", c_void_p),                     # struct wmOperatorType
    ("customdata", c_void_p),
    ("py_instance", c_void_p),

    ("ptr", c_void_p),                      # struct PointerRNA
    ("reports", c_void_p),                  # struct ReportList

    ("macro", ListBase),
    ("opm", POINTER(wmOperator)),
    ("layout", c_void_p),                   # struct uiLayout
    ("flag", c_short),
    ("_pad", c_char*6),
]


class wmEventHandler(Structure):
    """Defined in source/blender/windowmanager/wm_event_system.h"""

wmEventHandler._fields_ = [
    # from struct wmEventHandler
    ("next", POINTER(wmEventHandler)),
    ("prev", POINTER(wmEventHandler)),

    ("type", c_int8),                       # enum eWM_EventHandlerType
    ("flag", c_char),

    ("poll", c_void_p),                     # (*EventHandlerPoll)


    # from struct wmEventHandler_Op
    ("op", POINTER(wmOperator)),
]
