#!/usr/bin/env python3

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
# ***** END GPL LICENCE BLOCK *****

######################################################
#
#    Name:
#        dna.py
#        
#    Description:
#        Creates a browsable DNA output to HTML.
#        
#    Author:
#        Jeroen Bakker
#        
#    Version:
#        v0.1 (12-05-2009) - migration of original source code to python.
#           Added code to support blender 2.5 branch
#        v0.2 (25-05-2009) - integrated with BlendFileReader.py
#        
#    Input:
#        blender build executable
#        
#    Output:
#        dna.html
#        dna.css (will only be created when not existing)
#
#    Startup:
#        ./blender -P BlendFileDnaExporter.py
#
#    Process:
#        1: write blend file with SDNA info
#        2: read blend header from blend file
#        3: seek DNA1 file-block
#        4: read dna record from blend file
#        5: close and eventually delete temp blend file
#        6: export dna to html and css
#        7: quit blender
#
######################################################

import struct
import sys
import getopt                   # command line arguments handling
from string import Template     # strings completion


# logs
import logging
log = logging.getLogger("BlendFileDnaExporter")

if '--dna-debug' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)


class DNACatalogHTML:
    '''
    DNACatalog is a catalog of all information in the DNA1 file-block
    '''

    def __init__(self, catalog, bpy_module = None):
        self.Catalog = catalog
        self.bpy = bpy_module
    
    def WriteToHTML(self, handle):
            
        dna_html_template = """
            <!DOCTYPE html PUBLIC -//W3C//DTD HTML 4.01 Transitional//EN http://www.w3.org/TR/html4/loose.dtd>
            <html>
            <head>
                <link rel="stylesheet" type="text/css" href="dna.css" media="screen, print" />
                <meta http-equiv="Content-Type" content="text/html"; charset="ISO-8859-1" />
                <title>The mystery of the blend</title>
            </head>
            <body>
                <div class=title>
                    Blender ${version}<br/>
                    Internal SDNA structures
                </div>
                Architecture: ${bitness} ${endianness}<br/>
                Build revision: <a href="https://svn.blender.org/svnroot/bf-blender/!svn/bc/${revision}/trunk/">${revision}</a><br/>
                File format reference: <a href="mystery_of_the_blend.html">The mystery of the blend</a> by Jeroen Bakker<br/>
                <h1>Index of blender structures</h1>
                <ul class=multicolumn>
                    ${structs_list}
                </ul>
                ${structs_content}
            </body>
            </html>"""
        
        header = self.Catalog.Header
        bpy = self.bpy
        
        # ${version} and ${revision}
        if bpy:
            version = '.'.join(map(str, bpy.app.version))
            revision = bpy.app.build_hash
        else:
            version = str(header.Version)
            revision = 'Unknown'
        
        # ${bitness}
        if header.PointerSize == 8:
            bitness = '64 bit'
        else:
            bitness = '32 bit'

        # ${endianness}
        if header.LittleEndianness:
            endianess= 'Little endianness'
        else:
            endianess= 'Big endianness'
        
        # ${structs_list}
        log.debug("Creating structs index")
        structs_list = ''
        list_item = '<li class="multicolumn">({0}) <a href="#{1}">{1}</a></li>\n'
        structureIndex = 0
        for structure in self.Catalog.Structs:
            structs_list += list_item.format(structureIndex, structure.Type.Name)
            structureIndex+=1

        # ${structs_content}
        log.debug("Creating structs content")
        structs_content = ''
        for structure in self.Catalog.Structs:
            log.debug(structure.Type.Name)
            structs_content += self.Structure(structure)
           
        d = dict(
            version = version, 
            revision = revision, 
            bitness = bitness, 
            endianness = endianess, 
            structs_list = structs_list, 
            structs_content = structs_content
        )

        dna_html = Template(dna_html_template).substitute(d)
        dna_html = self.format(dna_html)
        handle.write(dna_html)
    
    def Structure(self, structure):
        struct_table_template = """
            <table><a name="${struct_name}"></a>
                <caption><a href="#${struct_name}">${struct_name}</a></caption>
                <thead>
                    <tr>
                        <th>reference</th>
                        <th>structure</th>
                        <th>type</th>
                        <th>name</th>
                        <th>offset</th>
                        <th>size</th>
                    </tr>
                </thead>
                <tbody>
                ${fields}
                </tbody>
            </table>
            <label>Total size: ${size} bytes</label><br/>
            <label>(<a href="#top">top</a>)</label><br/>"""
        
        d = dict(
            struct_name = structure.Type.Name, 
            fields = self.StructureFields(structure, None, 0), 
            size = str(structure.Type.Size)
        )
        
        struct_table = Template(struct_table_template).substitute(d)
        return struct_table
        
    def StructureFields(self, structure, parentReference, offset):
        fields = ''
        for field in structure.Fields:
            fields += self.StructureField(field, structure, parentReference, offset)
            offset += field.Size(self.Catalog.Header)
        return fields
        
    def StructureField(self, field, structure, parentReference, offset):
        structure_field_template = """
            <tr>
                <td>${reference}</td>
                <td>${struct}</td>
                <td>${type}</td>
                <td>${name}</td>
                <td>${offset}</td>
                <td>${size}</td>
            </tr>"""
        
        if field.Type.Structure is None or field.Name.IsPointer():

            # ${reference}
            reference = field.Name.AsReference(parentReference)

            # ${struct}
            if parentReference is not None:
                struct = '<a href="#{0}">{0}</a>'.format(structure.Type.Name)
            else:
                struct = structure.Type.Name
            
            # ${type}
            type = field.Type.Name
            
            # ${name}
            name = field.Name.Name
            
            # ${offset}
            # offset already set
            
            # ${size}
            size = field.Size(self.Catalog.Header)
        
            d = dict(
                reference = reference, 
                struct = struct, 
                type = type, 
                name = name, 
                offset = offset, 
                size = size
            )
            
            structure_field = Template(structure_field_template).substitute(d)
        
        elif field.Type.Structure is not None:
            reference = field.Name.AsReference(parentReference)
            structure_field = self.StructureFields(field.Type.Structure, reference, offset) 

        return structure_field

    def indent(self, input, dent, startswith = ''):
        output = ''
        if dent < 0:
            for line in input.split('\n'):
                dent = abs(dent)
                output += line[dent:] + '\n'   # unindent of a desired amount
        elif dent == 0:
            for line in input.split('\n'):
                output += line.lstrip() + '\n'  # remove indentation completely
        elif dent > 0:
            for line in input.split('\n'):
                output += ' '* dent + line + '\n'
        return output
    
    def format(self, input):
        diff = {
            '\n<!DOCTYPE':'<!DOCTYPE', 
            '\n</ul>'    :'</ul>', 
            '<a name'         :'\n<a name', 
            '<tr>\n'     :'<tr>', 
            '<tr>'       :'  <tr>', 
            '</th>\n'    :'</th>', 
            '</td>\n'    :'</td>', 
            '<tbody>\n'  :'<tbody>'
        }
        output = self.indent(input, 0)
        for key, value in diff.items():
            output = output.replace(key, value)
        return output

    def WriteToCSS(self, handle):
        '''
        Write the Cascading stylesheet template to the handle
        It is expected that the handle is a Filehandle
        '''
        css = """
            @CHARSET "ISO-8859-1";
            
            body {
                font-family: verdana;
                font-size: small;
            }
            
            div.title {
                font-size: large;
                text-align: center;
            }
            
            h1 {
                page-break-before: always;
            }

            h1, h2 {
                background-color: #D3D3D3;
                color:#404040;
                margin-right: 3%;
                padding-left: 40px;
            }
            
            h1:hover{
                background-color: #EBEBEB;
            }

            h3 {
                padding-left: 40px;
            }
            
            table {
                border-width: 1px;
                border-style: solid;
                border-color: #000000;
                border-collapse: collapse;
                width: 94%;
                margin: 20px 3% 10px;
            }
            
            caption {
                margin-bottom: 5px;
            }
            
            th {
                background-color: #000000;
                color:#ffffff;
                padding-left:5px;
                padding-right:5px;
            }
            
            tr {
            }
            
            td {
                border-width: 1px;
                border-style: solid;
                border-color: #a0a0a0;
                padding-left:5px;
                padding-right:5px;
            }
            
            label {
                float:right;
                margin-right: 3%;
            }
            
            ul.multicolumn {
                list-style:none;
                float:left;
                padding-right:0px;
                margin-right:0px;
            }

            li.multicolumn {
                float:left;
                width:200px;
                margin-right:0px;
            }
            
            a {
                color:#a000a0;
                text-decoration:none;
            }
            
            a:hover {
                color:#a000a0;
                text-decoration:underline;
            }
            """
            
        css = self.indent(css, 0)

        handle.write(css)


def usage():
    print("\nUsage: \n\tblender2.5 --background -noaudio --python BlendFileDnaExporter_25.py [-- [options]]")
    print("Options:")
    print("\t--dna-keep-blend:      doesn't delete the produced blend file DNA export to html")
    print("\t--dna-debug:           sets the logging level to DEBUG (lots of additional info)")
    print("\t--dna-versioned        saves version informations in the html and blend filenames")
    print("\t--dna-overwrite-css    overwrite dna.css, useful when modifying css in the script")
    print("Examples:")
    print("\tdefault:       % blender2.5 --background -noaudio --python BlendFileDnaExporter_25.py")
    print("\twith options:  % blender2.5 --background -noaudio --python BlendFileDnaExporter_25.py -- --dna-keep-blend --dna-debug\n")

    
######################################################
# Main
######################################################

def main():
    
    import os, os.path

    try:
        bpy = __import__('bpy')

        # Files
        if '--dna-versioned' in sys.argv:
            blender_version = '_'.join(map(str, bpy.app.version))
            filename = 'dna-{0}-{1}_endian-{2}-{3}'.format(sys.arch, sys.byteorder, blender_version, bpy.app.build_hash)
        else:
            filename = 'dna'
        dir = os.path.dirname(__file__)
        Path_Blend = os.path.join(dir, filename + '.blend') # temporary blend file
        Path_HTML  = os.path.join(dir, filename + '.html')  # output html file
        Path_CSS   = os.path.join(dir, 'dna.css')           # output css file

        # create a blend file for dna parsing
        if not os.path.exists(Path_Blend):
            log.info("1: write temp blend file with SDNA info")
            log.info("   saving to: " + Path_Blend)
            try:
                bpy.ops.wm.save_as_mainfile(filepath = Path_Blend, copy = True, compress = False)
            except:
                log.error("Filename {0} does not exist and can't be created... quitting".format(Path_Blend))
                return
        else:
            log.info("1: found blend file with SDNA info")
            log.info("   " + Path_Blend)
        
        # read blend header from blend file
        log.info("2: read file:")
        
        if not dir in sys.path:
            sys.path.append(dir)
        import BlendFileReader
        
        handle = BlendFileReader.openBlendFile(Path_Blend)
        blendfile = BlendFileReader.BlendFile(handle)
        catalog = DNACatalogHTML(blendfile.Catalog, bpy)

        # close temp file
        handle.close()
        
        # deleting or not?
        if '--dna-keep-blend' in sys.argv:
            # keep the blend, useful for studying hexdumps
            log.info("5: closing blend file:")
            log.info("   {0}".format(Path_Blend))
        else:
            # delete the blend
            log.info("5: close and delete temp blend:")
            log.info("   {0}".format(Path_Blend))
            os.remove(Path_Blend)
        
        # export dna to xhtml
        log.info("6: export sdna to xhtml file: %r" % Path_HTML)
        handleHTML = open(Path_HTML, "w")
        catalog.WriteToHTML(handleHTML)
        handleHTML.close()

        # only write the css when doesn't exist or at explicit request
        if not os.path.exists(Path_CSS) or '--dna-overwrite-css' in sys.argv:
            handleCSS = open(Path_CSS, "w")
            catalog.WriteToCSS(handleCSS)
            handleCSS.close()

        # quit blender
        if not bpy.app.background:
            log.info("7: quit blender")
            bpy.ops.wm.exit_blender()
    
    except ImportError:
        log.warning("  skipping, not running in Blender")
        usage()
        sys.exit(2)
        

if __name__ == '__main__':
    main()
