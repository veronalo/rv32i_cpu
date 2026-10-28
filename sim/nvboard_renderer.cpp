#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Only the project's replacement nvboard.o renames SDL_CreateRenderer to this
// function. The external archive and SDL itself remain untouched.
extern "C" SDL_Renderer *SDLCALL rv32i_create_renderer(SDL_Window *window,
                                                      int index, Uint32 flags) {
    if (!window) {
        std::fprintf(stderr, "NVBoard: window creation failed (a working desktop is required): %s\n", SDL_GetError());
        SDL_Quit();
        std::exit(2);
    }
    const char *mode = std::getenv("NVBOARD_RENDERER");
    if (!mode || !*mode) mode = "auto";
    const bool automatic = std::strcmp(mode, "auto") == 0;
    const bool gpu = std::strcmp(mode, "gpu") == 0;
    const bool software = std::strcmp(mode, "software") == 0;
    if (!automatic && !gpu && !software) {
        std::fprintf(stderr, "NVBOARD_RENDERER must be auto, gpu, or software (got %s)\n", mode);
        std::exit(2);
    }
    // Preserve unrelated flags, but don't request VSync: presenting must not
    // block the thread that advances RTL and PS/2 pixel-clock cycles.
    const Uint32 common = flags & ~(SDL_RENDERER_SOFTWARE |
                                    SDL_RENDERER_ACCELERATED |
                                    SDL_RENDERER_PRESENTVSYNC);
    SDL_Renderer *renderer = nullptr;
    if (!software) {
        renderer = SDL_CreateRenderer(window, index, common | SDL_RENDERER_ACCELERATED);
        if (!renderer && automatic)
            std::fprintf(stderr, "NVBoard: accelerated renderer unavailable (%s); falling back to software\n", SDL_GetError());
    }
    if (!renderer && !gpu)
        renderer = SDL_CreateRenderer(window, index, common | SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::fprintf(stderr, "NVBoard: renderer creation failed, requested=%s: %s\n", mode, SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::exit(2);
    }
    // Some SDL backends report/use VSync even without a creation-time request.
    // Explicitly disable it rather than relying only on the requested flags.
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (SDL_RenderSetVSync(renderer, 0) != 0)
        std::fprintf(stderr, "NVBoard: backend could not explicitly disable VSync: %s\n", SDL_GetError());
#endif
    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer, &info) != 0 ||
        (gpu && !(info.flags & SDL_RENDERER_ACCELERATED))) {
        std::fprintf(stderr, "NVBoard: could not verify requested renderer: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::exit(2);
    }
    std::fprintf(stderr, "NVBoard renderer: requested=%s backend=%s accelerated=%s vsync=%s\n",
                 mode, info.name ? info.name : "unknown",
                 info.flags & SDL_RENDERER_ACCELERATED ? "yes" : "no",
                 info.flags & SDL_RENDERER_PRESENTVSYNC ? "yes" : "no");
    if (info.name && std::strncmp(info.name, "opengl", 6) == 0) {
        // Activate the renderer's GL context before querying it. This clears
        // only the unpublished back buffer; NVBoard fills it before presenting.
        SDL_RenderClear(renderer);
        SDL_RenderFlush(renderer);
        using GetString = const GLubyte *(APIENTRY *)(GLenum);
        auto get_string = reinterpret_cast<GetString>(SDL_GL_GetProcAddress("glGetString"));
        const GLubyte *device = get_string ? get_string(GL_RENDERER) : nullptr;
        if (device) {
            const char *name = reinterpret_cast<const char *>(device);
            std::fprintf(stderr, "NVBoard GL device: %s\n", name);
            if (std::strstr(name, "llvmpipe") || std::strstr(name, "softpipe"))
                std::fprintf(stderr, "NVBoard warning: OpenGL is using a software rasterizer, not a physical GPU\n");
        }
    }
    return renderer;
}
