#pragma once
// Dedicated program for the new worlds: avoids evaluating the original scene
// under an opaque mathematical world and keeps software-driver JIT manageable.
static const char* const kIneffableScene = R"GLSL(#version 330 core
out vec4 frag;
uniform vec2 uRes;
uniform float uF3Time, uSacredTime, uSacredIntensity, uSacredAudio;
uniform float uDimA, uDimB, uDimMorph, uSeedMix;
uniform vec3 uSeedPal[3];
mat2 rot(float a) { float c=cos(a),s=sin(a); return mat2(c,-s,s,c); }
vec3 seedRamp(float t) {
    t=fract(t)*3.0;
    vec3 c=t<1.0?mix(uSeedPal[0],uSeedPal[1],t):
        (t<2.0?mix(uSeedPal[1],uSeedPal[2],t-1.0):mix(uSeedPal[2],uSeedPal[0],t-2.0));
    return c/max(max(c.r,c.g),max(c.b,0.08))*0.95;
}
)GLSL"
#include "Ineffable.glsl"
R"GLSL(
void main() {
    vec2 p=(gl_FragCoord.xy-0.5*uRes)/uRes.y;
    vec3 c=ineffable(p,int(uDimB+0.5)).rgb;
    if(uDimMorph<0.999) {
        float blend=smoothstep(0.0,1.0,uDimMorph);
        vec3 a=ineffable(p,int(uDimA+0.5)).rgb;
        // Blend radiance with a gentle mid-transition lift.
        c=mix(a,c,blend)*(1.0+0.15*sin(blend*3.14159265));
    }
    if(any(isnan(c)) || any(isinf(c))) c=vec3(0);
    frag=vec4(c,1);
}
)GLSL";
