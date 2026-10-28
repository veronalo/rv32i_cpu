#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    std::vector<uint8_t> rom(16+32768+8192);
    FILE *fp=std::fopen(argv[1],"rb");if(!fp)return 2;
    if(std::fread(rom.data()+32784,1,8192,fp)!=8192)return 2;
    std::fclose(fp);
    SMBEngine engine(rom.data());engine.reset();
    std::set<unsigned> walk_poses;
    unsigned visible_frames=0,stop_x=0,standing_frames=0;
    bool jumped=false;
    for(unsigned frame=0;frame<390;++frame){
        unsigned keys=(frame>=40 && frame<45)?8:0;
        if(frame>=240 && frame<280)keys|=128;
        if(frame>=340 && frame<356)keys|=1;
        for(unsigned i=0;i<8;++i)
            engine.getController1().setButtonState(ControllerButton(i),(keys>>i)&1);
        engine.update();
        uint8_t state[2564];engine.copyVideoState(state);
        if(frame>=220){
            if(engine.debugByte(0x079e)){
                std::fprintf(stderr,"Test route unexpectedly damaged player at frame %u\n",frame);return 1;
            }
            unsigned offset=engine.debugByte(0x06e4),pixels=0;
            for(unsigned i=0;i<8;++i){
                unsigned off=2048+offset+i*4;
                if(off+3>=2304)return 1;
                if(state[off]>=239 || state[off+3]>=249)continue;
                unsigned tile=state[off+1]+((state[2560]&8)?256:0);
                for(unsigned row=0;row<8;++row){
                    unsigned bits=rom[32784+tile*16+row]|rom[32784+tile*16+row+8];
                    for(unsigned b=0;b<8;++b)pixels+=(bits>>b)&1;
                }
            }
            if(!(state[2561]&16) || pixels<48){
                std::fprintf(stderr,"Invisible player frame=%u pose=%02x pixels=%u\n",frame,engine.debugByte(0x06d5),pixels);return 1;
            }
            ++visible_frames;
            if(frame>=250 && frame<280)walk_poses.insert(engine.debugByte(0x06d5));
            // SMB has inertia: allow natural braking before checking standing.
            if(frame>=320 && frame<340 && engine.debugByte(0x57)==0){
                if(standing_frames && (engine.debugByte(0x86)!=stop_x || engine.debugByte(0x06d5)!=0xb8)){
                    std::fprintf(stderr,"Standing pose/position changed after braking\n");return 1;
                }
                stop_x=engine.debugByte(0x86);++standing_frames;
            }
            if(frame>=340 && engine.debugByte(0xce)<160)jumped=true;
        }
        if(frame==230 || frame==260 || frame==330 || frame==360){
            std::vector<uint32_t> image(256*240);engine.render(image.data());
            char path[100];std::snprintf(path,sizeof(path),"build/mario-art-%u.ppm",frame);
            fp=std::fopen(path,"wb");if(!fp)return 2;
            std::fprintf(fp,"P6\n256 240\n255\n");
            for(auto rgb:image){uint8_t b[3]={uint8_t(rgb>>16),uint8_t(rgb>>8),uint8_t(rgb)};std::fwrite(b,1,3,fp);}
            std::fclose(fp);
        }
    }
    if(walk_poses.size()<3 || !jumped || standing_frames<8){std::fprintf(stderr,"Missing walk poses, jump or standing (%u frames)\n",standing_frames);return 1;}
    std::printf("PASS: %u consecutive visible player frames, %zu walking poses, %u stable standing frames after release, jump\n",visible_frames,walk_poses.size(),standing_frames);
}
