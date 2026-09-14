//  render_probe.cpp — offline render of the built-in genome for QA listening.
//
//  Prepares the real GranularEngine (instrument mode), sets a parameter patch,
//  plays a note, renders N seconds of stereo and writes a 32-bit float WAV plus
//  a one-line analysis (peak, rms, dc, nonfinite, max |Δ| click metric).
//
//    g++ -O2 -std=c++20 -I Source/dsp tools/render_probe.cpp \
//        Source/dsp/GranularEngine.cpp -o render_probe
//    ./render_probe out.wav [blk] [note]
//
#include "GranularEngine.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <cstring>

using namespace phenotype;
using namespace phenotype::dsp;

static const char* IDS[31] = {
    "caudal","soilDensity","saturation","grainDensity","grainSize","position","spray",
    "pitchA","pitchB","crossBlend","modDepth","outputGain","arpOn","arpRate","arpMode",
    "arpSync","scaleType","filterCutoff","filterReso","filterType","filterMod","drive",
    "unison","unisonDetune","stereoWidth","delayMix","delayTime","delayFb","reverbMix",
    "reverbSize","reverbDamp" };
// kDefs defaults (mirror Parameters.h)
static const float DEF[31] = {
    0.5f,0.5f,0.9f,0.4f,0.3f,0.5f,0.2f,0.5f,0.5f,0.5f,0.5f,0.8f,0.0f,0.4f,0.0f,
    0.0f,0.0f,1.0f,0.12f,0.0f,0.0f,0.1f,0.0f,0.25f,0.5f,0.0f,0.35f,0.35f,0.0f,0.5f,0.4f };

static void writeWavF32(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr){
    std::ofstream f(path, std::ios::binary);
    uint32_t n=(uint32_t)L.size(); uint16_t ch=2, bits=32; uint32_t byteRate=sr*ch*(bits/8);
    uint32_t dataBytes=n*ch*(bits/8); uint32_t riff=36+dataBytes; uint16_t blockAlign=ch*(bits/8); uint16_t fmt=3;//float
    auto w32=[&](uint32_t v){ f.write((char*)&v,4); }; auto w16=[&](uint16_t v){ f.write((char*)&v,2); };
    f.write("RIFF",4); w32(riff); f.write("WAVE",4); f.write("fmt ",4); w32(16); w16(fmt); w16(ch); w32(sr); w32(byteRate); w16(blockAlign); w16(bits);
    f.write("data",4); w32(dataBytes);
    for(uint32_t i=0;i<n;++i){ f.write((char*)&L[i],4); f.write((char*)&R[i],4); }
}

int main(int argc,char**argv){
    const std::string out = argc>1? argv[1] : "probe.wav";
    const int BLK = argc>2? std::atoi(argv[2]) : 512;
    const int NOTE = argc>3? std::atoi(argv[3]) : 60;
    const double SR = 44100.0;
    const double SECONDS = 3.0;

    GranularEngine eng; eng.prepare(SR, BLK); eng.setInstrumentMode(true);
    eng.useBuiltinGenome();
    auto& hub = eng.params();
    for(int i=0;i<31;++i) hub.set(IDS[i], DEF[i]);
    const std::string patch = argc>4? argv[4] : "default";
    auto setp=[&](const char* id,float v){ for(int i=0;i<31;++i) if(std::strcmp(IDS[i],id)==0){ hub.set(id,v); } };
    if(patch=="emerald"){
        setp("grainDensity",0.68f); setp("grainSize",0.8f); setp("caudal",0.3f); setp("soilDensity",0.72f);
        setp("modDepth",0.7f); setp("crossBlend",0.45f); setp("unison",0.55f); setp("unisonDetune",0.35f);
        setp("filterCutoff",0.65f); setp("filterMod",0.4f); setp("drive",0.14f); setp("stereoWidth",0.85f);
    } else if(patch=="stress"){
        setp("grainDensity",0.95f); setp("grainSize",0.9f); setp("unison",1.0f); setp("drive",0.8f);
        setp("delayMix",0.6f); setp("reverbMix",0.6f); setp("filterReso",0.7f); setp("stereoWidth",1.0f);
    }
    eng.setArp(false, 8.0f); eng.setScale(0);
    eng.reset();

    const long total = (long)(SECONDS*SR);
    std::vector<float> L, R; L.reserve(total); R.reserve(total);
    std::vector<float> bL(BLK), bR(BLK);

    // note on at t=0, hold ~2.4s then release
    eng.noteOn(NOTE, 0.95f);
    const long relAt = (long)(2.4*SR);
    bool released=false;
    long done=0;
    double peak=0, sq=0, dc=0; long nf=0; float prevL=0; double maxd=0; long nsamp=0;
    while(done<total){
        if(!released && done>=relAt){ eng.noteOff(NOTE); released=true; }
        int nb = (int)std::min((long)BLK, total-done);
        std::fill(bL.begin(), bL.end(), 0.0f); std::fill(bR.begin(), bR.end(), 0.0f);
        eng.process(bL.data(), bR.data(), bL.data(), bR.data(), nb);
        for(int i=0;i<nb;++i){
            float l=bL[i], r=bR[i];
            if(!std::isfinite(l)||!std::isfinite(r)) { nf++; l=std::isfinite(l)?l:0; r=std::isfinite(r)?r:0; }
            L.push_back(l); R.push_back(r);
            double a=std::fabs(l); if(a>peak)peak=a; sq+=(double)l*l; dc+=l; nsamp++;
            double d=std::fabs(l-prevL); if(d>maxd)maxd=d; prevL=l;
        }
        done+=nb;
    }
    double rms=std::sqrt(sq/std::max(1L,nsamp));
    std::printf("blk=%d note=%d  peak=%.4f  rms=%.5f  dc=%.6f  nonfinite=%ld  maxDelta=%.4f  frames=%ld\n",
                BLK, NOTE, peak, rms, dc/std::max(1L,nsamp), nf, maxd, (long)L.size());
    writeWavF32(out, L, R, (int)SR);
    return 0;
}
