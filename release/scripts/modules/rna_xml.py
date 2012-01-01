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
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>


import bpy

# easier to read
PRETTY_INTEND = True

invalid_classes = (
    bpy.types.Operator,
    bpy.types.Panel,
    bpy.types.KeyingSet,
    bpy.types.Header,
    )


def build_property_typemap():

    property_typemap = {}

    for attr in dir(bpy.types):
        cls = getattr(bpy.types, attr)
        if issubclass(cls, invalid_classes):
            continue

        properties = cls.bl_rna.properties.keys()
        properties.remove("rna_type")
        property_typemap[attr] = properties

    return property_typemap


def print_ln(data):
    print(data, end="")


def rna2xml(fw=print_ln, ident_val="  "):
    from xml.sax.saxutils import quoteattr
    property_typemap = build_property_typemap()

    def rna2xml_node(ident, value, parent):
        ident_next = ident + ident_val

        # divide into attrs and nodes.
        node_attrs = []
        nodes_items = []
        nodes_lists = []

        value_type = type(value)

        if issubclass(value_type, invalid_classes):
            return

        # XXX, fixme, pointcache has eternal nested pointer to its self.
        if value == parent:
            return

        value_type_name = value_type.__name__
        for prop in property_typemap[value_type_name]:

            subvalue = getattr(value, prop)
            subvalue_type = type(subvalue)

            if subvalue_type == int:
                node_attrs.append("%s=\"%d\"" % (prop, subvalue))
            elif subvalue_type == float:
                node_attrs.append("%s=\"%.4g\"" % (prop, subvalue))
            elif subvalue_type == bool:
                node_attrs.append("%s=\"%s\"" % (prop, "TRUE" if subvalue else "FALSE"))
            elif subvalue_type is str:
                node_attrs.append("%s=%s" % (prop, quoteattr(subvalue)))
            elif subvalue_type == set:
                node_attrs.append("%s=%s" % (prop, quoteattr("{" + ",".join(list(subvalue)) + "}")))
            elif subvalue is None:
                node_attrs.append("%s=\"NONE\"" % prop)
            elif issubclass(subvalue_type, bpy.types.ID):
                # special case, ID's are always referenced.
                node_attrs.append("%s=%s" % (prop, quoteattr(subvalue_type.__name__ + ":" + subvalue.name)))
            else:
                try:
                    subvalue_ls = list(subvalue)
                except:
                    subvalue_ls = None

                if subvalue_ls is None:
                    nodes_items.append((prop, subvalue, subvalue_type))
                else:
                    # check if the list contains native types
                    subvalue_rna = value.path_resolve(prop, False)
                    if type(subvalue_rna).__name__ == "bpy_prop_array":
                        # TODO, multi-dim!
                        def str_recursive(s):
                            if type(s) in (int, float, bool):
                                return str(s)
                            else:
                                return " ".join([str_recursive(si) for si in s])

                        node_attrs.append("%s=\"%s\"" % (prop, " ".join(str_recursive(v) for v in subvalue_rna)))
                    else:
                        nodes_lists.append((prop, subvalue_ls, subvalue_type))

        # declare + attributes
        if PRETTY_INTEND:
            tmp_str = "<%s " % value_type_name
            tmp_ident = "\n" + ident + (" " * len(tmp_str))
            
            fw("%s%s%s>\n" % (ident, tmp_str, tmp_ident.join(node_attrs)))
            
            del tmp_str
            del tmp_ident
        else:
            fw("%s<%s %s>\n" % (ident, value_type_name, " ".join(node_attrs)))
            

        # unique members
        for prop, subvalue, subvalue_type in nodes_items:
            rna2xml_node(ident_next, subvalue, value)

        # list members
        for prop, subvalue, subvalue_type in nodes_lists:
            fw("%s<%s>\n" % (ident_next, prop))
            for subvalue_item in subvalue:
                if subvalue_item is not None:
                    rna2xml_node(ident_next + ident_val, subvalue_item, value)
            fw("%s</%s>\n" % (ident_next, prop))

        fw("%s</%s>\n" % (ident, value_type_name))

    fw("<root>\n")
    for attr in dir(bpy.data):

        # exceptions
        if attr.startswith("_"):
            continue
        elif attr == "window_managers":
            continue

        value = getattr(bpy.data, attr)
        try:
            ls = value[:]
        except:
            ls = None

        if type(ls) == list:
            fw("%s<%s>\n" % (ident_val, attr))
            for blend_id in ls:
                rna2xml_node(ident_val + ident_val, blend_id, None)
            fw("%s</%s>\n" % (ident_val, attr))

    fw("</root>\n")


def main():
    filename = bpy.data.filepath.rstrip(".blend") + ".xml"
    file = open(filename, 'w')
    rna2xml(file.write)
    file.close()

    # read back.
    from xml.dom.minidom import parse
    xml_nodes = parse(filename)
    print("Written:", filename)

if __name__ == "__main__":
    main()
