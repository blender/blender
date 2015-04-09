!!ARBfp1.0
PARAM dx = program.local[0];
PARAM darkness = program.local[1];
PARAM render = program.local[2];
PARAM f = {1.442695041, 1.442695041, 1.442695041, 0.01};
TEMP temp, shadow, flame, spec, value;
TEX temp, fragment.texcoord[0], texture[0], 3D;
TEX shadow, fragment.texcoord[0], texture[1], 3D;
TEX flame, fragment.texcoord[0], texture[2], 3D;
TEX spec, flame.r, texture[3], 1D;
# calculate shading factor from density
MUL value.r, temp.a, darkness.a;
MUL value.r, value.r, dx.r;
MUL value.r, value.r, f.r;
EX2 temp, -value.r;
# alpha
SUB temp.a, 1.0, temp.r;
# shade colors
MUL temp.r, temp.r, shadow.r;
MUL temp.g, temp.g, shadow.r;
MUL temp.b, temp.b, shadow.r;
MUL temp.r, temp.r, darkness.r;
MUL temp.g, temp.g, darkness.g;
MUL temp.b, temp.b, darkness.b;
# for now this just replace smoke shading if rendering fire
CMP result.color, render.r, temp, spec;
END
