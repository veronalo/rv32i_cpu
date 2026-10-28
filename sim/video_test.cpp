#include "Vmario_video.h"
#include <verilated.h>
#include <cstdio>
#include <vector>
int main(int argc,char **argv){
    if(argc!=2)return 2;
    FILE *fp=std::fopen(argv[1],"rb");if(!fp)return 2;
    std::vector<unsigned char> fixture(8192+2564+256*240*3);
    if(std::fread(fixture.data(),1,fixture.size(),fp)!=fixture.size())return 2;
    std::fclose(fp);
    Vmario_video t;
    auto tick=[&](){t.clk=0;t.eval();t.clk=1;t.eval();};
    t.reset=1;tick();t.reset=0;t.selected=1;
    for(unsigned i=0;i<8192+2564;i+=4){
        t.address=i;t.wstrb=15;t.wdata=fixture[i]|(fixture[i+1]<<8)|(fixture[i+2]<<16)|(fixture[i+3]<<24);tick();
    }
    t.address=0x2a04;t.wdata=1;tick();t.wstrb=0;t.selected=0;
    while(t.frame_count==0)tick();
    unsigned pixel=0,errors=0,hs=0,vs=0,cycles=0;
    while(t.frame_count==1){
        tick();++cycles;if(!t.hsync)++hs;if(!t.vsync)++vs;
        if(!t.valid)continue;
        unsigned x=pixel%640,y=pixel/640; ++pixel;
        unsigned actual=(t.red<<16)|(t.green<<8)|t.blue,expected=0;
        if(x>=64 && x<576){
            auto off=8192+2564+((y/2)*256+(x-64)/2)*3;
            expected=(fixture[off]<<16)|(fixture[off+1]<<8)|fixture[off+2];
        }
        if(actual!=expected){if(errors<5)std::printf("pixel %u,%u rtl=%06x ref=%06x\n",x,y,actual,expected);++errors;}
    }
    if(pixel!=640*480 || cycles!=800*525 || hs!=96*525 || vs!=2*800){
        std::printf("FAIL timing pixels=%u cycles=%u hs=%u vs=%u\n",pixel,cycles,hs,vs);return 1;
    }
    std::printf("%s: RTL VGA vs native PPU, 307200 pixels, %u mismatches\n",errors?"FAIL":"PASS",errors);
    return errors?1:0;
}
