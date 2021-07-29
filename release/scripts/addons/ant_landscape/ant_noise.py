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

# Another Noise Tool - Noise and Effects
# Jimmy Hazevoet

import bpy
from mathutils.noise import (
        seed_set,
        noise,
        turbulence,
        turbulence_vector,
        fractal,
        hybrid_multi_fractal,
        multi_fractal,
        ridged_multi_fractal,
        hetero_terrain,
        random_unit_vector,
        variable_lacunarity,
        voronoi,
        )
from math import (
        floor, sqrt,
        sin, cos, pi,
        )

# ------------------------------------------------------------
# Height scale:
def Height_Scale(input, iscale, offset, invert):
    if invert != 0:
        return (1.0 - input) * iscale + offset
    else:
        return input * iscale + offset


# Functions for marble_noise and effects:

def Dist(x, y):
    return sqrt((x * x) + (y * y))


def sin_bias(a):
    return 0.5 + 0.5 * sin(a)


def cos_bias(a):
    return 0.5 + 0.5 * cos(a)


def tri_bias(a):
    b = 2 * pi
    a = 1 - 2 * abs(floor((a * (1 / b)) + 0.5) - (a * (1 / b)))
    return a


def saw_bias(a):
    b = 2 * pi
    n = int(a / b)
    a -= n * b
    if a < 0:
        a += b
    return a / b


def soft(a):
    return a


def sharp(a):
    return a**0.5


def sharper(a):
    return sharp(sharp(a))


def no_bias(a):
    return a


def shapes(x, y, z, shape=0):
    p = pi
    if shape is 1:
        # ring
        x = x * p
        y = y * p
        s = cos(x**2 + y**2) / (x**2 + y**2 + 0.5)
    elif shape is 2:
        # swirl
        x = x * p
        y = y * p
        s = ((x * sin(x * x + y * y) + y * cos(x * x + y * y)) / (x**2 + y**2 + 0.5))
    elif shape is 3:
        # bumps
        x = x * p
        y = y * p
        z = z * p
        s = 1 - ((cos(x * p) + cos(y * p) + cos(z * p)) - 0.5)
    elif shape is 4:
        # wave
        x = x * p * 2
        y = y * p * 2
        s = sin(x + sin(y))
    elif shape is 5:
        # z grad.
        s = (z * p)
    elif shape is 6:
        # y grad.
        s = (y * p)
    elif shape is 7:
        # x grad.
        s = (x * p)
    else:
        # marble default
        s = ((x + y + z) * 5)
    return s


# marble_noise
def marble_noise(x, y, z, origin, size, shape, bias, sharpnes, turb, depth, hard, basis, amp, freq):

    s = shapes(x, y, z, shape)
    x += origin[0]
    y += origin[1]
    z += origin[2]
    value = s + turb * turbulence_vector((x, y, z), depth, hard, basis)[1]

    if bias is 1:
        value = cos_bias(value)
    elif bias is 2:
        value = tri_bias(value)
    elif bias is 3:
        value = saw_bias(value)
    else:
        value = sin_bias(value)

    if sharpnes is 1:
        value = 1.0 - sharp(value)
    elif sharpnes is 2:
        value = 1.0 - sharper(value)
    elif sharpnes is 3:
        value = soft(value)
    elif sharpnes is 4:
        value = sharp(value)
    elif sharpnes is 5:
        value = sharper(value)
    else:
        value = 1.0 - soft(value)

    return value


# vl_noise_turbulence:
def vlnTurbMode(coords, distort, basis, vlbasis, hardnoise):
    # hard noise
    if hardnoise:
        return (abs(-variable_lacunarity(coords, distort, basis, vlbasis)))
    # soft noise
    else:
        return variable_lacunarity(coords, distort, basis, vlbasis)


def vl_noise_turbulence(coords, distort, depth, basis, vlbasis, hardnoise, amp, freq):
    x, y, z = coords
    value = vlnTurbMode(coords, distort, basis, vlbasis, hardnoise)
    i=0
    for i in range(depth):
        i+=1
        value += vlnTurbMode((x * (freq * i), y * (freq * i), z * (freq * i)), distort, basis, vlbasis, hardnoise) * (amp * 0.5 / i)
    return value


## duo_multiFractal:
def double_multiFractal(coords, H, lacunarity, octaves, offset, gain, basis, vlbasis):
    x, y, z = coords
    n1 = multi_fractal((x * 1.5 + 1, y * 1.5 + 1, z * 1.5 + 1), 1.0, 1.0, 1.0, basis) * (offset * 0.5)
    n2 = multi_fractal((x - 1, y - 1, z - 1), H, lacunarity, octaves, vlbasis) * (gain * 0.5)
    return (n1 * n1 + n2 * n2) * 0.5


## distorted_heteroTerrain:
def distorted_heteroTerrain(coords, H, lacunarity, octaves, offset, distort, basis, vlbasis):
    x, y, z = coords
    h1 = (hetero_terrain((x, y, z), 1.0, 2.0, 1.0, 1.0, basis) * 0.5)
    d =  h1 * distort
    h2 = (hetero_terrain((x + d, y + d, z + d), H, lacunarity, octaves, offset, vlbasis) * 0.25)
    return (h1 * h1 + h2 * h2) * 0.5


## SlickRock:
def slick_rock(coords, H, lacunarity, octaves, offset, gain, distort, basis, vlbasis):
    x, y, z = coords
    n = multi_fractal((x,y,z), 1.0, 2.0, 2.0, basis) * distort * 0.25
    r = ridged_multi_fractal((x + n, y + n, z + n), H, lacunarity, octaves, offset + 0.1, gain * 2, vlbasis)
    return (n + (n * r)) * 0.5


## vlhTerrain
def vl_hTerrain(coords, H, lacunarity, octaves, offset, basis, vlbasis, distort):
    x, y, z = coords
    ht = hetero_terrain((x, y, z), H, lacunarity, octaves, offset, basis ) * 0.25
    vl = ht * variable_lacunarity((x, y, z), distort, basis, vlbasis) * 0.5 + 0.5
    return vl * ht


# another turbulence
def ant_turbulence(coords, depth, hardnoise, nbasis, amp, freq, distortion):
    x, y, z = coords
    t = turbulence_vector((x/2, y/2, z/2), depth, 0, nbasis, amp, freq) * 0.5 * distortion
    return turbulence((t[0], t[1], t[2]), 2, hardnoise, 3) * 0.5 + 0.5


# rocks noise
def rocks_noise(coords, depth, hardnoise, nbasis, distortion):
    x,y,z = coords
    p = turbulence((x, y, z), 4, 0, 0) * 0.125 * distortion
    xx, yy, zz = x, y, z
    a = turbulence((xx + p, yy + p, zz), 2, 0, 7)
    pa = a * 0.1875 * distortion
    b = turbulence((x, y, z + pa), depth, hardnoise, nbasis)
    return ((a + 0.5 * (b - a)) * 0.5 + 0.5)


# shattered_hterrain:
def shattered_hterrain(coords, H, lacunarity, octaves, offset, distort, basis):
    x, y, z = coords
    d = (turbulence_vector(coords, 6, 0, 0)[0] * 0.5 + 0.5) * distort * 0.5
    t1 = (turbulence_vector((x + d, y + d, z + d), 0, 0, 7)[0] + 0.5)
    t2 = (hetero_terrain((x * 2, y * 2, z * 2), H, lacunarity, octaves, offset, basis) * 0.5)
    return ((t1 * t2) + t2 * 0.5) * 0.5


# strata_hterrain
def strata_hterrain(coords, H, lacunarity, octaves, offset, distort, basis):
    x, y, z = coords
    value = hetero_terrain((x, y, z), H, lacunarity, octaves, offset, basis) * 0.5
    steps = (sin(value * (distort * 5) * pi) * (0.1 / (distort * 5) * pi))
    return (value * (1.0 - 0.5) + steps * 0.5)


# Planet Noise by: Farsthary
# https://farsthary.com/2010/11/24/new-planet-procedural-texture/
def planet_noise(coords, oct=6, hard=0, noisebasis=1, nabla=0.001):
    x, y, z = coords
    d = 0.001
    offset = nabla * 1000
    x = turbulence((x, y, z), oct, hard, noisebasis)
    y = turbulence((x + offset, y, z), oct, hard, noisebasis)
    z = turbulence((x, y + offset, z), oct, hard, noisebasis)
    xdy = x - turbulence((x, y + d, z), oct, hard, noisebasis)
    xdz = x - turbulence((x, y, z + d), oct, hard, noisebasis)
    ydx = y - turbulence((x + d, y, z), oct, hard, noisebasis)
    ydz = y - turbulence((x, y, z + d), oct, hard, noisebasis)
    zdx = z - turbulence((x + d, y, z), oct, hard, noisebasis)
    zdy = z - turbulence((x, y + d, z), oct, hard, noisebasis)
    return (zdy - ydz), (zdx - xdz), (ydx - xdy)


###----------------------------------------------------------------------
# v.1.04 Effect functions:

def maximum(a, b):
    if (a > b): b = a
    return b


def minimum(a, b):
    if (a < b): b = a
    return b


def Mix_Modes(a, b, mixfactor, mode):
    mode = int(mode)
    a = a * (1.0 - mixfactor)
    b = b * (1.0 + mixfactor)
    #1  mix
    if mode == 0:
        return (a * (1.0 - 0.5) + b * 0.5)
    #2  add
    elif mode == 1:
        return (a + b)
    #3  sub.
    elif mode == 2:
        return (a - b)
    #4  mult.
    elif mode == 3:
        return (a * b)
    #5  abs diff.
    elif mode == 4:
        return (abs(a - b))
    #6  screen
    elif mode == 5:
        return 1.0 - ((1.0 - a) * (1.0 - b) / 1.0)
    #7  addmodulo
    elif mode == 6:
        return (a + b) % 1.0
    #8  min.
    elif mode == 7:
        return minimum(a, b)
    #9  max.
    elif mode == 8:
        return maximum(a, b)
    else:
        return 0


Bias_Types  = [sin_bias, cos_bias, tri_bias, saw_bias, no_bias]
Sharp_Types = [soft, sharp, sharper]


# Transpose effect coords:
def Trans_Effect(coords, size, loc):
    x, y, z = coords
    x = (x * 2.0 / size + loc[0])
    y = (y * 2.0 / size + loc[1])
    return x, y, z


# Effect_Basis_Function:
def Effect_Basis_Function(coords, type, bias):
    bias = int(bias)
    type = int(type)
    x, y, z = coords
    iscale = 1.0
    offset = 0.0

    ## gradient:
    if type == 1:
        effect = offset + iscale * (Bias_Types[bias](x + y))
    ## waves / bumps:
    elif type == 2:
        effect = offset + iscale * 0.5 * (Bias_Types[bias](x * pi) + Bias_Types[bias](y * pi))
    ## zigzag:
    elif type == 3:
        effect = offset + iscale * Bias_Types[bias](offset + iscale * sin(x * pi + sin(y * pi)))
    ## wavy:
    elif type == 4:
        effect = offset + iscale * (Bias_Types[bias](cos(x) + sin(y) + cos(x * 2 + y * 2) - sin(-x * 4 + y * 4)))
    ## sine bump:
    elif type == 5:
        effect =   offset + iscale * 1 - Bias_Types[bias]((sin(x * pi) + sin(y * pi)))
    ## dots:
    elif type == 6:
        effect = offset + iscale * (Bias_Types[bias](x * pi * 2) * Bias_Types[bias](y * pi * 2)) - 0.5
    ## rings:
    elif type == 7:
        effect = offset + iscale * (Bias_Types[bias ](1.0 - (x * x + y * y)))
    ## spiral:
    elif type == 8:
        effect = offset + iscale * Bias_Types[bias]( (x * sin(x * x + y * y) + y * cos(x * x + y * y)) / (x**2 + y**2 + 0.5)) * 2
    ## square / piramide:
    elif type == 9:
        effect = offset + iscale * Bias_Types[bias](1.0 - sqrt((x * x)**10 + (y * y)**10)**0.1)
    ## blocks:
    elif type == 10:
        effect = (0.5 - max(Bias_Types[bias](x * pi) , Bias_Types[bias](y * pi)))
        if effect > 0.0:
            effect = 1.0
        effect = offset + iscale * effect
    ## grid:
    elif type == 11:
        effect = (0.025 - min(Bias_Types[bias](x * pi), Bias_Types[bias](y * pi)))
        if effect > 0.0:
            effect = 1.0
        effect = offset + iscale * effect
    ## tech:
    elif type == 12:
        a = max(Bias_Types[bias](x * pi), Bias_Types[bias](y * pi))
        b = max(Bias_Types[bias](x * pi * 2 + 2), Bias_Types[bias](y * pi * 2 + 2))
        effect = min(Bias_Types[bias](a), Bias_Types[bias](b)) * 3.0 - 2.0
        if effect > 0.5:
            effect = 1.0
        effect = offset + iscale * effect
    ## crackle:
    elif type == 13:
        t = turbulence((x, y, 0), 6, 0, 0) * 0.25
        effect = variable_lacunarity((x, y, t), 0.25, 0, 8)
        if effect > 0.5:
            effect = 0.5
        effect = offset + iscale * effect
    ## sparse cracks noise:
    elif type == 14:
        effect = 2.5 * abs(noise((x, y, 0), 1)) - 0.1
        if effect > 0.25:
            effect = 0.25
        effect = offset + iscale * (effect * 2.5)
    ## shattered rock noise:
    elif type == 15:
        effect = 0.5 + noise((x, y, 0), 7)
        if effect > 0.75:
            effect = 0.75
        effect = offset + iscale * effect
    ## lunar noise:
    elif type == 16:
        effect = 0.25 + 1.5 * voronoi((x, y, 0), 1)[0][0]
        if effect > 0.5:
            effect = 0.5
        effect = offset + iscale * effect * 2
    ## cosine noise:
    elif type == 17:
        effect = cos(5 * noise((x, y, 0), 0))
        effect = offset + iscale * (effect * 0.5)
    ## spikey noise:
    elif type == 18:
        n = 0.5 + 0.5 * turbulence((x * 5, y * 5, 0), 8, 0, 0)
        effect = ((n * n)**5)
        effect = offset + iscale * effect
    ## stone noise:
    elif type == 19:
        effect = offset + iscale * (noise((x * 2, y * 2, 0), 0) * 1.5 - 0.75)
    ## Flat Turb:
    elif type == 20:
        t = turbulence((x, y, 0), 6, 0, 0)
        effect = t * 2.0
        if effect > 0.25:
            effect = 0.25
        effect = offset + iscale * effect
    ## Flat Voronoi:
    elif type == 21:
        t = 1 - voronoi((x, y, 0), 1)[0][0]
        effect = t * 2 - 1.5
        if effect > 0.25:
            effect = 0.25
        effect = offset + iscale * effect
    else:
        effect = 0.0

    if effect < 0.0:
        effect = 0.0

    return effect


# fractalize Effect_Basis_Function: ------------------------------
def Effect_Function(coords, type, bias, turb, depth, frequency, amplitude):

    x, y, z = coords
    ## turbulence:
    if turb > 0.0:
        t = turb * ( 0.5 + 0.5 * turbulence(coords, 6, 0, 0))
        x = x + t
        y = y + t
        z = z + t

    result = Effect_Basis_Function((x, y, z), type, bias) * amplitude
    ## fractalize:
    if depth != 0:
        i=0
        for i in range(depth):
            i+=1
            x *= frequency
            y *= frequency
            result += Effect_Basis_Function((x, y, z), type, bias) * amplitude / i

    return result


# ------------------------------------------------------------
# landscape_gen
def noise_gen(coords, props):

    terrain_name = props[0]
    cursor = props[1]
    smooth = props[2]
    triface = props[3]
    sphere = props[4]
    land_mat = props[5]
    water_mat = props[6]
    texture_name = props[7]
    subd_x = props[8]
    subd_y = props[9]
    meshsize_x = props[10]
    meshsize_y = props[11]
    meshsize = props[12]
    rseed = props[13]
    x_offset = props[14]
    y_offset = props[15]
    z_offset = props[16]
    size_x = props[17]
    size_y = props[18]
    size_z = props[19]
    nsize = props[20]
    ntype = props[21]
    nbasis = int(props[22])
    vlbasis = int(props[23])
    distortion = props[24]
    hardnoise = int(props[25])
    depth = props[26]
    amp = props[27]
    freq = props[28]
    dimension = props[29]
    lacunarity = props[30]
    offset = props[31]
    gain = props[32]
    marblebias = int(props[33])
    marblesharpnes = int(props[34])
    marbleshape = int(props[35])
    height = props[36]
    height_invert = props[37]
    height_offset = props[38]
    maximum = props[39]
    minimum = props[40]
    falloff = int(props[41])
    edge_level = props[42]
    falloffsize_x = props[43]
    falloffsize_y = props[44]
    stratatype = props[45]
    strata = props[46]
    addwater = props[47]
    waterlevel = props[48]
    vert_group = props[49]
    remove_double = props[50]
    fx_mixfactor = props[51]
    fx_mix_mode = props[52]
    fx_type = props[53]
    fx_bias = props[54]
    fx_turb = props[55]
    fx_depth = props[56]
    fx_frequency = props[57]
    fx_amplitude = props[58]
    fx_size = props[59]
    fx_loc_x = props[60]
    fx_loc_y = props[61]
    fx_height = props[62]
    fx_offset = props[63]
    fx_invert = props[64]

    x, y, z = coords

    # Origin
    if rseed is 0:
        origin = x_offset, y_offset, z_offset
        origin_x = x_offset
        origin_y = y_offset
        origin_z = z_offset
        o_range = 1.0
    else:
        # Randomise origin
        o_range = 10000.0
        seed_set(rseed)
        origin = random_unit_vector()
        ox = (origin[0] * o_range)
        oy = (origin[1] * o_range)
        oz = (origin[2] * o_range)
        origin_x = (ox - (ox / 2)) + x_offset
        origin_y = (oy - (oy / 2)) + y_offset
        origin_z = (oz - (oz / 2)) + z_offset

    ncoords = (x / (nsize * size_x) + origin_x, y / (nsize * size_y) + origin_y, z / (nsize * size_z) + origin_z)

    # Noise basis type's
    if nbasis == 9:
        nbasis = 14  # Cellnoise
    if vlbasis == 9:
        vlbasis = 14

    # Noise type's
    if ntype in [0, 'multi_fractal']:
        value = multi_fractal(ncoords, dimension, lacunarity, depth, nbasis) * 0.5

    elif ntype in [1, 'ridged_multi_fractal']:
        value = ridged_multi_fractal(ncoords, dimension, lacunarity, depth, offset, gain, nbasis) * 0.5

    elif ntype in [2, 'hybrid_multi_fractal']:
        value = hybrid_multi_fractal(ncoords, dimension, lacunarity, depth, offset, gain, nbasis) * 0.5

    elif ntype in [3, 'hetero_terrain']:
        value = hetero_terrain(ncoords, dimension, lacunarity, depth, offset, nbasis) * 0.25

    elif ntype in [4, 'fractal']:
        value = fractal(ncoords, dimension, lacunarity, depth, nbasis)

    elif ntype in [5, 'turbulence_vector']:
        value = turbulence_vector(ncoords, depth, hardnoise, nbasis, amp, freq)[0]

    elif ntype in [6, 'variable_lacunarity']:
        value = variable_lacunarity(ncoords, distortion, nbasis, vlbasis)

    elif ntype in [7, 'marble_noise']:
        value = marble_noise(
                        (ncoords[0] - origin_x + x_offset),
                        (ncoords[1] - origin_y + y_offset),
                        (ncoords[2] - origin_z + z_offset),
                        (origin[0] + x_offset, origin[1] + y_offset, origin[2] + z_offset), nsize,
                        marbleshape, marblebias, marblesharpnes,
                        distortion, depth, hardnoise, nbasis, amp, freq
                        )
    elif ntype in [8, 'shattered_hterrain']:
        value = shattered_hterrain(ncoords, dimension, lacunarity, depth, offset, distortion, nbasis)

    elif ntype in [9, 'strata_hterrain']:
        value = strata_hterrain(ncoords, dimension, lacunarity, depth, offset, distortion, nbasis)

    elif ntype in [10, 'ant_turbulence']:
        value = ant_turbulence(ncoords, depth, hardnoise, nbasis, amp, freq, distortion)

    elif ntype in [11, 'vl_noise_turbulence']:
        value = vl_noise_turbulence(ncoords, distortion, depth, nbasis, vlbasis, hardnoise, amp, freq)

    elif ntype in [12, 'vl_hTerrain']:
        value = vl_hTerrain(ncoords, dimension, lacunarity, depth, offset, nbasis, vlbasis, distortion)

    elif ntype in [13, 'distorted_heteroTerrain']:
        value = distorted_heteroTerrain(ncoords, dimension, lacunarity, depth, offset, distortion, nbasis, vlbasis)

    elif ntype in [14, 'double_multiFractal']:
        value = double_multiFractal(ncoords, dimension, lacunarity, depth, offset, gain, nbasis, vlbasis)

    elif ntype in [15, 'rocks_noise']:
        value = rocks_noise(ncoords, depth, hardnoise, nbasis, distortion)

    elif ntype in [16, 'slick_rock']:
        value = slick_rock(ncoords,dimension, lacunarity, depth, offset, gain, distortion, nbasis, vlbasis)

    elif ntype in [17, 'planet_noise']:
        value = planet_noise(ncoords, depth, hardnoise, nbasis)[2] * 0.5 + 0.5

    elif ntype in [18, 'blender_texture']:
        if texture_name != "" and texture_name in bpy.data.textures:
            value = bpy.data.textures[texture_name].evaluate(ncoords)[3]
        else:
            value = 0.0
    else:
        value = 0.5

    # Effect mix
    val = value
    if fx_type in [0,"0"]:
        fx_mixfactor = -1.0
        fxval = val
    else:
        fxcoords = Trans_Effect((x, y, z), fx_size, (fx_loc_x, fx_loc_y))
        effect = Effect_Function(fxcoords, fx_type, fx_bias, fx_turb, fx_depth, fx_frequency, fx_amplitude)
        effect = Height_Scale(effect, fx_height, fx_offset, fx_invert)
        fxval = Mix_Modes(val, effect, fx_mixfactor, fx_mix_mode)
    value = fxval

    # Adjust height
    value = Height_Scale(value, height, height_offset, height_invert)

    # Edge falloff:
    if not sphere:
        if falloff:
            ratio_x, ratio_y = abs(x) * 2 / meshsize_x, abs(y) * 2 / meshsize_y
            fallofftypes = [0,
                            sqrt(ratio_y**falloffsize_y),
                            sqrt(ratio_x**falloffsize_x),
                            sqrt(ratio_x**falloffsize_x + ratio_y**falloffsize_y)
                           ]
            dist = fallofftypes[falloff]
            value -= edge_level
            if(dist < 1.0):
                dist = (dist * dist * (3 - 2 * dist))
                value = (value - value * dist) + edge_level
            else:
                value = edge_level

    # Strata / terrace / layers
    if stratatype not in [0, "0"]:
        if stratatype in [1, "1"]:
            strata = strata / height
            strata *= 2
            steps = (sin(value * strata * pi) * (0.1 / strata * pi))
            value = (value * 0.5 + steps * 0.5) * 2.0

        elif stratatype in [2, "2"]:
            strata = strata / height
            steps = -abs(sin(value * strata * pi) * (0.1 / strata * pi))
            value = (value * 0.5 + steps * 0.5) * 2.0

        elif stratatype in [3, "3"]:
            strata = strata / height
            steps = abs(sin(value * strata * pi) * (0.1 / strata * pi))
            value = (value * 0.5 + steps * 0.5) * 2.0

        elif stratatype in [4, "4"]:
            strata = strata / height
            value = int( value * strata ) * 1.0 / strata

        elif stratatype in [5, "5"]:
            strata = strata / height
            steps = (int( value * strata ) * 1.0 / strata)
            value = (value * (1.0 - 0.5) + steps * 0.5)

    # Clamp height min max
    if (value < minimum):
        value = minimum
    if (value > maximum):
        value = maximum

    return value
