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

# <pep8 compliant>

"""
This module translates a python like XML representation into XML
or simple python blender/ui function calls.

    sometag(arg=10) [
        another()
        another(key="value")
    ]

# converts into ...

    <sometag arg="10">
        <another/>
        <another key="value" />
    </sometag>

"""

TAG, ARGS, CHILDREN = range(3)


class ReturnStore(tuple):
    def __getitem__(self, key):

        # single item get's
        if type(key) is ReturnStore:
            key = (key, )

        if type(key) is tuple:
            children = self[CHILDREN]
            if children:
                raise Exception("Only a single __getitem__ is allowed on the ReturnStore")
            else:
                children[:] = key
            return self
        else:
            return tuple.__getitem__(self, key)


class FunctionStore:
    def __call__(self, **kwargs):
        return ReturnStore((self.__class__.__name__, kwargs, []))


def tag_vars(tags, module=__name__):
    return {tag: type(tag, (FunctionStore, ), {"__module__": module})() for tag in tags}


def tag_module(mod_name, tags):
    import sys
    from types import ModuleType
    mod = ModuleType(mod_name)
    sys.modules[mod_name] = mod
    dict_values = tag_vars(tags, mod_name)
    mod.__dict__.update(dict_values)
    return mod


def toxml(py_data, indent="    "):

    if len(py_data) != 1 or type(py_data) != list:
        raise Exception("Expected a list with one member")

    def _to_xml(py_item, xml_node=None):
        if xml_node is None:
            xml_node = newdoc.createElement(py_item[TAG])

        for key, value in py_item[ARGS].items():
            xml_node.setAttribute(key, str(value))

        for py_item_child in py_item[CHILDREN]:
            xml_node.appendChild(_to_xml(py_item_child))

        return xml_node

    def _to_xml_iter(xml_parent, data_ls):
        for py_item in data_ls:
            xml_node = newdoc.createElement(py_item[TAG])

            # ok if its empty
            _to_xml_iter(xml_node, py_item[CHILDREN])

    import xml.dom.minidom
    impl = xml.dom.minidom.getDOMImplementation()
    newdoc = impl.createDocument(None, py_data[0][TAG], None)

    _to_xml(py_data[0], newdoc.documentElement)

    return newdoc.documentElement.toprettyxml(indent="  ")


def fromxml(data):
    def _fromxml_kwargs(xml_node):
        kwargs = {}
        for key, value in xml_node.attributes.items():
            kwargs[key] = value
        return kwargs

    def _fromxml(xml_node):
        py_item = (xml_node.tagName, _fromxml_kwargs(xml_node), [])
        #_fromxml_iter(py_item, xml_node.childNodes)
        for xml_node_child in xml_node.childNodes:
            if xml_node_child.nodeType not in {xml_node_child.TEXT_NODE, xml_node_child.COMMENT_NODE}:
                py_item[CHILDREN].append(_fromxml(xml_node_child))
        return py_item

    import xml.dom.minidom
    xml_doc = xml.dom.minidom.parseString(data)
    return [_fromxml(xml_doc.documentElement)]


def topretty_py(py_data, indent="    "):

    if len(py_data) != 1:
        raise Exception("Expected a list with one member")

    lines = []

    def _to_kwargs(kwargs):
        return ", ".join([("%s=%s" % (key, repr(value))) for key, value in sorted(kwargs.items())])

    def _topretty(py_item, indent_ctx, last):
        if py_item[CHILDREN]:
            lines.append("%s%s(%s) [" % (indent_ctx, py_item[TAG], _to_kwargs(py_item[ARGS])))
            py_item_last = py_item[CHILDREN][-1]
            for py_item_child in py_item[CHILDREN]:
                _topretty(py_item_child, indent_ctx + indent, (py_item_child is py_item_last))
            lines.append("%s]%s" % (indent_ctx, ("" if last else ",")))
        else:
            lines.append("%s%s(%s)%s" % (indent_ctx, py_item[TAG], _to_kwargs(py_item[ARGS]), ("" if last else ",")))

    _topretty(py_data[0], "", True)

    return "\n".join(lines)

if __name__ == "__main__":
    # testing code.

    tag_module("bpyml_test", ("ui", "prop", "row", "column", "active", "separator", "split"))
    from bpyml_test import *

    draw = [
        ui()[
            split()[
                column()[
                    prop(data='context.scene.render', property='use_stamp_time', text='Time'),
                    prop(data='context.scene.render', property='use_stamp_date', text='Date'),
                    prop(data='context.scene.render', property='use_stamp_render_time', text='RenderTime'),
                    prop(data='context.scene.render', property='use_stamp_frame', text='Frame'),
                    prop(data='context.scene.render', property='use_stamp_scene', text='Scene'),
                    prop(data='context.scene.render', property='use_stamp_camera', text='Camera'),
                    prop(data='context.scene.render', property='use_stamp_filename', text='Filename'),
                    prop(data='context.scene.render', property='use_stamp_marker', text='Marker'),
                    prop(data='context.scene.render', property='use_stamp_sequencer_strip', text='Seq. Strip')
                ],
                column()[
                    active(expr='context.scene.render.use_stamp'),
                    prop(data='context.scene.render', property='stamp_foreground', slider=True),
                    prop(data='context.scene.render', property='stamp_background', slider=True),
                    separator(),
                    prop(data='context.scene.render', property='stamp_font_size', text='Font Size')
                ]
            ],
            split(percentage=0.2)[
                prop(data='context.scene.render', property='use_stamp_note', text='Note'),
                row()[
                    active(expr='context.scene.render.use_stamp_note'),
                    prop(data='context.scene.render', property='stamp_note_text', text='')
                ]
            ]
        ]
    ]

    xml_data = toxml(draw)
    print(xml_data)  # xml version

    py_data = fromxml(xml_data)
    print(py_data)  # converted back to py

    xml_data = toxml(py_data)
    print(xml_data)  # again back to xml

    py_data = fromxml(xml_data)  # pretty python version
    print(topretty_py(py_data))
