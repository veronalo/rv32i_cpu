#include "SMB/SMBEngine.hpp"
#include "SMB/SMBConstants.hpp"
#include "Emulation/Controller.hpp"
#include "game_input.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

class SMBInjuryBlinkTest {
public:
    static void run(SMBEngine& engine){
        auto require=[](bool ok,const char* message){
            if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
        };
        // Boot and enter the level through the ordinary controller/update path.
        bool ready=false;
        for(unsigned f=0;f<300;++f){
            // Stay still after Start; wait out the WORLD 1-1 intro as well.
            unsigned keys=(f>=20&&f<23)?8:0;
            for(unsigned b=0;b<8;++b)engine.getController1().setButtonState(ControllerButton(b),(keys>>b)&1);
            for(unsigned t=0;t<MARIO_UPDATES_PER_FRAME;++t)engine.update();
            if(engine.debugByte(OperMode)==1&&engine.debugByte(OperMode_Task)==3&&
               engine.debugByte(GameEngineSubroutine)==8){ready=true;break;}
        }
        require(ready,"fixture entered normal player-control gameplay");
        for(unsigned b=0;b<8;++b)engine.getController1().setButtonState(ControllerButton(b),false);
        bool phase_seen[4]={},sample_visible[2]={},last_presented=false;
        unsigned presentations=0;
        for(unsigned i=0;i<16;++i){
            engine.writeData(InjuryTimer,0x40);
            unsigned old_offset=engine.debugByte(Player_SprDataOffset);
            engine.update();
            unsigned phase=engine.debugByte(FrameCounter)&3;
            unsigned offset=engine.debugByte(Player_SprDataOffset);
            bool visible=false;
            for(unsigned row=0;row<6;++row)
                visible|=engine.debugByte(Sprite_Y_Position+offset+4*row)<240;
            std::printf("logic phase=%u player=%s\n",phase,visible?"visible":"hidden");
            require(visible==(phase<2),"injured player must show for two ticks, hide for two ticks");
            phase_seen[phase]=true;
            if((i&1)==1){
                // Check the actual PPU/OAM payload submitted every two ticks,
                // including its one-tick DMA delay, not only RAM sprite writes.
                uint8_t video[2564];engine.copyVideoState(video);bool presented=false;
                for(unsigned row=0;row<6;++row)presented|=video[2048+old_offset+4*row]<240;
                if(presentations++)require(presented!=last_presented,"submitted 30 Hz video must alternate every frame");
                last_presented=presented;sample_visible[presented]=true;
            }
        }
        for(bool seen:phase_seen)require(seen,"covered all four logic phases");
        require(sample_visible[0]&&sample_visible[1],"30 Hz decimation retains both visible and hidden phases");
        for(unsigned i=0;i<8;++i){
            engine.writeData(InjuryTimer,0);engine.update();
            unsigned offset=engine.debugByte(Player_SprDataOffset);bool visible=false;
            for(unsigned row=0;row<6;++row)visible|=engine.debugByte(Sprite_Y_Position+offset+4*row)<240;
            require(visible,"uninjured player does not blink");
        }
        std::puts("PASS: injury blink 2-on/2-off at 60 Hz, both phases at 30 Hz; normal player stays visible");
    }
};
int main(){
    std::vector<uint8_t> rom(16+32768+8192);
    SMBEngine engine(rom.data());engine.reset();SMBInjuryBlinkTest::run(engine);
}
