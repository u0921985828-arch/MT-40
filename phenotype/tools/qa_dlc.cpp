//  qa_dlc.cpp — QA + level calibration for the 10k DLC.
//
//  Reads dlc/_qa_in.tsv (idx, genome-relpath, 31 params in kDefs order),
//  renders each preset through the real GranularEngine with its genome sample,
//  measures near-clip fraction / peak / rms / finiteness, and reduces
//  outputGain until the preset no longer rides the soft-clipper. Writes
//  dlc/_qa_out.tsv: idx  newGain  clip%  peak  rms  flags
//
//    g++ -O2 -std=c++20 -I Source -I Source/dsp tools/qa_dlc.cpp \
//        Source/dsp/GranularEngine.cpp -o qa_dlc
//    ./qa_dlc dlc/_qa_in.tsv dlc/_qa_out.tsv dlc
//
//  flags: ok | silent | nonfinite  (finalize drops non-ok presets).

#include "GranularEngine.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

using namespace phenotype;
using namespace phenotype::dsp;

static const char* IDS[31] = {
    "caudal","soilDensity","saturation","grainDensity","grainSize","position","spray",
    "pitchA","pitchB","crossBlend","modDepth","outputGain","arpOn","arpRate","arpMode",
    "arpSync","scaleType","filterCutoff","filterReso","filterType","filterMod","drive",
    "unison","unisonDetune","stereoWidth","delayMix","delayTime","delayFb","reverbMix",
    "reverbSize","reverbDamp" };

// Minimal PCM16 WAV -> mono float.
static std::vector<float> readWav(const std::string& path){
    std::ifstream f(path, std::ios::binary);
    std::vector<float> out;
    if(!f) return out;
    std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
    if(b.size()<44) return out;
    auto u16=[&](size_t o){ return (uint16_t)((uint8_t)b[o] | ((uint8_t)b[o+1]<<8)); };
    auto u32=[&](size_t o){ return (uint32_t)((uint8_t)b[o] | ((uint8_t)b[o+1]<<8) | ((uint8_t)b[o+2]<<16) | ((uint8_t)b[o+3]<<24)); };
    uint16_t ch=u16(22);
    size_t p=12;
    while(p+8<=b.size()){
        uint32_t id=u32(p), sz=u32(p+4);
        if(id==0x61746164){ // 'data'
            size_t n=std::min((size_t)sz, b.size()-(p+8))/2;
            const int16_t* s=reinterpret_cast<const int16_t*>(&b[p+8]);
            size_t frames=n/(ch?ch:1);
            out.resize(frames);
            for(size_t i=0;i<frames;++i){ float acc=0; for(int c=0;c<ch;++c) acc+=s[i*ch+c]/32768.0f; out[i]=acc/(ch?ch:1); }
            return out;
        }
        p+=8+sz+(sz&1);
    }
    return out;
}

int main(int argc,char**argv){
    if(argc<3){ std::fprintf(stderr,"usage: qa_dlc in.tsv out.tsv [basedir]\n"); return 1; }
    std::string inPath=argv[1], outPath=argv[2];
    std::string base = argc>3? argv[3] : "dlc";

    constexpr double SR=44100; constexpr int BLK=512;
    const int HELD=(int)(0.42*SR/BLK);       // ~0.42 s sustained
    const int notes[3]={57,60,64};
    std::vector<float> L(BLK),R(BLK);
    std::map<std::string,std::vector<float>> cache;

    GranularEngine eng; eng.prepare(SR,BLK); eng.setInstrumentMode(true);
    auto& hub=eng.params();

    auto measure=[&](float outGain,double& rmsOut)->std::pair<double,double>{
        hub.set("outputGain",outGain);
        eng.setArp(false,1.0f); eng.allNotesOff(); eng.reset();
        for(int n:notes) eng.noteOn(n,0.95f);
        long nc=0,N=0; double peak=0,sq=0;
        for(int b=0;b<HELD;++b){ eng.process(nullptr,nullptr,L.data(),R.data(),BLK);
            for(int i=0;i<BLK;++i){ double al=std::fabs(L[i]),ar=std::fabs(R[i]);
                double m=std::max(al,ar); peak=std::max(peak,m);
                sq+=(double)L[i]*L[i]+(double)R[i]*R[i];
                if(al>0.985||ar>0.985)++nc; N+=2; } }
        rmsOut=std::sqrt(sq/std::max(1L,N));
        return {(double)nc/std::max(1L,N/2), peak};
    };

    std::ifstream in(inPath);
    std::ofstream out(outPath);
    std::string line;
    long done=0, hot=0, sil=0, nf=0;
    while(std::getline(in,line)){
        if(line.empty()) continue;
        std::stringstream ss(line);
        std::string idxs, gpath; std::getline(ss,idxs,'\t'); std::getline(ss,gpath,'\t');
        float pv[31]; for(int k=0;k<31;++k){ std::string c; std::getline(ss,c,'\t'); pv[k]=c.empty()?0.f:std::stof(c); }

        // genome (cached)
        if(!cache.count(gpath)) cache[gpath]=readWav(base+"/"+gpath);
        auto& g=cache[gpath];
        if(!g.empty()) eng.loadGenomeFromSample(g.data(),(int)g.size());
        else eng.useBuiltinGenome();

        for(int k=0;k<31;++k) hub.set(IDS[k],pv[k]);
        float g0=pv[11];                        // provisional outputGain

        double rms=0; auto [c0,p0]=measure(g0,rms);
        float gg=g0; double c=c0; int it=0;
        while(c>0.03 && gg>0.12f && it<12){ gg*=0.9f; double r2; auto pr=measure(gg,r2); c=pr.first; rms=r2; ++it; }
        if(it) ++hot;

        // final finiteness + silence probe (already have rms/peak from last measure)
        double r_last; auto [cf,pf]=measure(gg,r_last);
        const char* flag="ok";
        if(!std::isfinite(pf)||!std::isfinite(r_last)){ flag="nonfinite"; ++nf; }
        else if(r_last<1e-4){ flag="silent"; ++sil; }

        out<<idxs<<"\t"<<gg<<"\t"<<(cf*100.0)<<"\t"<<pf<<"\t"<<r_last<<"\t"<<flag<<"\n";
        if(++done % 1000 == 0) std::fprintf(stderr,"  %ld/10000  (hot=%ld silent=%ld nonfinite=%ld)\n",done,hot,sil,nf);
    }
    std::fprintf(stderr,"done: %ld presets, recalibrated=%ld, silent=%ld, nonfinite=%ld\n",done,hot,sil,nf);
    return 0;
}
