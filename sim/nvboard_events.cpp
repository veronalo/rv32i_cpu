#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <deque>
#include <configs.h>
#include <vga.h>
#include "nvboard_mouse.h"
#include "../sw/PvZ-simplized/mini/pointer.h"

// Build the installed NVBoard event dispatcher in this project's archive,
// without copying or modifying its source. Only SDL polling is adapted here.
// NVBOARD_EVENT_SOURCE is the absolute path to NVBoard's src/event.cpp.
extern "C" int SDLCALL rv32i_poll_event(SDL_Event *event);
#define SDL_PollEvent rv32i_poll_event
#include NVBOARD_EVENT_SOURCE
#undef SDL_PollEvent

static bool mouse_enabled=false;
static std::deque<uint32_t> mouse_clicks;
static uint32_t pointer_position=0;
static bool system_cursor_hidden=false;
static int saved_cursor_state=SDL_ENABLE;
static void show_system_cursor(bool hide){
    if(hide==system_cursor_hidden)return;
    SDL_ShowCursor(hide?SDL_DISABLE:saved_cursor_state);
    system_cursor_hidden=hide;
}
static void update_pointer(int x,int y);
uint32_t rv32i_mouse_position(){return pointer_position;}
void rv32i_mouse_enable(bool enabled){
    show_system_cursor(false);
    mouse_enabled=enabled;mouse_clicks.clear();pointer_position=0;
    if(enabled){
        saved_cursor_state=SDL_ShowCursor(SDL_QUERY);
        if(saved_cursor_state<0)saved_cursor_state=SDL_ENABLE;
        if(SDL_GetMouseFocus()){int x,y;SDL_GetMouseState(&x,&y);update_pointer(x,y);}
    }
}
bool rv32i_mouse_pop(uint32_t& packet){
    if(mouse_clicks.empty())return false;
    packet=mouse_clicks.front();mouse_clicks.pop_front();return true;
}
static SDL_Rect mouse_viewport(){
    // NVBoard's init_vga does NOT add VGA to `components`. Match the layout
    // in upstream VGA::VGA using its own configuration headers, not widget
    // enumeration or coordinates of the whole 1280x960 board window.
    return SDL_Rect{0,WINDOW_HEIGHT/2,VGA_DEFAULT_WIDTH,VGA_DEFAULT_HEIGHT};
}
static void update_pointer(int x,int y){
    const auto r=mouse_viewport();
    bool inside=x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
    pointer_position=inside?(1u<<19)|(unsigned(y-r.y)*480/unsigned(r.h)<<10)|
                                    (unsigned(x-r.x)*640/unsigned(r.w)):0;
    show_system_cursor(inside);
}
static void capture_mouse(const SDL_Event& event){
    if(!mouse_enabled)return;
    if(event.type==SDL_WINDOWEVENT){
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST||event.window.event==SDL_WINDOWEVENT_LEAVE){
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST)mouse_clicks.clear();
            pointer_position=0;show_system_cursor(false);
        }else if(event.window.event==SDL_WINDOWEVENT_ENTER||event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){
            if(SDL_GetMouseFocus()){int x,y;SDL_GetMouseState(&x,&y);update_pointer(x,y);}
        }
        return;
    }
    if(event.type==SDL_MOUSEMOTION){update_pointer(event.motion.x,event.motion.y);return;}
    if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)
        update_pointer(event.button.x,event.button.y);
    if(event.type!=SDL_MOUSEBUTTONDOWN||
       (event.button.button!=SDL_BUTTON_LEFT&&event.button.button!=SDL_BUTTON_RIGHT))return;
    const SDL_Rect viewport=mouse_viewport();const SDL_Rect* r=&viewport;
    // Upstream widgets and their hit testing use SDL window coordinates too.
    unsigned x,y;
    if(!pvz_map_pointer(event.button.x,event.button.y,r->x,r->y,r->w,r->h,x,y))return;
    if(mouse_clicks.size()==64){
        static bool warned=false;
        if(!warned){std::fputs("NVBoard mouse: click queue full; dropping excess clicks\n",stderr);warned=true;}
        return;
    }
    mouse_clicks.push_back(pvz_click(x,y,event.button.button==SDL_BUTTON_RIGHT));
}
bool rv32i_mouse_inject(uint32_t packet){
    if(!mouse_enabled)return false;
    const SDL_Rect viewport=mouse_viewport();const SDL_Rect* r=&viewport;
    if((packet&511)>=320||((packet>>9)&255)>=240)return false;
    SDL_Event event{};event.type=SDL_MOUSEBUTTONDOWN;event.button.state=SDL_PRESSED;
    event.button.button=(packet&(1u<<18))?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT;
    event.button.x=r->x+int((packet&511)*unsigned(r->w)/320)+1;
    event.button.y=r->y+int(((packet>>9)&255)*unsigned(r->h)/240)+1;
    if(SDL_PushEvent(&event)!=1)return false;
    event.type=SDL_MOUSEBUTTONUP;event.button.state=SDL_RELEASED;
    return SDL_PushEvent(&event)==1;
}

static std::uint64_t ignored_key_repeats = 0;
// Diagnostic-only A/B switch. The SDL desktop default enables text input even
// before NVBoard's UART widget is selected; normal game input needs scancodes,
// not IME composition. Keep the old behavior available for controlled tests.
static const bool legacy_text_input = [] {
    const char *mode = std::getenv("NVBOARD_TEXT_INPUT");
    if (!mode || !*mode || std::strcmp(mode, "focused") == 0) return false;
    if (std::strcmp(mode, "legacy") == 0) return true;
    std::fputs("NVBOARD_TEXT_INPUT must be focused or legacy\n", stderr);
    std::exit(2);
}();
static FILE *input_trace = nullptr;
static void close_input_trace() { if (input_trace) std::fclose(input_trace); }
static bool input_trace_ready = [] {
    const char *path = std::getenv("NVBOARD_INPUT_TRACE");
    if (path && *path) {
        input_trace = std::fopen(path, "w");
        if (!input_trace) { std::perror("NVBOARD_INPUT_TRACE"); std::exit(2); }
        std::fprintf(input_trace, "monotonic_ms,poll_ms,event_type,scancode,repeat,text_active,uart_focus,mouse_x,mouse_y,mouse_button\n");
        std::atexit(close_input_trace);
    }
    return true;
}();

static void sync_text_input_focus() {
    if (legacy_text_input) return;
    bool active = SDL_IsTextInputActive() == SDL_TRUE;
    if (active && !uart_term_get_focus) SDL_StopTextInput();
    else if (!active && uart_term_get_focus) SDL_StartTextInput();
}

extern "C" int SDLCALL rv32i_poll_event(SDL_Event *event) {
    while (true) {
        // Do this BEFORE polling: filtering an already-processed KEYDOWN is
        // too late to avoid work in the platform event pump/input method.
        sync_text_input_focus();
        const auto start = input_trace ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        int available = SDL_PollEvent(event);
        if (input_trace) {
            auto now = std::chrono::steady_clock::now();
            bool key = available && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP);
            bool mouse = available && (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP);
            std::fprintf(input_trace, "%.3f,%.3f,%u,%u,%u,%u,%u,%d,%d,%u\n",
                std::chrono::duration<double,std::milli>(now.time_since_epoch()).count(),
                std::chrono::duration<double,std::milli>(now-start).count(),
                available ? event->type : 0u, key ? unsigned(event->key.keysym.scancode) : 0u,
                key ? event->key.repeat : 0u, SDL_IsTextInputActive() == SDL_TRUE, uart_term_get_focus,
                mouse ? event->button.x : -1, mouse ? event->button.y : -1, mouse ? event->button.button : 0u);
        }
        if (!available) return 0;
        capture_mouse(*event);
        // uart_term_get_focus belongs to the upstream dispatcher included
        // above. Checking it here preserves UART repeats, text, and shortcuts.
        if (!uart_term_get_focus && event->type == SDL_KEYDOWN && event->key.repeat) {
            if (++ignored_key_repeats == 1)
                std::fputs("NVBoard input: filtering host key repeats on PS/2; initial presses, releases and UART input are preserved\n", stderr);
            continue;
        }
        return 1;
    }
}
