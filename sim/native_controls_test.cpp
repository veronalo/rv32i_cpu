#include "HostControls.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
int main() {
    std::array<Uint8,SDL_NUM_SCANCODES> keys{};
    auto expect=[&](uint16_t wanted){
        auto actual=smbHostButtons(keys.data());
        if(actual!=wanted){std::fprintf(stderr,"native keys got=%03x expected=%03x\n",actual,wanted);std::exit(1);}
    };
    const SDL_Scancode codes[]={SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,
        SDL_SCANCODE_D,SDL_SCANCODE_SPACE,SDL_SCANCODE_RETURN,SDL_SCANCODE_KP_ENTER};
    const uint16_t values[]={17,64,32,128,1,8,8};
    for(unsigned i=0;i<7;++i){keys.fill(0);keys[codes[i]]=1;expect(values[i]);}
    keys.fill(0);keys[SDL_SCANCODE_W]=keys[SDL_SCANCODE_SPACE]=keys[SDL_SCANCODE_D]=1;
    expect(145);keys[SDL_SCANCODE_W]=0;expect(129);
    keys[SDL_SCANCODE_D]=0;expect(1);keys[SDL_SCANCODE_SPACE]=0;expect(0);
    keys[SDL_SCANCODE_A]=keys[SDL_SCANCODE_LEFT]=1;expect(64);
    keys[SDL_SCANCODE_A]=0;expect(64);keys[SDL_SCANCODE_LEFT]=0;expect(0);
    std::puts("PASS: native WASD, W/space jump, Enter, simultaneous keys and independent alias release");
}
