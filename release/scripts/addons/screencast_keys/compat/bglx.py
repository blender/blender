from threading import Lock

import bgl
from bgl import Buffer as Buffer
import gpu
from gpu_extras.batch import batch_for_shader

GL_LINES = 0
GL_LINE_STRIP = 1
GL_LINE_LOOP = 2
GL_TRIANGLES = 5
GL_TRIANGLE_FAN = 6
GL_QUADS = 4

class InternalData:
    __inst = None
    __lock = Lock()

    def __init__(self):
        raise NotImplementedError("Not allowed to call constructor")

    @classmethod
    def __internal_new(cls):
        inst = super().__new__(cls)
        inst.color = [1.0, 1.0, 1.0, 1.0]
        inst.line_width = 1.0

        return inst

    @classmethod
    def get_instance(cls):
        if not cls.__inst:
            with cls.__lock:
                if not cls.__inst:
                    cls.__inst = cls.__internal_new()

        return cls.__inst

    def init(self):
        self.clear()

    def set_prim_mode(self, mode):
        self.prim_mode = mode

    def set_dims(self, dims):
        self.dims = dims

    def add_vert(self, v):
        self.verts.append(v)

    def add_tex_coord(self, uv):
        self.tex_coords.append(uv)

    def set_color(self, c):
        self.color = c

    def set_line_width(self, width):
        self.line_width = width

    def clear(self):
        self.prim_mode = None
        self.verts = []
        self.dims = None
        self.tex_coords = []

    def get_verts(self):
        return self.verts

    def get_dims(self):
        return self.dims

    def get_prim_mode(self):
        return self.prim_mode

    def get_color(self):
        return self.color

    def get_line_width(self):
        return self.line_width

    def get_tex_coords(self):
        return self.tex_coords


def glLineWidth(width):
    inst = InternalData.get_instance()
    inst.set_line_width(width)


def glColor3f(r, g, b):
    inst = InternalData.get_instance()
    inst.set_color([r, g, b, 1.0])


def glColor4f(r, g, b, a):
    inst = InternalData.get_instance()
    inst.set_color([r, g, b, a])


def glRecti(x0, y0, x1, y1):
    glBegin(GL_QUADS)
    glVertex2f(x0, y0)
    glVertex2f(x0, y1)
    glVertex2f(x1, y1)
    glVertex2f(x1, y0)
    glEnd()


def glBegin(mode):
    inst = InternalData.get_instance()
    inst.init()
    inst.set_prim_mode(mode)


def _get_transparency_shader():
    vertex_shader = '''
    uniform mat4 modelViewMatrix;
    uniform mat4 projectionMatrix;
    
    in vec2 pos;
    in vec2 texCoord;
    out vec2 uvInterp;
    
    void main()
    {
        uvInterp = texCoord;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(pos.xy, 0.0, 1.0);
        gl_Position.z = 1.0;
    }
    '''

    fragment_shader = '''
    uniform sampler2D image;
    uniform vec4 color;
    
    in vec2 uvInterp;
    out vec4 fragColor;
    
    void main()
    {
        fragColor = texture(image, uvInterp);
        fragColor.a = color.a;
    }
    '''

    return vertex_shader, fragment_shader


def glEnd():
    inst = InternalData.get_instance()

    color = inst.get_color()
    coords = inst.get_verts()
    tex_coords = inst.get_tex_coords()
    if inst.get_dims() == 2:
        if len(tex_coords) == 0:
            shader = gpu.shader.from_builtin('2D_UNIFORM_COLOR')
        else:
            #shader = gpu.shader.from_builtin('2D_IMAGE')
            vert_shader, frag_shader = _get_transparency_shader()
            shader = gpu.types.GPUShader(vert_shader, frag_shader)
    else:
        raise NotImplemented("get_dims() != 2")

    if len(tex_coords) == 0:
        data = {
            "pos": coords,
        }
    else:
        data = {
            "pos": coords,
            "texCoord": tex_coords
        }

    if inst.get_prim_mode() == GL_LINES:
        indices = []
        for i in range(0, len(coords), 2):
            indices.append([i, i + 1])
        batch = batch_for_shader(shader, 'LINES', data, indices=indices)

    elif inst.get_prim_mode() == GL_LINE_STRIP:
        batch = batch_for_shader(shader, 'LINE_STRIP', data)


    elif inst.get_prim_mode() == GL_LINE_LOOP:
        data["pos"].append(data["pos"][0])
        batch = batch_for_shader(shader, 'LINE_STRIP', data)

    elif inst.get_prim_mode() == GL_TRIANGLES:
        indices = []
        for i in range(0, len(coords), 3):
            indices.append([i, i + 1, i + 2])
        batch = batch_for_shader(shader, 'TRIS', data, indices=indices)

    elif inst.get_prim_mode() == GL_TRIANGLE_FAN:
        indices = []
        for i in range(1, len(coords) - 1):
            indices.append([0, i, i + 1])
        batch = batch_for_shader(shader, 'TRIS', data, indices=indices)

    elif inst.get_prim_mode() == GL_QUADS:
        indices = []
        for i in range(0, len(coords), 4):
            indices.extend([[i, i + 1, i + 2], [i + 2, i + 3, i]])
        batch = batch_for_shader(shader, 'TRIS', data, indices=indices)
    else:
        raise NotImplemented("get_prim_mode() != (GL_LINES|GL_TRIANGLES|GL_QUADS)")

    shader.bind()
    if len(tex_coords) != 0:
        shader.uniform_float("modelViewMatrix", gpu.matrix.get_model_view_matrix())
        shader.uniform_float("projectionMatrix", gpu.matrix.get_projection_matrix())
        shader.uniform_int("image", 0)
    shader.uniform_float("color", color)
    batch.draw(shader)

    inst.clear()


def glVertex2f(x, y):
    inst = InternalData.get_instance()
    inst.add_vert([x, y])
    inst.set_dims(2)


def glTexCoord2f(u, v):
    inst = InternalData.get_instance()
    inst.add_tex_coord([u, v])


GL_BLEND = bgl.GL_BLEND
GL_LINE_SMOOTH = bgl.GL_LINE_SMOOTH
GL_INT = bgl.GL_INT
GL_SCISSOR_BOX = bgl.GL_SCISSOR_BOX


def glEnable(cap):
    bgl.glEnable(cap)


def glDisable(cap):
    bgl.glDisable(cap)


def glScissor(x, y, width, height):
    bgl.glScissor(x, y, width, height)


def glGetIntegerv(pname, params):
    bgl.glGetIntegerv(pname, params)
