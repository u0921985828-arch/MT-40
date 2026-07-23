#!/usr/bin/env python3
# Build a self-contained ST-40 emulator HTML with real Web Audio synthesis
# (a faithful JS port of the plugin's DSP), driven by the existing panel UI.
#
# Audio path: prefers an AudioWorklet; if the worklet module fails to load
# (e.g. Blob-URL worklets are blocked on file://), it transparently falls back
# to a ScriptProcessorNode running the SAME DSP on the main thread — so it
# makes sound whether opened locally (file://) or served over http.
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = open(os.path.join(ROOT, "ui", "index.html")).read()
SCR = os.path.join(ROOT, "dist")
os.makedirs(SCR, exist_ok=True)
OUT = os.path.join(SCR, "st40-emulator-sound.html")

# ----------------------------------------------------------------- shared CORE
# Pure-JS DSP: identical text is embedded in the worklet realm AND the page
# realm (worklets run in a separate realm, so the code must exist in both).
CORE = r'''
// [duty1,duty2,detuneCents,lpfHz,attack,decay,sustain,release,gain]
// Percussive tones use sustain 0 (they ring out and die while held);
// sustained tones hold at their sustain level until the key is released.
const PRESETS=[
[0.50,0.25,6,3000,0.002,1.60,0.00,0.35,0.85], // 1 electric piano  (struck, long decay)
[0.20,0.12,7,3600,0.001,0.35,0.00,0.14,0.80], // 2 banjo           (plucked, short)
[0.40,0.20,6,2800,0.001,0.90,0.00,0.22,0.82], // 3 guitar          (plucked)
[0.25,0.15,6,4000,0.001,0.60,0.00,0.18,0.80], // 4 harpsichord     (plucked)
[0.50,0.50,4,3200,0.001,0.45,0.00,0.15,0.74], // 5 xylophone       (mallet ping)
[0.50,0.33,5,2800,0.001,0.90,0.00,0.20,0.74], // 6 celesta         (bell)
[0.30,0.30,5,3800,0.001,1.40,0.00,0.25,0.72], // 7 glockenspiel    (bell, long)
[0.50,0.50,4,2600,0.004,0.05,1.00,0.10,0.84], // 8 organ           (sustained)
[0.40,0.40,10,2600,0.012,0.08,0.95,0.16,0.80],// 9 accordion
[0.50,0.50,3,3600,0.004,0.05,1.00,0.09,0.85], //10 pipe organ
[0.33,0.33,6,2200,0.010,0.10,0.90,0.16,0.80], //11 oriental pipe
[0.35,0.20,7,3600,0.030,0.12,0.90,0.20,0.82], //12 brass          (slow-ish onset)
[0.125,0.20,9,1600,0.060,0.15,0.90,0.30,0.74],//13 cello          (bowed)
[0.15,0.30,14,3200,0.006,0.10,0.95,0.22,0.72],//14 synth fuzz
[0.125,0.125,8,3000,0.070,0.15,0.90,0.28,0.74],//15 violin         (bowed)
[0.35,0.20,7,4000,0.010,0.10,0.92,0.20,0.82], //16 trumpet
[0.12,0.35,16,2600,0.006,0.10,0.90,0.30,0.70],//17 funny fuzz
[0.125,0.125,12,2800,0.090,0.20,0.90,0.40,0.72],//18 st. ensemble  (slow swell)
[0.50,0.50,5,2000,0.030,0.08,0.95,0.24,0.80], //19 clarinet
[0.50,0.50,4,1200,0.050,0.10,0.90,0.26,0.80], //20 flute
[0.50,0.45,3,1600,0.035,0.08,0.92,0.22,0.78], //21 recorder
[0.50,0.40,5,1400,0.050,0.10,0.90,0.28,0.76]  //22 folk flute
].map(a=>({d1:a[0],d2:a[1],det:a[2],lpf:a[3],atk:a[4],dec:a[5],sus:a[6],rel:a[7],gain:a[8]}));

const REST=-128;
// 6 rhythm patterns (index: 0 Samba,1 Waltz,2 Swing,3 Slow Rock,4 Pops,5 Rock)
const PAT=[
 {k:[1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0],
  s:[0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0],
  hc:[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
  b:[0,REST,REST,REST,REST,REST,7,REST,REST,REST,REST,REST,5,REST,REST,REST,REST,REST,3,REST,REST,REST,REST,REST,0,REST,REST,REST,REST,REST,7,REST]},
 {k:[1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
  s:[0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0],
  hc:[1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
  b:[0,REST,REST,REST,7,REST,REST,REST,7,REST,REST,REST,REST,REST,REST,REST,0,REST,REST,REST,7,REST,REST,REST,7,REST,REST,REST,REST,REST,REST,REST]},
 {k:[1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0],
  s:[0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
  hc:[1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0],
  b:[0,REST,REST,REST,3,REST,REST,REST,5,REST,REST,REST,7,REST,REST,REST,0,REST,REST,REST,3,REST,REST,REST,5,REST,REST,REST,7,REST,REST,REST]},
 {k:[1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
  s:[0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
  hc:[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0],
  b:[0,REST,REST,REST,0,REST,REST,REST,7,REST,REST,REST,5,REST,REST,REST,0,REST,REST,REST,0,REST,REST,REST,7,REST,REST,REST,3,REST,REST,REST]},
 {k:[1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0],
  s:[0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1],
  hc:[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
  b:[0,REST,REST,REST,REST,REST,7,REST,0,REST,REST,REST,5,REST,REST,REST,0,REST,REST,REST,REST,REST,7,REST,0,REST,REST,REST,5,REST,3,REST]},
 // Index 5 = "Rock" — the MT-40 pattern that became the Sleng Teng riddim.
 // Bassline in E minor (root-relative offsets 0=E, 3=G, 5=A, 7=B), the
 // ascending/descending 4-note climb; steppers kick 1&3, snare 2&4, hats on
 // eighths, open hat as the bar-2 turnaround. Default tempo 82 BPM, root E2.
 {k:[1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
  s:[0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
  hc:[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0],
  ho:[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0],
  b:[0,REST,REST,0,REST,REST,3,REST,5,REST,REST,5,REST,7,REST,REST,0,REST,REST,0,REST,REST,3,REST,5,REST,7,REST,5,REST,3,REST]}
];

class Biquad{constructor(){this.b0=1;this.b1=0;this.b2=0;this.a1=0;this.a2=0;this.x1=0;this.x2=0;this.y1=0;this.y2=0;}
 reset(){this.x1=this.x2=this.y1=this.y2=0;}
 bp(fs,fc,q){const w=2*Math.PI*fc/fs,al=Math.sin(w)/(2*q),cw=Math.cos(w),a0=1+al;this.b0=q*al/a0;this.b1=0;this.b2=-q*al/a0;this.a1=-2*cw/a0;this.a2=(1-al)/a0;}
 hp(fs,fc,q){const w=2*Math.PI*fc/fs,al=Math.sin(w)/(2*q),cw=Math.cos(w),a0=1+al;this.b0=(1+cw)/2/a0;this.b1=-(1+cw)/a0;this.b2=(1+cw)/2/a0;this.a1=-2*cw/a0;this.a2=(1-al)/a0;}
 lp(fs,fc,q){const w=2*Math.PI*fc/fs,al=Math.sin(w)/(2*q),cw=Math.cos(w),a0=1+al;this.b0=(1-cw)/2/a0;this.b1=(1-cw)/a0;this.b2=(1-cw)/2/a0;this.a1=-2*cw/a0;this.a2=(1-al)/a0;}
 p(x){const y=this.b0*x+this.b1*this.x1+this.b2*this.x2-this.a1*this.y1-this.a2*this.y2;this.x2=this.x1;this.x1=x;this.y2=this.y1;this.y1=y;return y;}}
class OnePole{constructor(fs){this.fs=fs;this.a=0;this.b=1;this.z=0;this.set(4000);}
 set(fc){this.a=Math.exp(-2*Math.PI*fc/this.fs);this.b=1-this.a;} reset(){this.z=0;} p(x){this.z=this.b*x+this.a*this.z;return this.z;}}
class LFSR{constructor(s){this.s=(s||0xACE1)>>>0;} next(){let lsb=this.s&1;this.s>>>=1;if(lsb)this.s^=0x80200003;this.s>>>=0;return (this.s&0xFFFFFF)/8388608-1;}}
// Attack-Decay-Sustain-Release. Percussive MT-40 tones (sus=0) decay to
// silence while held (xylophone/harpsichord/banjo ping); sustained tones
// (sus~0.9-1) hold until release. States: 1 atk, 2 dec, 3 sus, 4 rel.
class Env2{constructor(fs){this.fs=fs;this.atk=0.005;this.dec=0.1;this.sus=1;this.rel=0.3;this.mult=1;this.ai=0.01;this.dc=0.999;this.rc=0.999;this.lvl=0;this.st=0;}
 times(a,d,s,r){this.atk=a;this.dec=d;this.sus=s;this.rel=r;this.ai=(a*this.fs>1)?1/(a*this.fs):1;this.dc=d>0?Math.exp(-6.9077553/(d*this.fs)):0;this.rcf();}
 setMult(m){this.mult=m;this.rcf();} rcf(){const t=this.rel*this.mult;this.rc=t>0?Math.exp(-6.9077553/(t*this.fs)):0;}
 on(){this.st=1;} off(){if(this.st!==0)this.st=4;} reset(){this.lvl=0;this.st=0;} active(){return this.st!==0;}
 n(){if(this.st===1){this.lvl+=this.ai;if(this.lvl>=1){this.lvl=1;this.st=2;}}
  else if(this.st===2){this.lvl=this.sus+(this.lvl-this.sus)*this.dc;if(this.lvl-this.sus<=1e-4){this.lvl=this.sus;this.st=(this.sus<=1e-4)?0:3;}}
  else if(this.st===4){this.lvl*=this.rc;if(this.lvl<=1e-4){this.lvl=0;this.st=0;}}
  return this.lvl;}}
class Exp{constructor(fs){this.fs=fs;this.dec=0.1;this.c=0.999;this.lvl=0;}
 set(d){this.dec=d;this.c=d>0?Math.exp(-6.9077553/(d*this.fs)):0;} trig(){this.lvl=1;} choke(){this.lvl=0;} active(){return this.lvl>1e-4;}
 n(){const o=this.lvl;this.lvl*=this.c;if(this.lvl<1e-5)this.lvl=0;return o;}}
class Voice{constructor(fs){this.fs=fs;this.lpf=new OnePole(fs);this.env=new Env2(fs);this.p1=0;this.p2=0;this.f=440;this.vel=1;this.note=-1;this.act=false;this.age=0;this.pr=PRESETS[0];}
 setPreset(p){this.pr=p;this.lpf.set(p.lpf);this.env.times(p.atk,p.dec,p.sus,p.rel);} setMult(m){this.env.setMult(m);}
 on(n,v){this.note=n;this.vel=v;this.f=440*Math.pow(2,(n-69)/12);this.p1=0;this.p2=0;this.lpf.reset();this.env.times(this.pr.atk,this.pr.dec,this.pr.sus,this.pr.rel);this.env.on();this.act=true;this.age=0;}
 off(){this.env.off();}
 render(lfo,depth){if(!this.act)return 0;const det=Math.pow(2,this.pr.det/1200),vib=Math.pow(2,(lfo*depth)/12);
  const i1=this.f*vib/this.fs,i2=this.f*det*vib/this.fs;const a=(this.p1<this.pr.d1)?1:-1,b=(this.p2<this.pr.d2)?1:-1;
  this.p1+=i1;if(this.p1>=1)this.p1-=1;this.p2+=i2;if(this.p2>=1)this.p2-=1;
  let raw=0.5*(a+b);raw=this.lpf.p(raw);const e=this.env.n();if(!this.env.active())this.act=false;return raw*e*this.vel*this.pr.gain;}}
class Alloc{constructor(fs){this.v=[];for(let i=0;i<8;i++)this.v.push(new Voice(fs));}
 setPreset(p){this.v.forEach(x=>x.setPreset(p));} setMult(m){this.v.forEach(x=>x.setMult(m));}
 on(n,vel){for(const x of this.v)if(x.act&&x.note===n){x.on(n,vel);return;}for(const x of this.v)if(!x.act){x.on(n,vel);return;}let o=this.v[0];for(const x of this.v)if(x.age>o.age)o=x;o.on(n,vel);}
 off(n){for(const x of this.v)if(x.act&&x.note===n)x.off();} allOff(){this.v.forEach(x=>x.off());}
 render(lfo,depth){let s=0;for(const x of this.v){if(x.act){s+=x.render(lfo,depth);x.age++;}}return s;}}
class Kick{constructor(fs){this.fs=fs;this.bp=new Biquad();this.bp.bp(fs,55,18.5);this.imp=0;this.act=false;this.mag=0;this.drive=4;}
 trig(){this.bp.reset();this.imp=Math.max(1,(0.001*this.fs)|0);this.act=true;}
 p(){if(!this.act)return 0;let x=0;if(this.imp>0){x=1;this.imp--;}let y=this.bp.p(x);y=Math.tanh(this.drive*y);this.mag=0.999*this.mag+0.001*Math.abs(y);if(this.imp===0&&this.mag<1e-4)this.act=false;return y;}}
class Snare{constructor(fs,noise){this.fs=fs;this.noise=noise;this.body=new Biquad();this.body.bp(fs,220,6);this.hpf=new Biquad();this.hpf.hp(fs,2500,0.707);this.be=new Exp(fs);this.ne=new Exp(fs);this.be.set(0.09);this.ne.set(0.15);this.imp=0;}
 trig(){this.body.reset();this.imp=Math.max(1,(0.001*this.fs)|0);this.be.trig();this.ne.trig();}
 active(){return this.be.active()||this.ne.active();}
 p(){if(!this.active()&&this.imp===0)return 0;let x=0;if(this.imp>0){x=1;this.imp--;}const body=this.body.p(x)*this.be.n()*0.4;const w=this.hpf.p(this.noise.next())*this.ne.n()*0.85;return body+w;}}
class Hat{constructor(fs,noise){this.fs=fs;this.noise=noise;this.h1=new Biquad();this.h1.hp(fs,8000,0.5412);this.h2=new Biquad();this.h2.hp(fs,8000,1.3066);this.ce=new Exp(fs);this.oe=new Exp(fs);this.ce.set(0.04);this.oe.set(0.30);}
 closed(){if(this.oe.active())this.oe.choke();this.ce.trig();} open(){this.oe.trig();} active(){return this.ce.active()||this.oe.active();}
 p(){if(!this.active())return 0;let n=this.noise.next();n=this.h1.p(n);n=this.h2.p(n);return n*(this.ce.n()+this.oe.n());}}
// Deep "digital" MT-40 bass: pulse wave, resonant LPF for body, longer decay
// so eighth notes sustain and connect; slight drive for punch.
class Bass{constructor(fs){this.fs=fs;this.lpf=new Biquad();this.lpf.lp(fs,1300,1.1);this.env=new Exp(fs);this.env.set(0.40);this.ph=0;this.inc=0;this.ag=0;this.ai=0;}
 trig(n){this.inc=440*Math.pow(2,(n-69)/12)/this.fs;this.ph=0;this.env.trig();this.ag=0;this.ai=1/(0.004*this.fs);}
 active(){return this.env.active();}
 p(){if(!this.env.active())return 0;const sq=(this.ph<0.45)?1:-1;this.ph+=this.inc;if(this.ph>=1)this.ph-=1;if(this.ag<1){this.ag+=this.ai;if(this.ag>1)this.ag=1;}return Math.tanh(1.4*this.lpf.p(sq))*this.env.n()*this.ag*1.15;}}
function detectChord(held){if(!held.length)return null;let root=held[0];for(const n of held)if(n>root)root=n;let add=0;for(const n of held)if(n<root)add++;
 let t;if(add===0)t=[root,root+4,root+7];else if(add===1)t=[root,root+3,root+7];else if(add===2)t=[root,root+4,root+7,root+10];else t=[root,root+3,root+7,root+10];return {root:root,tones:t};}
class Rhythm{constructor(fs){this.fs=fs;this.noise=new LFSR(0xACE1);this.kick=new Kick(fs);this.snare=new Snare(fs,this.noise);this.hat=new Hat(fs,this.noise);this.bass=new Bass(fs);
 this.bpm=82;this.sps=0;this.setTempo(82);this.phase=0;this.step=0;this.trans=0;this.fill=0;this.fh=false;this.fw=false;this.pend=false;this.rIdx=5;this.root=-1;this.rg=0.8;this.bg=0.85;}
 setTempo(b){this.bpm=Math.max(40,Math.min(240,b));this.sps=this.fs/((this.bpm/60)*4);}
 setRhythm(i){this.rIdx=Math.max(0,Math.min(5,i));} setChordRoot(r){this.root=r;}
 synchro(){if(this.trans===0)this.trans=1;}
 startStop(){if(this.trans===2){this.trans=0;}else{this.trans=2;this.step=0;this.phase=0;this.fire(0);}}
 noteOnSplit(){if(this.trans!==2){this.trans=2;this.step=0;this.phase=0;this.fire(0);}}
 allReleased(){if(this.trans===1)this.trans=0;}
 setFill(h){if(h&&!this.fw){this.fill=(this.fill===1)?2:1;this.pend=false;}else if(!h&&this.fw){this.pend=true;}this.fh=h;this.fw=h;}
 adv(){this.step=(this.step+1)%32;if(this.step%16===0){if(this.pend&&!this.fh){this.fill=0;this.pend=false;}}this.fire(this.step);}
 fire(st){const p=PAT[this.rIdx];let kh=p.k[st]!==0;if(this.fill===1)kh=(st%4===0);if(kh)this.kick.trig();
  let sh=p.s[st]!==0;if(this.fill===2)sh=(st%2===0);if(sh)this.snare.trig();
  if(p.ho[st]!==0)this.hat.open();else if(p.hc[st]!==0)this.hat.closed();
  const off=p.b[st];if(off!==REST){const root=(this.root>=0)?this.root:40;this.bass.trig(root+off);}}
 p(){if(this.trans===2){this.phase+=1;if(this.phase>=this.sps){this.phase-=this.sps;this.adv();}}
  const drums=this.kick.p()+this.snare.p()+this.hat.p();const b=this.bass.p();return drums*this.rg+b*this.bg;}}

// The complete ST-40 signal chain as a realm-agnostic object (no AudioWorklet
// or DOM references) so it can run inside a worklet OR on the main thread.
function makeST40(fs){
 const mel=new Alloc(fs), chord=new Alloc(fs), rhythm=new Rhythm(fs);
 let held=[], activeChord=[], master=0.85, vib=false, sus=false, lfoPh=0, lastTr=-1;
 mel.setPreset(PRESETS[0]); chord.setPreset(PRESETS[0]);
 function recompute(){const c=detectChord(held);
  if(!c){for(const t of activeChord)chord.off(t);activeChord=[];rhythm.setChordRoot(-1);return;}
  rhythm.setChordRoot(c.root);const nt=c.tones.map(t=>t+12);
  if(nt.length===activeChord.length&&nt.every((x,i)=>x===activeChord[i]))return;
  for(const t of activeChord)chord.off(t);for(const t of nt)chord.on(t,0.6);activeChord=nt;}
 function noteOn(n,vel){if(n<51){rhythm.noteOnSplit();if(held.indexOf(n)<0)held.push(n);recompute();}else mel.on(n,vel);}
 function noteOff(n){if(n<51){const i=held.indexOf(n);if(i>=0)held.splice(i,1);if(!held.length)rhythm.allReleased();recompute();}else mel.off(n);}
 function param(id,v){if(id==='master_vca_gain')master=v;else if(id==='rhythm_bus_gain')rhythm.rg=v;
  else if(id==='bass_osc_gain')rhythm.bg=v;else if(id==='host_bpm')rhythm.setTempo(v);
  else if(id==='global_lfo_on')vib=v>0.5;else if(id==='env_release_mult'){sus=v>0.5;const m=sus?3:1;mel.setMult(m);chord.setMult(m);}
  else if(id==='active_rhythm_idx')rhythm.setRhythm(v|0);
  else if(id==='active_patch_idx'){const p=PRESETS[Math.max(0,Math.min(21,v|0))];mel.setPreset(p);chord.setPreset(p);}}
 return {
  msg(m){const t=m.t;if(t==='on')noteOn(m.n,m.vel);else if(t==='off')noteOff(m.n);
   else if(t==='synchro')rhythm.synchro();else if(t==='startstop')rhythm.startStop();
   else if(t==='fill')rhythm.setFill(!!m.b);else if(t==='param')param(m.id,m.v);},
  block(L,Rr,n,onTransport){for(let i=0;i<n;i++){let lfo=0;if(vib)lfo=Math.sin(2*Math.PI*lfoPh);lfoPh+=5.5/fs;if(lfoPh>=1)lfoPh-=1;
    const depth=vib?0.2:0;const mv=mel.render(lfo,depth)+chord.render(lfo,depth);const rhy=rhythm.p();
    const s=Math.tanh((mv*0.45+rhy*0.6)*master);L[i]=s;if(Rr)Rr[i]=s;}
   const tr=rhythm.trans;if(tr!==lastTr){lastTr=tr;if(onTransport)onTransport(tr);}}
 };
}
'''

# --------------------------------------------------------------- worklet shell
WORKLET = CORE + r'''
class ST40Processor extends AudioWorkletProcessor{
 constructor(){super();this.core=makeST40(sampleRate);this.port.onmessage=(e)=>this.core.msg(e.data);}
 process(inp,out){const ch=out[0];this.core.block(ch[0],ch[1],ch[0].length,(tr)=>this.port.postMessage({t:'transport',st:tr}));return true;}}
registerProcessor('st40',ST40Processor);
'''

# ----------------------------------------------------------------- page bridge
# CORE is included here too so the ScriptProcessor fallback can run the DSP on
# the main thread. Prefer the worklet; fall back on any load failure.
BRIDGE = CORE + r'''
    // ---- Web Audio synth bridge (self-contained; faithful DSP port) ----
    let actx=null, node=null, spNode=null, fbCore=null; const pending=[];
    function postMsg(m){ if(node) node.port.postMessage(m); else if(fbCore) fbCore.msg(m); else pending.push(m); }
    function flushPending(){ const q=pending.slice(); pending.length=0; q.forEach(postMsg); }
    function hideHint(){ var b=document.getElementById("audioHint"); if(b) b.style.display="none"; }
    function startFallback(){
      // ScriptProcessorNode path — works on file:// where Blob worklets are blocked.
      fbCore=makeST40(actx.sampleRate);
      spNode=actx.createScriptProcessor(1024,0,2);
      spNode.onaudioprocess=(e)=>{ const ob=e.outputBuffer, L=ob.getChannelData(0), Rr=ob.numberOfChannels>1?ob.getChannelData(1):null;
        fbCore.block(L,Rr,L.length,(tr)=>{ if(window.st40SetTransport) window.st40SetTransport(tr); }); };
      spNode.connect(actx.destination);
    }
    async function startAudio(){
      if(actx){ if(actx.state==="suspended") actx.resume(); return; }
      actx=new (window.AudioContext||window.webkitAudioContext)();
      let ok=false;
      if(actx.audioWorklet){
        try{
          const src=document.getElementById("st40worklet").textContent;
          const url=URL.createObjectURL(new Blob([src],{type:"application/javascript"}));
          await actx.audioWorklet.addModule(url);
          node=new AudioWorkletNode(actx,"st40",{outputChannelCount:[2]});
          node.connect(actx.destination);
          node.port.onmessage=(e)=>{ if(e.data.t==="transport" && window.st40SetTransport) window.st40SetTransport(e.data.st); };
          ok=true;
        }catch(err){ node=null; }   // worklet unavailable (e.g. file://) -> fall back
      }
      if(!ok) startFallback();
      flushPending();
      if(actx.state==="suspended") actx.resume();
      hideHint();
    }
    window.addEventListener("pointerdown", startAudio, true);
    window.addEventListener("keydown", startAudio, true);

    const Juce=(function(){
      function L(){ return { _f:[], addListener(fn){this._f.push(fn);}, removeListener(){}, fire(){this._f.forEach(fn=>fn());} }; }
      function slider(scaled,norm,name){ return { properties:{start:0,end:1,skew:1,name:name}, valueChangedEvent:L(),propertiesChangedEvent:L(),_n:norm,_s:scaled,
        getNormalisedValue(){return this._n;}, getScaledValue(){return this._s;},
        setNormalisedValue(v){ this._n=v; this._s=(name==="host_bpm")?(40+v*200):v; postMsg({t:"param",id:name,v:this._s}); this.valueChangedEvent.fire(); },
        sliderDragStarted(){}, sliderDragEnded(){} }; }
      function toggle(v,name){ return { valueChangedEvent:L(),propertiesChangedEvent:L(),_v:v,
        getValue(){return this._v;}, setValue(x){ this._v=x; postMsg({t:"param",id:name,v:x?1:0}); this.valueChangedEvent.fire(); } }; }
      function combo(ch,ix,name){ return { properties:{choices:ch}, valueChangedEvent:L(),propertiesChangedEvent:L(),_i:ix,
        getChoiceIndex(){return this._i;}, setChoiceIndex(i){ this._i=i; postMsg({t:"param",id:name,v:i}); this.valueChangedEvent.fire(); } }; }
      const sl={ master_vca_gain:slider(0.85,0.85,"master_vca_gain"), host_bpm:slider(82,0.21,"host_bpm"),
        rhythm_bus_gain:slider(0.8,0.8,"rhythm_bus_gain"), bass_osc_gain:slider(0.85,0.85,"bass_osc_gain") };
      const tg={ global_lfo_on:toggle(false,"global_lfo_on"), env_release_mult:toggle(false,"env_release_mult") };
      const co={ active_rhythm_idx:combo(["Samba","Waltz","Swing","Slow Rock","Pops","Rock"],5,"active_rhythm_idx"),
        active_patch_idx:combo(["electric piano","banjo","guitar","harpsichord","xylophone","celesta","glockenspiel","organ","accordion","pipe organ","oriental pipe","brass","cello","synth fuzz","violin","trumpet","funny fuzz","st. ensemble","clarinet","flute","recorder","folk flute"],0,"active_patch_idx") };
      const fns={ noteOn:(n,v)=>postMsg({t:"on",n:n,vel:v}), noteOff:(n)=>postMsg({t:"off",n:n}),
        synchro:()=>postMsg({t:"synchro"}), setFill:(b)=>postMsg({t:"fill",b:b}), startStop:()=>postMsg({t:"startstop"}) };
      return { getSliderState:n=>sl[n], getToggleState:n=>tg[n], getComboBoxState:n=>co[n], getNativeFunction:n=>fns[n] };
    })();'''

html = UI.replace('import * as Juce from "./js/juce/index.js";', BRIDGE)

# a small "tap to enable sound" hint chip + the worklet script tag
hint = ('<div id="audioHint" style="position:fixed;left:50%;bottom:14px;transform:translateX(-50%);'
        'z-index:9999;font:600 13px system-ui,sans-serif;color:#f4eee1;background:rgba(20,17,13,.9);'
        'border:1px solid #4a4438;border-radius:999px;padding:8px 16px;box-shadow:0 6px 20px rgba(0,0,0,.5)">'
        '\U0001F50A  Toca el teclado para activar el sonido</div>')
worklet_tag = '<script id="st40worklet" type="text/worklet">' + WORKLET + '</script>'

body_inner = '<div class="st40" id="st40"></div>'
html = html.replace(body_inner, body_inner + "\n" + hint + "\n" + worklet_tag, 1)

doc = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
       '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
       '<title>Artifacts ST-40 — Emulator (sound)</title>\n</head>\n<body>\n'
       + html + '\n</body>\n</html>\n')
open(OUT, "w").write(doc)
print("wrote", OUT, os.path.getsize(OUT), "bytes")

