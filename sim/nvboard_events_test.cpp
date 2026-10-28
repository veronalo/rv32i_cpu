// Include the real adapter/dispatcher so this test also checks its UART-focus
// branch. Only widget hit testing and final UART/keyboard sinks are mocked.
#include "nvboard_events.cpp"
#include <cstdlib>
#include <utility>
#include <vector>

std::vector<Component *> components;
PinNode pin_array[NR_PINS]{};
static std::vector<std::pair<uint8_t, bool>> received_keys;
static std::vector<uint8_t> received_uart;

bool Component::in_rect(int, int) const { return false; }
int Component::get_component_type() const { return UART_TYPE; }
uint16_t Component::get_pin(int) const { return 0; }
void kb_push_key(uint8_t code, bool down) { received_keys.emplace_back(code, down); }
void uart_rx_getchar(uint8_t ch) { received_uart.push_back(ch); }
void uart_term_focus(bool) {}

static void require(bool value, const char *message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError()); std::exit(1); }
}

static void push_key(SDL_Scancode code, bool down, bool repeat = false, SDL_Keycode sym = SDLK_UNKNOWN) {
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = repeat;
    event.key.keysym.scancode = code;
    event.key.keysym.sym = sym;
    require(SDL_PushEvent(&event) == 1, "push keyboard event");
}

int main() {
    require(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL events init");
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_StartTextInput(); // Desktop SDL starts in this state by default.
    require(SDL_IsTextInputActive() == SDL_TRUE, "text input initially enabled");
    push_key(SDL_SCANCODE_Q, true);
    for (int i = 0; i < 10000; ++i) push_key(SDL_SCANCODE_Q, true, true);
    push_key(SDL_SCANCODE_Q, false);
    read_event();
    require(ignored_key_repeats == 10000, "all 10000 typematic events filtered");
    require(SDL_IsTextInputActive() == SDL_FALSE, "game polling disables IME/text input before pumping events");
    require(received_keys == std::vector<std::pair<uint8_t, bool>>{
        {SDL_SCANCODE_Q, true}, {SDL_SCANCODE_Q, false}}, "Q press/release remain ordered");

    received_keys.clear();
    for (SDL_Scancode code : {SDL_SCANCODE_W, SDL_SCANCODE_SPACE, SDL_SCANCODE_D}) {
        push_key(code, true);
        push_key(code, true, true);
    }
    for (SDL_Scancode code : {SDL_SCANCODE_W, SDL_SCANCODE_SPACE, SDL_SCANCODE_D}) push_key(code, false);
    read_event();
    require(received_keys == std::vector<std::pair<uint8_t, bool>>{
        {SDL_SCANCODE_W, true}, {SDL_SCANCODE_SPACE, true}, {SDL_SCANCODE_D, true},
        {SDL_SCANCODE_W, false}, {SDL_SCANCODE_SPACE, false}, {SDL_SCANCODE_D, false}},
        "simultaneous and alias key transitions remain unchanged");

    // An unflagged repeated press is not silently deduplicated by key state.
    received_keys.clear();
    push_key(SDL_SCANCODE_D, true); push_key(SDL_SCANCODE_D, true); push_key(SDL_SCANCODE_D, false);
    read_event();
    require(received_keys.size() == 3, "non-repeat events are preserved");

    received_keys.clear();
    uart_term_get_focus = true; // Emulate selecting the UART widget.
    auto before = ignored_key_repeats;
    push_key(SDL_SCANCODE_BACKSPACE, true, true, SDLK_BACKSPACE);
    push_key(SDL_SCANCODE_RETURN, true, true, SDLK_RETURN);
    push_key(SDL_SCANCODE_TAB, true, true, SDLK_TAB);
    push_key(SDL_SCANCODE_Q, false);
    SDL_Event text{}; text.type = SDL_TEXTINPUT; text.text.text[0] = 'q';
    require(SDL_PushEvent(&text) == 1, "push UART text");
    read_event();
    require(ignored_key_repeats == before, "UART repeat events are not filtered");
    require(SDL_IsTextInputActive() == SDL_TRUE, "UART focus enables text input");
    require(received_keys.empty(), "UART focus does not send PS/2 keys");
    require(received_uart == std::vector<uint8_t>{'\b', '\n', '\t', 'q'}, "UART repeat shortcuts/text preserved");

    SDL_Event click{}; click.type = SDL_MOUSEBUTTONDOWN; click.button.x = -1; click.button.y = -1;
    require(SDL_PushEvent(&click) == 1, "push focus change");
    push_key(SDL_SCANCODE_Q, true, true); push_key(SDL_SCANCODE_Q, false);
    read_event();
    require(!uart_term_get_focus && ignored_key_repeats == before + 1, "filter follows focus change in same event batch");
    require(SDL_IsTextInputActive() == SDL_FALSE, "leaving UART disables text input");
    require(received_keys == std::vector<std::pair<uint8_t, bool>>{{SDL_SCANCODE_Q, false}}, "release survives focus change");
    require(components.empty(),"VGA is not in upstream widget registry");
    rv32i_mouse_enable(true);
    auto push_motion=[](int x,int y){
        SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.x=x;e.motion.y=y;
        require(SDL_PushEvent(&e)==1,"push mouse motion");
    };
    uint32_t motion_packet;
    for(int i=0;i<10000;++i)push_motion(i%640,480+i%480);
    push_motion(639,959);read_event();
    require(rv32i_mouse_position()==((1u<<19)|(479u<<10)|639u),"continuous physical 640x480 position");
    require(!rv32i_mouse_pop(motion_packet),"10000 motion events never enter click FIFO");
    require(system_cursor_hidden,"system cursor hidden only in VGA");
    push_motion(640,700);read_event();
    require(rv32i_mouse_position()==0&&!system_cursor_hidden,"outside restores system cursor");
    auto push_click=[](int x,int y,uint8_t button){
        SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.x=x;e.button.y=y;e.button.button=button;
        require(SDL_PushEvent(&e)==1,"push mouse click");
        e.type=SDL_MOUSEBUTTONUP;require(SDL_PushEvent(&e)==1,"push mouse release");
    };
    push_click(100,200,SDL_BUTTON_LEFT); // board controls: not the VGA viewport
    push_click(639,959,SDL_BUTTON_LEFT);
    push_click(640,600,SDL_BUTTON_LEFT); // right boundary excluded
    push_click(120,760,SDL_BUTTON_RIGHT);
    read_event();uint32_t packet;
    require(rv32i_mouse_pop(packet)&&packet==pvz_click(319,239),"VGA scaling and bottom/right edge");
    require(rv32i_mouse_pop(packet)&&packet==pvz_click(60,140,true),"ordered right click");
    require(!rv32i_mouse_pop(packet),"widget/outside clicks and releases excluded");
    require(rv32i_mouse_inject(pvz_click(52,20)),"inject SDL mouse event");read_event();
    require(rv32i_mouse_pop(packet)&&packet==pvz_click(52,20),"SDL roundtrip");
    for(unsigned i=0;i<64;++i)push_click(int(i*2),720,SDL_BUTTON_LEFT);
    read_event();
    for(unsigned i=0;i<64;++i)
        require(rv32i_mouse_pop(packet)&&packet==pvz_click(i,120),"rapid clicks retain FIFO order");
    require(!rv32i_mouse_pop(packet),"rapid click queue drained exactly once");
    push_click(120,760,SDL_BUTTON_LEFT);read_event();
    SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    require(SDL_PushEvent(&focus)==1,"focus loss");read_event();
    require(!rv32i_mouse_pop(packet),"focus loss drops stale clicks");
    require(rv32i_mouse_position()==0&&!system_cursor_hidden,"focus loss hides RTL cursor and restores system cursor");
    push_motion(200,700);read_event();
    SDL_Event leave{};leave.type=SDL_WINDOWEVENT;leave.window.event=SDL_WINDOWEVENT_LEAVE;
    require(SDL_PushEvent(&leave)==1,"mouse leaves window");read_event();
    require(rv32i_mouse_position()==0&&!system_cursor_hidden,"window leave restores cursor");
    rv32i_mouse_enable(false);push_click(120,760,SDL_BUTTON_LEFT);read_event();
    require(!rv32i_mouse_pop(packet),"Mario default/disabled mouse receives nothing");
    require(rv32i_mouse_position()==0&&!system_cursor_hidden,"disable clears pointer and restores system cursor");
    components.clear();SDL_Quit();
    std::puts("PASS: mouse viewport mapping/queue/buttons/focus/opt-in; keyboard and UART regressions");
    std::puts("PASS: 10000 key repeats filtered; presses/releases, aliases, UART repeats/text and focus changes preserved");
}
