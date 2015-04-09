!!ARBfp1.0
PARAM dx = program.local[0];
PARAM darkness = program.local[1];
PARAM render = program.local[2];
PARAM f = {1.442695041, 1.442695041, 1.442695041, 1.442695041};
TEMP temp, shadow, flame, spec, value;
TEX temp, fragment.texcoord[0], texture[0], 3D;
TEX shadow, fragment.texcoord[0], texture[1], 3D;
TEX flame, fragment.texcoord[0], texture[2], 3D;
TEX spec, flame.r, texture[3], 1D;
# unpremultiply volume texture
RCP value.r, temp.a;
MUL temp.r, temp.r, value.r;
MUL temp.g, temp.g, value.r;
MUL temp.b, temp.b, value.r;
# calculate shading factor from density
MUL value.r, temp.a, darkness.a;
MUL value.r, value.r, dx.r;
MUL value.r, value.r, f.r;
EX2 value.r, -value.r;
# alpha
SUB temp.a, 1.0, value.r;
# shade colors
MUL temp.r, temp.r, shadow.r;
MUL temp.g, temp.g, shadow.r;
MUL temp.b, temp.b, shadow.r;
MUL temp.r, temp.r, value.r;
MUL temp.g, temp.g, value.r;
MUL temp.b, temp.b, value.r;
# for now this just replace smoke shading if rendering fire
CMP result.color, render.r, temp, spec;
END
