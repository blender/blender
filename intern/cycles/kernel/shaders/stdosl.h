/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Sony Pictures Imageworks nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////////


#ifndef CCL_STDOSL_H
#define CCL_STDOSL_H


#ifndef M_PI
#define M_PI       3.1415926535897932        /* pi */
#define M_PI_2     1.5707963267948966        /* pi/2 */
#define M_PI_4     0.7853981633974483        /* pi/4 */
#define M_2_PI     0.6366197723675813        /* 2/pi */
#define M_2PI      6.2831853071795865        /* 2*pi */
#define M_4PI     12.566370614359173         /* 4*pi */
#define M_2_SQRTPI 1.1283791670955126        /* 2/sqrt(pi) */
#define M_E        2.7182818284590452        /* e (Euler's number) */
#define M_LN2      0.6931471805599453        /* ln(2) */
#define M_LN10     2.3025850929940457        /* ln(10) */
#define M_LOG2E    1.4426950408889634        /* log_2(e) */
#define M_LOG10E   0.4342944819032518        /* log_10(e) */
#define M_SQRT2    1.4142135623730950        /* sqrt(2) */
#define M_SQRT1_2  0.7071067811865475        /* 1/sqrt(2) */
#endif



// Declaration of built-in functions and closures
#define BUILTIN [[ int builtin = 1 ]]
#define BUILTIN_DERIV [[ int builtin = 1, int deriv = 1 ]]

#define PERCOMP1(name)                          \
    normal name (normal x) BUILTIN;             \
    vector name (vector x) BUILTIN;             \
    point  name (point x) BUILTIN;              \
    color  name (color x) BUILTIN;              \
    float  name (float x) BUILTIN;

#define PERCOMP2(name)                          \
    normal name (normal x, normal y) BUILTIN;   \
    vector name (vector x, vector y) BUILTIN;   \
    point  name (point x, point y) BUILTIN;     \
    color  name (color x, color y) BUILTIN;     \
    float  name (float x, float y) BUILTIN;

#define PERCOMP2F(name)                         \
    normal name (normal x, float y) BUILTIN;    \
    vector name (vector x, float y) BUILTIN;    \
    point  name (point x, float y) BUILTIN;     \
    color  name (color x, float y) BUILTIN;     \
    float  name (float x, float y) BUILTIN;


// Basic math
normal degrees (normal x) { return x*(180.0/M_PI); }
vector degrees (vector x) { return x*(180.0/M_PI); }
point  degrees (point x)  { return x*(180.0/M_PI); }
color  degrees (color x)  { return x*(180.0/M_PI); }
float  degrees (float x)  { return x*(180.0/M_PI); }
normal radians (normal x) { return x*(M_PI/180.0); }
vector radians (vector x) { return x*(M_PI/180.0); }
point  radians (point x)  { return x*(M_PI/180.0); }
color  radians (color x)  { return x*(M_PI/180.0); }
float  radians (float x)  { return x*(M_PI/180.0); }
PERCOMP1 (cos)
PERCOMP1 (sin)
PERCOMP1 (tan)
PERCOMP1 (acos)
PERCOMP1 (asin)
PERCOMP1 (atan)
PERCOMP2 (atan2)
PERCOMP1 (cosh)
PERCOMP1 (sinh)
PERCOMP1 (tanh)
PERCOMP2F (pow)
PERCOMP1 (exp)
PERCOMP1 (exp2)
PERCOMP1 (expm1)
PERCOMP1 (log)
point  log (point a,  float b) { return log(a)/log(b); }
vector log (vector a, float b) { return log(a)/log(b); }
color  log (color a,  float b) { return log(a)/log(b); }
float  log (float a,  float b) { return log(a)/log(b); }
PERCOMP1 (log2)
PERCOMP1 (log10)
PERCOMP1 (logb)
PERCOMP1 (sqrt)
PERCOMP1 (inversesqrt)
float hypot (float a, float b) { return sqrt (a*a + b*b); }
float hypot (float a, float b, float c) { return sqrt (a*a + b*b + c*c); }
PERCOMP1 (abs)
int abs (int x) BUILTIN;
PERCOMP1 (fabs)
int fabs (int x) BUILTIN;
PERCOMP1 (sign)
PERCOMP1 (floor)
PERCOMP1 (ceil)
PERCOMP1 (round)
PERCOMP1 (trunc)
PERCOMP2 (fmod)
PERCOMP2F (fmod)
int    mod (int    a, int    b) { return a - b*(int)floor(a/b); }
point  mod (point  a, point  b) { return a - b*floor(a/b); }
vector mod (vector a, vector b) { return a - b*floor(a/b); }
normal mod (normal a, normal b) { return a - b*floor(a/b); }
color  mod (color  a, color  b) { return a - b*floor(a/b); }
point  mod (point  a, float  b) { return a - b*floor(a/b); }
vector mod (vector a, float  b) { return a - b*floor(a/b); }
normal mod (normal a, float  b) { return a - b*floor(a/b); }
color  mod (color  a, float  b) { return a - b*floor(a/b); }
float  mod (float  a, float  b) { return a - b*floor(a/b); }
PERCOMP2 (min)
int min (int a, int b) BUILTIN;
PERCOMP2 (max)
int max (int a, int b) BUILTIN;
normal clamp (normal x, normal minval, normal maxval) { return max(min(x,maxval),minval); }
vector clamp (vector x, vector minval, vector maxval) { return max(min(x,maxval),minval); }
point  clamp (point x, point minval, point maxval) { return max(min(x,maxval),minval); }
color  clamp (color x, color minval, color maxval) { return max(min(x,maxval),minval); }
float  clamp (float x, float minval, float maxval) { return max(min(x,maxval),minval); }
int    clamp (int x, int minval, int maxval) { return max(min(x,maxval),minval); }
#if 0
normal mix (normal x, normal y, normal a) { return x*(1-a) + y*a; }
normal mix (normal x, normal y, float  a) { return x*(1-a) + y*a; }
vector mix (vector x, vector y, vector a) { return x*(1-a) + y*a; }
vector mix (vector x, vector y, float  a) { return x*(1-a) + y*a; }
point  mix (point  x, point  y, point  a) { return x*(1-a) + y*a; }
point  mix (point  x, point  y, float  a) { return x*(1-a) + y*a; }
color  mix (color  x, color  y, color  a) { return x*(1-a) + y*a; }
color  mix (color  x, color  y, float  a) { return x*(1-a) + y*a; }
float  mix (float  x, float  y, float  a) { return x*(1-a) + y*a; }
#else
normal mix (normal x, normal y, normal a) BUILTIN;
normal mix (normal x, normal y, float  a) BUILTIN;
vector mix (vector x, vector y, vector a) BUILTIN;
vector mix (vector x, vector y, float  a) BUILTIN;
point  mix (point  x, point  y, point  a) BUILTIN;
point  mix (point  x, point  y, float  a) BUILTIN;
color  mix (color  x, color  y, color  a) BUILTIN;
color  mix (color  x, color  y, float  a) BUILTIN;
float  mix (float  x, float  y, float  a) BUILTIN;
#endif
int isnan (float x) BUILTIN;
int isinf (float x) BUILTIN;
int isfinite (float x) BUILTIN;
float erf (float x) BUILTIN;
float erfc (float x) BUILTIN;

// Vector functions

vector cross (vector a, vector b) BUILTIN;
float dot (vector a, vector b) BUILTIN;
float length (vector v) BUILTIN;
float distance (point a, point b) BUILTIN;
float distance (point a, point b, point q)
{
    vector d = b - a;
    float dd = dot(d, d);
    if(dd == 0.0)
        return distance(q, a);
    float t = dot(q - a, d)/dd;
    return distance(q, a + clamp(t, 0.0, 1.0)*d);
}
normal normalize (normal v) BUILTIN;
vector normalize (vector v) BUILTIN;
vector faceforward (vector N, vector I, vector Nref) BUILTIN;
vector faceforward (vector N, vector I) BUILTIN;
vector reflect (vector I, vector N) { return I - 2*dot(N,I)*N; }
vector refract (vector I, vector N, float eta) {
    float IdotN = dot (I, N);
    float k = 1 - eta*eta * (1 - IdotN*IdotN);
    return (k < 0) ? vector(0,0,0) : (eta*I - N * (eta*IdotN + sqrt(k)));
}
void fresnel (vector I, normal N, float eta,
              output float Kr, output float Kt,
              output vector R, output vector T)
{
    float sqr(float x) { return x*x; }
    float c = dot(I, N);
    if (c < 0)
        c = -c;
    R = reflect(I, N);
    float g = 1.0 / sqr(eta) - 1.0 + c * c;
    if (g >= 0.0) {
        g = sqrt (g);
        float beta = g - c;
        float F = (c * (g+c) - 1.0) / (c * beta + 1.0);
        F = 0.5 * (1.0 + sqr(F));
        F *= sqr (beta / (g+c));
        Kr = F;
        Kt = (1.0 - Kr) * eta*eta;
        // OPT: the following recomputes some of the above values, but it 
        // gives us the same result as if the shader-writer called refract()
        T = refract(I, N, eta);
    } else {
        // total internal reflection
        Kr = 1.0;
        Kt = 0.0;
        T = vector (0,0,0);
    }
}

void fresnel (vector I, normal N, float eta,
              output float Kr, output float Kt)
{
    vector R, T;
    fresnel(I, N, eta, Kr, Kt, R, T);
}


normal transform (matrix Mto, normal p) BUILTIN;
vector transform (matrix Mto, vector p) BUILTIN;
point  transform (matrix Mto, point p) BUILTIN;
normal transform (string from, string to, normal p) BUILTIN;
vector transform (string from, string to, vector p) BUILTIN;
point  transform (string from, string to, point p) BUILTIN;
normal transform (string to, normal p) { return transform("common",to,p); }
vector transform (string to, vector p) { return transform("common",to,p); }
point  transform (string to, point p)  { return transform("common",to,p); }

float transformu (string tounits, float x) BUILTIN;
float transformu (string fromunits, string tounits, float x) BUILTIN;

point rotate (point p, float angle, point a, point b)
{
    vector axis = normalize (b - a);
    float cosang, sinang;
    sincos (angle, sinang, cosang);
    float cosang1 = 1.0 - cosang;
    float x = axis[0], y = axis[1], z = axis[2];
    matrix M = matrix (x * x + (1.0 - x * x) * cosang,
                       x * y * cosang1 + z * sinang,
                       x * z * cosang1 - y * sinang,
                       0.0,
                       x * y * cosang1 - z * sinang,
                       y * y + (1.0 - y * y) * cosang,
                       y * z * cosang1 + x * sinang,
                       0.0,
                       x * z * cosang1 + y * sinang,
                       y * z * cosang1 - x * sinang,
                       z * z + (1.0 - z * z) * cosang,
                       0.0,
                       0.0, 0.0, 0.0, 1.0);
    return transform (M, p-a) + a;
}



// Color functions

float luminance (color c) BUILTIN;
color blackbody (float temperatureK) BUILTIN;
color wavelength_color (float wavelength_nm) BUILTIN;


color transformc (string to, color x)
{
    color rgb_to_hsv (color rgb) {  // See Foley & van Dam
        float r = rgb[0], g = rgb[1], b = rgb[2];
        float mincomp = min (r, min (g, b));
        float maxcomp = max (r, max (g, b));
        float delta = maxcomp - mincomp;  // chroma
        float h, s, v;
        v = maxcomp;
        if (maxcomp > 0)
            s = delta / maxcomp;
        else s = 0;
        if (s <= 0)
            h = 0;
        else {
            if      (r >= maxcomp) h = (g-b) / delta;
            else if (g >= maxcomp) h = 2 + (b-r) / delta;
            else                   h = 4 + (r-g) / delta;
            h /= 6;
            if (h < 0)
                h += 1;
        }
        return color (h, s, v);
    }

    color rgb_to_hsl (color rgb) {  // See Foley & van Dam
        // First convert rgb to hsv, then to hsl
        float minval = min (rgb[0], min (rgb[1], rgb[2]));
        color hsv = rgb_to_hsv (rgb);
        float maxval = hsv[2];   // v == maxval
        float h = hsv[0], s, l = (minval+maxval) / 2;
        if (minval == maxval)
            s = 0;  // special 'achromatic' case, hue is 0
        else if (l <= 0.5)
            s = (maxval - minval) / (maxval + minval);
        else
            s = (maxval - minval) / (2 - maxval - minval);
        return color (h, s, l);
    }

    color r;
    if (to == "rgb" || to == "RGB")
        r = x;
    else if (to == "hsv")
        r = rgb_to_hsv (x);
    else if (to == "hsl")
        r = rgb_to_hsl (x);
    else if (to == "YIQ")
        r = color (dot (vector(0.299,  0.587,  0.114), (vector)x),
                   dot (vector(0.596, -0.275, -0.321), (vector)x),
                   dot (vector(0.212, -0.523,  0.311), (vector)x));
    else if (to == "XYZ")
        r = color (dot (vector(0.412453, 0.357580, 0.180423), (vector)x),
                   dot (vector(0.212671, 0.715160, 0.072169), (vector)x),
                   dot (vector(0.019334, 0.119193, 0.950227), (vector)x));
    else {
        error ("Unknown color space \"%s\"", to);
        r = x;
    }
    return r;
}


color transformc (string from, string to, color x)
{
    color hsv_to_rgb (color c) { // Reference: Foley & van Dam
        float h = c[0], s = c[1], v = c[2];
        color r;
        if (s < 0.0001) {
            r = v;
        } else {
            h = 6 * (h - floor(h));  // expand to [0..6)
            int hi = (int)h;
            float f = h - hi;
            float p = v * (1-s);
            float q = v * (1-s*f);
            float t = v * (1-s*(1-f));
            if      (hi == 0) r = color (v, t, p);
            else if (hi == 1) r = color (q, v, p);
            else if (hi == 2) r = color (p, v, t);
            else if (hi == 3) r = color (p, q, v);
            else if (hi == 4) r = color (t, p, v);
            else              r = color (v, p, q);
        }
        return r;
    }

    color hsl_to_rgb (color c) {
        float h = c[0], s = c[1], l = c[2];
        // Easiest to convert hsl -> hsv, then hsv -> RGB (per Foley & van Dam)
        float v = (l <= 0.5) ? (l * (1 + s)) : (l * (1 - s) + s);
        color r;
        if (v <= 0) {
            r = 0;
        } else {
            float min = 2 * l - v;
            s = (v - min) / v;
            r = hsv_to_rgb (color (h, s, v));
        }
        return r;
    }

    color r;
    if (from == "rgb" || from == "RGB")
        r = x;
    else if (from == "hsv")
        r = hsv_to_rgb (x);
    else if (from == "hsl")
        r = hsl_to_rgb (x);
    else if (from == "YIQ")
        r = color (dot (vector(1,  0.9557,  0.6199), (vector)x),
                   dot (vector(1, -0.2716, -0.6469), (vector)x),
                   dot (vector(1, -1.1082,  1.7051), (vector)x));
    else if (from == "XYZ")
        r = color (dot (vector( 3.240479, -1.537150, -0.498535), (vector)x),
                   dot (vector(-0.969256,  1.875991,  0.041556), (vector)x),
                   dot (vector( 0.055648, -0.204043,  1.057311), (vector)x));
    else {
        error ("Unknown color space \"%s\"", to);
        r = x;
    }
    return transformc (to, r);
}

 

// Matrix functions

float determinant (matrix m) BUILTIN;
matrix transpose (matrix m) BUILTIN;



// Pattern generation

color step (color edge, color x) BUILTIN;
point step (point edge, point x) BUILTIN;
vector step (vector edge, vector x) BUILTIN;
normal step (normal edge, normal x) BUILTIN;
float step (float edge, float x) BUILTIN;
float smoothstep (float edge0, float edge1, float x) BUILTIN;

float aastep (float edge, float s, float dedge, float ds) {
    // Box filtered AA step
    float width = fabs(dedge) + fabs(ds);
    float halfwidth = 0.5*width;
    float e1 = edge-halfwidth;
    return (s <= e1) ? 0.0 : ((s >= (edge+halfwidth)) ? 1.0 : (s-e1)/width);
}
float aastep (float edge, float s, float ds) {
    return aastep (edge, s, filterwidth(edge), ds);
}
float aastep (float edge, float s) {
    return aastep (edge, s, filterwidth(edge), filterwidth(s));
}


// Derivatives and area operators


// Displacement functions


// String functions

int strlen (string s) BUILTIN;
int startswith (string s, string prefix) BUILTIN;
int endswith (string s, string suffix) BUILTIN;
string substr (string s, int start, int len) BUILTIN;
string substr (string s, int start) { return substr (s, start, strlen(s)); }
float stof (string str) BUILTIN;
int stoi (string str) BUILTIN;

// Define concat in terms of shorter concat
string concat (string a, string b, string c) {
    return concat(concat(a,b), c);
}
string concat (string a, string b, string c, string d) {
    return concat(concat(a,b,c), d);
}
string concat (string a, string b, string c, string d, string e) {
    return concat(concat(a,b,c,d), e);
}
string concat (string a, string b, string c, string d, string e, string f) {
    return concat(concat(a,b,c,d,e), f);
}


// Texture


// Closures

closure color diffuse(normal N) BUILTIN;
closure color oren_nayar(normal N, float sigma) BUILTIN;
closure color diffuse_ramp(normal N, color colors[8]) BUILTIN;
closure color phong_ramp(normal N, float exponent, color colors[8]) BUILTIN;
closure color diffuse_toon(normal N, float size, float smooth) BUILTIN;
closure color glossy_toon(normal N, float size, float smooth) BUILTIN;
closure color translucent(normal N) BUILTIN;
closure color reflection(normal N) BUILTIN;
closure color refraction(normal N, float eta) BUILTIN;
closure color transparent() BUILTIN;
closure color microfacet_ggx(normal N, float ag) BUILTIN;
closure color microfacet_ggx_aniso(normal N, vector T, float ax, float ay) BUILTIN;
closure color microfacet_ggx_refraction(normal N, float ag, float eta) BUILTIN;
closure color microfacet_beckmann(normal N, float ab) BUILTIN;
closure color microfacet_beckmann_aniso(normal N, vector T, float ax, float ay) BUILTIN;
closure color microfacet_beckmann_refraction(normal N, float ab, float eta) BUILTIN;
closure color ashikhmin_shirley(normal N, vector T,float ax, float ay) BUILTIN;
closure color ashikhmin_velvet(normal N, float sigma) BUILTIN;
closure color emission() BUILTIN;
closure color background() BUILTIN;
closure color holdout() BUILTIN;
closure color ambient_occlusion() BUILTIN;

// BSSRDF
closure color bssrdf_cubic(normal N, vector radius, float texture_blur, float sharpness) BUILTIN;
closure color bssrdf_gaussian(normal N, vector radius, float texture_blur) BUILTIN;

// Hair
closure color hair_reflection(normal N, float roughnessu, float roughnessv, vector T, float offset) BUILTIN;
closure color hair_transmission(normal N, float roughnessu, float roughnessv, vector T, float offset) BUILTIN;

// Volume
closure color henyey_greenstein(float g) BUILTIN;
closure color absorption() BUILTIN;

// Renderer state
int backfacing () BUILTIN;
int raytype (string typename) BUILTIN;
// the individual 'isFOOray' functions are deprecated
int iscameraray () { return raytype("camera"); }
int isdiffuseray () { return raytype("diffuse"); }
int isglossyray () { return raytype("glossy"); }
int isshadowray () { return raytype("shadow"); }
int getmatrix (string fromspace, string tospace, output matrix M) BUILTIN;
int getmatrix (string fromspace, output matrix M) {
    return getmatrix (fromspace, "common", M);
}


// Miscellaneous




#undef BUILTIN
#undef BUILTIN_DERIV
#undef PERCOMP1
#undef PERCOMP2
#undef PERCOMP2F

#endif /* CCL_STDOSL_H */
