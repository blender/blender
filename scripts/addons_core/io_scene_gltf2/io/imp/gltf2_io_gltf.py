# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.path import uri_to_path
from ..com.gltf2_io import gltf_from_dict
from ..com.debug import Log
import logging
import json
import struct
import base64
from os.path import dirname, join, isfile


# Raise this error to have the importer report an error message.
class ImportError(RuntimeError):
    pass


class glTFImporter():
    """glTF Importer class."""

    def __init__(self, filename, import_settings):
        """initialization."""
        self.filename = filename
        self.import_settings = import_settings
        self.glb_buffer = None
        self.buffers = {}
        self.accessor_cache = {}
        self.decode_accessor_cache = {}
        self.import_user_extensions = import_settings['import_user_extensions']
        self.variant_mapping = {}  # Used to map between mgltf material idx and blender material, for Variants

        if 'loglevel' not in self.import_settings.keys():
            self.import_settings['loglevel'] = logging.CRITICAL

        self.log = Log(import_settings['loglevel'])

        # TODO: move to a com place?
        self.extensions_managed = [
            'KHR_materials_pbrSpecularGlossiness',
            'KHR_lights_punctual',
            'KHR_materials_unlit',
            'KHR_texture_transform',
            'KHR_materials_clearcoat',
            'KHR_mesh_quantization',
            'EXT_mesh_gpu_instancing',
            'KHR_draco_mesh_compression',
            'KHR_materials_variants',
            'KHR_materials_emissive_strength',
            'KHR_materials_transmission',
            'KHR_materials_specular',
            'KHR_materials_sheen',
            'KHR_materials_ior',
            'KHR_animation_pointer',
            'KHR_materials_volume',
            'EXT_texture_webp',
            'KHR_materials_anisotropy'
        ]

        # Add extensions required supported by custom import extensions
        for import_extension in self.import_user_extensions:
            if hasattr(import_extension, "extensions"):
                for custom_extension in import_extension.extensions:
                    if custom_extension.required:
                        self.extensions_managed.append(custom_extension.name)

    @staticmethod
    def load_json(content):
        def bad_constant(val):
            raise ImportError('Bad glTF: json contained %s' % val)
        try:
            text = str(content, encoding='utf-8')
            return json.loads(text, parse_constant=bad_constant)
        except ValueError as e:
            raise ImportError('Bad glTF: json error: %s' % e.args[0])

    @staticmethod
    def check_version(gltf):
        """Check version. This is done *before* gltf_from_dict."""
        if not isinstance(gltf, dict) or 'asset' not in gltf:
            raise ImportError("Bad glTF: no asset in json")
        if 'version' not in gltf['asset']:
            raise ImportError("Bad glTF: no version")
        if gltf['asset']['version'] != "2.0":
            raise ImportError("glTF version must be 2.0; got %s" % gltf['asset']['version'])

    def checks(self):
        """Some checks."""
        if self.data.extensions_required is not None:
            for extension in self.data.extensions_required:
                if extension not in self.data.extensions_used:
                    raise ImportError("Extension required must be in Extension Used too")
                if extension not in self.extensions_managed:
                    raise ImportError("Extension %s is not available on this addon version" % extension)

        if self.data.extensions_used is not None:
            for extension in self.data.extensions_used:
                if extension not in self.extensions_managed:
                    # Non blocking error #TODO log
                    pass

    def load_glb(self, content):
        """Load binary glb."""
        magic = content[:4]
        if magic != b'glTF':
            raise ImportError("This file is not a glTF/glb file")

        version, file_size = struct.unpack_from('<II', content, offset=4)
        if version != 2:
            raise ImportError("GLB version must be 2; got %d" % version)
        if file_size != len(content):
            raise ImportError("Bad GLB: file size doesn't match")

        glb_buffer = None
        offset = 12  # header size = 12

        # JSON chunk is first
        type_, len_, json_bytes, offset = self.load_chunk(content, offset)
        if type_ != b"JSON":
            raise ImportError("Bad GLB: first chunk not JSON")
        if len_ != len(json_bytes):
            raise ImportError("Bad GLB: length of json chunk doesn't match")
        gltf = glTFImporter.load_json(json_bytes)

        # BIN chunk is second (if it exists)
        if offset < len(content):
            type_, len_, data, offset = self.load_chunk(content, offset)
            if type_ == b"BIN\0":
                if len_ != len(data):
                    raise ImportError("Bad GLB: length of BIN chunk doesn't match")
                glb_buffer = data

        return gltf, glb_buffer

    def load_chunk(self, content, offset):
        """Load chunk."""
        chunk_header = struct.unpack_from('<I4s', content, offset)
        data_length = chunk_header[0]
        data_type = chunk_header[1]
        data = content[offset + 8: offset + 8 + data_length]

        return data_type, data_length, data, offset + 8 + data_length

    def read(self):
        """Read file."""
        if not isfile(self.filename):
            raise ImportError("Please select a file")

        with open(self.filename, 'rb') as f:
            content = memoryview(f.read())

        if content[:4] == b'glTF':
            gltf, self.glb_buffer = self.load_glb(content)
        else:
            gltf = glTFImporter.load_json(content)
            self.glb_buffer = None

        glTFImporter.check_version(gltf)

        try:
            self.data = gltf_from_dict(gltf)
        except AssertionError:
            import traceback
            traceback.print_exc()
            raise ImportError("Couldn't parse glTF. Check that the file is valid")

    def load_buffer(self, buffer_idx):
        """Load buffer."""
        buffer = self.data.buffers[buffer_idx]

        if buffer.uri:
            data = self.load_uri(buffer.uri)
            if data is None:
                raise ImportError("Missing resource, '" + buffer.uri + "'.")
            self.buffers[buffer_idx] = data

        else:
            # GLB-stored buffer
            if buffer_idx == 0 and self.glb_buffer is not None:
                self.buffers[buffer_idx] = self.glb_buffer

    def load_uri(self, uri):
        """Loads a URI."""
        sep = ';base64,'
        if uri.startswith('data:'):
            idx = uri.find(sep)
            if idx != -1:
                data = uri[idx + len(sep):]
                return memoryview(base64.b64decode(data))

        path = join(dirname(self.filename), uri_to_path(uri))
        try:
            with open(path, 'rb') as f_:
                return memoryview(f_.read())
        except Exception:
            self.log.error("Couldn't read file: " + path)
            return None
