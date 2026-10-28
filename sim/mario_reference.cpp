#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "game_input.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc,char **argv) {
    if(argc!=4 && argc!=5){std::fprintf(stderr,"usage: reference chr.bin frames log [video-fixture]\n");return 2;}
    std::vector<uint8_t> rom(16+32768+8192);
    FILE *fp=std::fopen(argv[1],"rb");if(!fp)return 2;
    if(std::fread(rom.data()+16+32768,1,8192,fp)!=8192)return 2;
    std::fclose(fp);
    SMBEngine engine(rom.data());engine.reset();
    fp=std::fopen(argv[3],"w");if(!fp)return 2;
    for(unsigned f=0;f<unsigned(std::atoi(argv[2]));++f){
        auto keys=demo_keys(f);
        for(unsigned i=0;i<8;++i)engine.getController1().setButtonState(ControllerButton(i),(keys>>i)&1);
        for(unsigned tick=0;tick<MARIO_UPDATES_PER_FRAME;++tick)engine.update();
        uint32_t state=engine.debugByte(0x770)|(engine.debugByte(0x86)<<8)|
            (engine.debugByte(0xce)<<16)|(engine.debugByte(0x6d)<<24);
        uint32_t level=engine.debugByte(0x71c)|(engine.debugByte(0x75f)<<8)|(engine.debugByte(0x75c)<<16);
        std::fprintf(fp,"%u %08x %08x\n",f+1,state,level);
    }
    std::fclose(fp);
    if(argc==5){
        uint8_t state[2564];engine.copyVideoState(state);
        std::vector<uint32_t> image(256*240);engine.render(image.data());
        fp=std::fopen(argv[4],"wb");if(!fp)return 2;
        std::fwrite(rom.data()+16+32768,1,8192,fp);
        std::fwrite(state,1,sizeof(state),fp);
        for(auto rgb:image){
            uint8_t b[3]={uint8_t(rgb>>16),uint8_t(rgb>>8),uint8_t(rgb)};
            std::fwrite(b,1,3,fp);
        }
        std::fclose(fp);
    }
}
