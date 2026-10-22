R"GLSL(
// Four continuous mathematical worlds. Included inside the scene shader string.
// No frame-random noise: all motion is phase-continuous and audio is smoothed.
vec3 jewel(float x) {
    vec3 c = 0.5 + 0.5*cos(6.2831853*(x + vec3(0.02,0.34,0.64)));
    c = pow(c, vec3(1.45));
    return mix(c, seedRamp(x), 0.5*uSeedMix);
}
float thread(float d, float width) {
    float aa = max(fwidth(d), 0.0005);
    return 1.0-smoothstep(width, width+aa*1.25, abs(d));
}
// An implicit (3,2) torus knot. The angular distance is measured in the
// tube's cross section; rotations preserve its topology.
float knotField(vec3 p) {
    float a = atan(p.y,p.x);
    vec2 q = vec2(length(p.xy)-1.45, p.z);
    float b = atan(q.y,q.x);
    float phase = 2.0*b-3.0*a;
    return length(vec2(length(q)-0.56, sin(phase*0.5)*0.30))-0.105;
}
// A conservative step estimate for a thickened gyroid level set, with
// a clear central passage so the camera can travel indefinitely.
float cathedralField(vec3 p) {
    vec3 q = p*1.65;
    float g = dot(sin(q),cos(q.yzx));
    float shell = (abs(g)-0.16)/5.8;
    float passage = 0.78-length(p.xy);
    return max(shell, passage);
}
vec3 rotateKnot(vec3 p, float t) {
    p.xz = rot(t*0.19)*p.xz;
    p.yz = rot(0.55+0.27*sin(t*0.13))*p.yz;
    p.xy = rot(t*0.08)*p.xy;
    return p;
}
float sacredField(vec3 p, int kind, float t) {
    return kind==6 ? cathedralField(p) : knotField(rotateKnot(p,t));
}
vec3 sacredSpace(vec2 uv, int kind) {
    float t = uSacredTime;
    vec3 ro = kind==6 ? vec3(0,0,t*0.36) : vec3(0,0,-4.9);
    vec3 rd = normalize(vec3(uv*1.7,1.35));
    if(kind==6) rd.xy = rot(0.12*sin(t*0.13))*rd.xy;
    float travel=0.0, fog=0.0;
    bool hit=false;
    vec3 q=ro;
    for(int i=0;i<88;i++) {
        q=ro+rd*travel;
        float d=sacredField(q,kind,t);
        fog += exp(-abs(d)*34.0)*0.006;
        if(d<0.0015*(1.0+travel*0.12)) { hit=true; break; }
        travel+=max(d*(kind==6 ? 0.80 : 0.50),0.002);
        if(travel>18.0) break;
    }
    vec3 col = vec3(0.006,0.003,0.022);
    float r=length(uv);
    col += jewel(0.64+t*0.012+r*0.25)*0.045/(0.15+r*r);
    if(hit) {
        vec2 e=vec2(0.002,0);
        vec3 n=normalize(vec3(sacredField(q+e.xyy,kind,t)-sacredField(q-e.xyy,kind,t),
                              sacredField(q+e.yxy,kind,t)-sacredField(q-e.yxy,kind,t),
                              sacredField(q+e.yyx,kind,t)-sacredField(q-e.yyx,kind,t)));
        vec3 local=kind==6?q:rotateKnot(q,t);
        float phase=kind==6 ? q.z*0.15+atan(q.y,q.x)*0.25 : atan(local.y,local.x)*0.48+local.z*0.24;
        float facing=max(dot(n,-rd),0.0);
        float fres=pow(1.0-facing,2.8);
        float light=max(dot(n,normalize(vec3(-0.6,0.8,-0.5))),0.0);
        float weave=sin(local.x*23.0+sin(local.z*7.0))*sin(local.y*23.0+sin(local.x*7.0));
        float veins=pow(0.5+0.5*sin(phase*65.0-t*0.8+weave*1.3),14.0);
        float pulse=0.5+0.5*sin(local.z*5.0-t*1.4);
        vec3 ink=jewel(phase+t*0.018+0.08*uSacredAudio);
        col=ink*(0.19+light*0.7)+jewel(phase+0.3)*(fres*1.2+veins*(0.6+0.4*pulse));
        col += pow(max(dot(reflect(rd,n),normalize(vec3(-0.4,0.7,-1))),0.0),40.0)*vec3(0.6,0.8,1.0);
        col*=exp(-travel*(kind==6?0.085:0.025));
    }
    col+=jewel(t*0.025+travel*0.075)*min(fog,0.55)*0.6;
    // Fine orbital halo behind the knot, with an optically quiet central void.
    if(kind==7 && !hit) {
        float a=atan(uv.y,uv.x);
        float halo=thread(r-0.36-0.014*sin(a*24.0+t*0.2),0.0018);
        col+=jewel(a/6.283+t*0.02)*halo*0.75;
    }
    // Phyllotaxis: golden-angle stars arranged in a deterministic spiral.
    // Only a few neighbouring rings are needed, keeping the cost bounded.
    if(kind==7) {
        for(int i=0;i<32;i++) {
            float k=float(i)+1.0;
            float angle=k*2.39996323+t*0.025;
            float radius=0.48+0.055*sqrt(k);
            vec2 center=radius*vec2(cos(angle),sin(angle));
            float d=length(uv-center);
            float star=exp(-d*d/0.000012);
            col+=jewel(k*0.0618+t*0.01)*star*0.55;
        }
    }
    return col;
}
vec3 lotus(vec2 p) {
    float t=uSacredTime;
    p=rot(t*0.025)*p;
    float r=max(length(p),0.0001), a=atan(p.y,p.x);
    float logR=log(r)*1.7+t*0.16;
    vec3 col=vec3(0.006,0.002,0.018);
    // Logarithmic radius makes every petal recur at every scale; integer
    // angular harmonics keep the polar seam exactly continuous.
    for(int i=0;i<7;i++) {
        float k=float(i), n=6.0+2.0*k;
        float angle=a+(i%2==0?1.0:-1.0)*t*0.035;
        float petal=sin(angle*n+0.4*sin(logR*2.0+t*0.16));
        float v=logR+k*0.49+0.28*cos(angle*n)+0.08*sin(angle*n*2.0-t*0.22);
        float d=sin(v*3.14159265);
        float line=thread(d,0.024+uSacredAudio*0.01);
        float glow=exp(-abs(d)*15.0)*0.13;
        float gate=0.45+0.55*pow(0.5+0.5*petal,2.0);
        col+=jewel(k*0.137+logR*0.10+t*0.013)*(line*0.65+glow)*gate;
    }
    // Translucent enamel between the filaments, lit by the radial harmonics.
    float enamel=pow(0.5+0.5*cos(logR*5.0+1.5*cos(a*12.0+t*0.12)),3.0);
    col+=jewel(logR*0.16+0.1*cos(a*6.0)+t*0.013)*enamel*0.24;
    // A softly breathing heart and two interleaved logarithmic spirals.
    float spiral=sin(a*12.0-logR*8.0+t*0.18);
    col+=jewel(logR*0.14+0.5)*thread(spiral,0.012)*0.16;
    col*=smoothstep(0.005,0.045,r);
    col+=jewel(t*0.02+0.15)*exp(-r*65.0)*0.7;
    return col;
}
vec3 quasicrystal(vec2 p) {
    float t=uSacredTime;
    p=rot(t*0.023)*p;
    p*=9.0+0.3*sin(t*0.11);
    p+=0.16*vec2(sin(p.y*0.7+t*0.15),cos(p.x*0.7-t*0.13));
    float f=0.0,g=0.0;
    // Five incommensurate plane waves: tenfold order without a repeating tile.
    for(int i=0;i<5;i++) {
        float a=float(i)*3.14159265/5.0;
        vec2 axis=vec2(cos(a),sin(a));
        f+=cos(dot(p,axis)*2.3+sin(t*0.12+float(i)*1.2566));
        g+=cos(dot(p,axis)*3.721478-t*0.11);
    }
    float field=f*0.45+g*0.18;
    float contour=sin(field*6.0+t*0.27);
    float ridge=thread(contour,0.055);
    vec3 col=jewel(field*0.18+t*0.018)*(0.07+0.3*pow(0.5+0.5*cos(field*3.0),4.0));
    col+=jewel(field*0.18+t*0.018+0.22)*ridge*0.75;
    col+=jewel(g*0.15+0.6)*pow(0.5+0.5*cos(f*2.0-g*1.5),24.0)*0.4;
    return col;
}
vec4 ineffable(vec2 p, int kind) {
    vec3 c=kind<8?sacredSpace(p,kind):(kind==8?lotus(p):quasicrystal(p));
    c*=uSacredIntensity;
    c*=1.0-0.22*smoothstep(0.2,1.2,length(p));
    return vec4(max(c,0.0),1.0);
}
)GLSL"
