/* sdl_funcs_renderer.inl — SDL2 2D software/accelerated renderer
** Included by builtin_sdl2.cpp
**
** This is the SDL2 built-in 2D renderer — NOT OpenGL raw drawing.
** Use it when you don't need shaders and want simple 2D (rectangles,
** lines, textures, colours). For shader-based 3D use SDL_GL_* + import gl.
**
** Typical usage:
**   var ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED)
**   SDL_SetRenderDrawColor(ren, 30, 30, 30, 255)
**   SDL_RenderClear(ren)
**   SDL_SetRenderDrawColor(ren, 255, 100, 0, 255)
**   SDL_RenderFillRect(ren, 10, 10, 200, 100)
**   SDL_RenderPresent(ren)
**   SDL_DestroyRenderer(ren)
*/

/* SDL_CreateRenderer(window, index, flags) → renderer_ptr or nil
**   index: -1 = first available driver
**   flags: SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_SOFTWARE */
static int nat_SDL_CreateRenderer(VM *vm, Value *args, int nargs)
{
    void *win = nullptr;
    if (nargs < 3 || !expect_ptr_arg(vm, args[0], &win, "SDL_CreateRenderer", "window")
            || !is_int(args[1]) || !is_int(args[2]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_CreateRenderer expects (window, index, flags).");
        return 0;
    }
    SDL_Renderer *r = SDL_CreateRenderer(
        (SDL_Window *)win, (int)args[1].as.integer, (uint32_t)args[2].as.integer);
    args[0] = r ? val_ptr(r) : val_nil();
    return 1;
}

static int nat_SDL_DestroyRenderer(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_DestroyRenderer", "renderer")) return 0;
    SDL_DestroyRenderer((SDL_Renderer *)p); args[0] = val_nil(); return 1;
}

/* SDL_SetRenderDrawColor(renderer, r, g, b, a) */
static int nat_SDL_SetRenderDrawColor(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetRenderDrawColor", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetRenderDrawColor expects (ren, r, g, b, a).");
        return 0;
    }
    SDL_SetRenderDrawColor((SDL_Renderer *)p,
        (uint8_t)args[1].as.integer, (uint8_t)args[2].as.integer,
        (uint8_t)args[3].as.integer, (uint8_t)args[4].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_SetRenderDrawBlendMode(renderer, blend_mode) */
static int nat_SDL_SetRenderDrawBlendMode(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 2 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetRenderDrawBlendMode", "renderer")
            || !is_int(args[1]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetRenderDrawBlendMode expects (ren, mode).");
        return 0;
    }
    SDL_SetRenderDrawBlendMode((SDL_Renderer *)p, (SDL_BlendMode)args[1].as.integer);
    args[0] = val_nil(); return 1;
}

static int nat_SDL_RenderClear(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderClear", "renderer")) return 0;
    SDL_RenderClear((SDL_Renderer *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_RenderPresent(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderPresent", "renderer")) return 0;
    SDL_RenderPresent((SDL_Renderer *)p); args[0] = val_nil(); return 1;
}

/* SDL_RenderDrawPoint(renderer, x, y) */
static int nat_SDL_RenderDrawPoint(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 3 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderDrawPoint", "renderer")
            || !is_int(args[1]) || !is_int(args[2]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_RenderDrawPoint expects (ren, x, y).");
        return 0;
    }
    SDL_RenderDrawPoint((SDL_Renderer *)p, (int)args[1].as.integer, (int)args[2].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_RenderDrawLine(renderer, x1, y1, x2, y2) */
static int nat_SDL_RenderDrawLine(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderDrawLine", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_RenderDrawLine expects (ren, x1, y1, x2, y2).");
        return 0;
    }
    SDL_RenderDrawLine((SDL_Renderer *)p,
        (int)args[1].as.integer, (int)args[2].as.integer,
        (int)args[3].as.integer, (int)args[4].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_RenderDrawRect(renderer, x, y, w, h) */
static int nat_SDL_RenderDrawRect(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderDrawRect", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_RenderDrawRect expects (ren, x, y, w, h).");
        return 0;
    }
    SDL_Rect r = {(int)args[1].as.integer, (int)args[2].as.integer,
                  (int)args[3].as.integer, (int)args[4].as.integer};
    SDL_RenderDrawRect((SDL_Renderer *)p, &r);
    args[0] = val_nil(); return 1;
}

/* SDL_RenderFillRect(renderer, x, y, w, h) */
static int nat_SDL_RenderFillRect(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderFillRect", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_RenderFillRect expects (ren, x, y, w, h).");
        return 0;
    }
    SDL_Rect r = {(int)args[1].as.integer, (int)args[2].as.integer,
                  (int)args[3].as.integer, (int)args[4].as.integer};
    SDL_RenderFillRect((SDL_Renderer *)p, &r);
    args[0] = val_nil(); return 1;
}

/* SDL_RenderSetViewport(renderer, x, y, w, h) — pass w=0,h=0 to reset */
static int nat_SDL_RenderSetViewport(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderSetViewport", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_RenderSetViewport expects (ren, x, y, w, h).");
        return 0;
    }
    int w = (int)args[3].as.integer, h = (int)args[4].as.integer;
    if (w == 0 && h == 0) {
        SDL_RenderSetViewport((SDL_Renderer *)p, nullptr);
    } else {
        SDL_Rect r = {(int)args[1].as.integer, (int)args[2].as.integer, w, h};
        SDL_RenderSetViewport((SDL_Renderer *)p, &r);
    }
    args[0] = val_nil(); return 1;
}

/* SDL_RenderSetScale(renderer, sx, sy) */
static int nat_SDL_RenderSetScale(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 3 || !expect_ptr_arg(vm, args[0], &p, "SDL_RenderSetScale", "renderer"))
        return 0;
    float sx = is_int(args[1]) ? (float)args[1].as.integer : (float)args[1].as.number;
    float sy = is_int(args[2]) ? (float)args[2].as.integer : (float)args[2].as.number;
    SDL_RenderSetScale((SDL_Renderer *)p, sx, sy);
    args[0] = val_nil(); return 1;
}

/* SDL_GetRendererOutputSize(renderer) → w, h */
static int nat_SDL_GetRendererOutputSize(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GetRendererOutputSize", "renderer")) return 0;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize((SDL_Renderer *)p, &w, &h);
    args[0] = val_int(w); args[1] = val_int(h); return 2;
}

/* SDL_CreateTexture(renderer, format, access, w, h) → texture_ptr or nil
**   format: SDL_PIXELFORMAT_RGBA8888, etc.
**   access: SDL_TEXTUREACCESS_STATIC / STREAMING / TARGET */
static int nat_SDL_CreateTexture(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 5 || !expect_ptr_arg(vm, args[0], &p, "SDL_CreateTexture", "renderer")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_CreateTexture expects (ren, format, access, w, h).");
        return 0;
    }
    SDL_Texture *t = SDL_CreateTexture((SDL_Renderer *)p,
        (uint32_t)args[1].as.integer, (int)args[2].as.integer,
        (int)args[3].as.integer,      (int)args[4].as.integer);
    args[0] = t ? val_ptr(t) : val_nil(); return 1;
}

/* SDL_CreateTextureFromSurface(renderer, surface) → texture_ptr or nil */
static int nat_SDL_CreateTextureFromSurface(VM *vm, Value *args, int nargs)
{
    void *ren = nullptr, *surf = nullptr;
    if (nargs < 2
            || !expect_ptr_arg(vm, args[0], &ren,  "SDL_CreateTextureFromSurface", "renderer")
            || !expect_ptr_arg(vm, args[1], &surf, "SDL_CreateTextureFromSurface", "surface"))
        return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface((SDL_Renderer *)ren, (SDL_Surface *)surf);
    args[0] = t ? val_ptr(t) : val_nil(); return 1;
}

static int nat_SDL_DestroyTexture(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_DestroyTexture", "texture")) return 0;
    SDL_DestroyTexture((SDL_Texture *)p); args[0] = val_nil(); return 1;
}

/* SDL_SetTextureBlendMode(texture, mode) */
static int nat_SDL_SetTextureBlendMode(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 2 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetTextureBlendMode", "texture")
            || !is_int(args[1]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetTextureBlendMode expects (texture, mode).");
        return 0;
    }
    SDL_SetTextureBlendMode((SDL_Texture *)p, (SDL_BlendMode)args[1].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_SetTextureAlphaMod(texture, alpha) */
static int nat_SDL_SetTextureAlphaMod(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 2 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetTextureAlphaMod", "texture")
            || !is_int(args[1]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetTextureAlphaMod expects (texture, alpha).");
        return 0;
    }
    SDL_SetTextureAlphaMod((SDL_Texture *)p, (uint8_t)args[1].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_SetTextureColorMod(texture, r, g, b) */
static int nat_SDL_SetTextureColorMod(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 4 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetTextureColorMod", "texture")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetTextureColorMod expects (texture, r, g, b).");
        return 0;
    }
    SDL_SetTextureColorMod((SDL_Texture *)p,
        (uint8_t)args[1].as.integer, (uint8_t)args[2].as.integer, (uint8_t)args[3].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_QueryTexture(texture) → format, access, w, h */
static int nat_SDL_QueryTexture(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_QueryTexture", "texture")) return 0;
    uint32_t fmt = 0; int access = 0, w = 0, h = 0;
    SDL_QueryTexture((SDL_Texture *)p, &fmt, &access, &w, &h);
    args[0] = val_int((int64_t)fmt);
    args[1] = val_int(access);
    args[2] = val_int(w);
    args[3] = val_int(h);
    return 4;
}

/* SDL_RenderCopy(renderer, texture, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h)
**   Pass src_w=0 to use whole texture as source. */
static int nat_SDL_RenderCopy(VM *vm, Value *args, int nargs)
{
    void *ren = nullptr, *tex = nullptr;
    if (nargs < 10
            || !expect_ptr_arg(vm, args[0], &ren, "SDL_RenderCopy", "renderer")
            || !expect_ptr_arg(vm, args[1], &tex, "SDL_RenderCopy", "texture"))
        return 0;
    SDL_Rect src = {(int)args[2].as.integer, (int)args[3].as.integer,
                    (int)args[4].as.integer, (int)args[5].as.integer};
    SDL_Rect dst = {(int)args[6].as.integer, (int)args[7].as.integer,
                    (int)args[8].as.integer, (int)args[9].as.integer};
    SDL_RenderCopy((SDL_Renderer *)ren,
        (SDL_Texture *)tex,
        (src.w == 0 && src.h == 0) ? nullptr : &src,
        &dst);
    args[0] = val_nil(); return 1;
}

/* SDL_RenderCopyEx(renderer, texture, sx,sy,sw,sh, dx,dy,dw,dh, angle, cx,cy, flip)
**   angle in degrees, cx/cy = rotation centre (relative to dst), flip = SDL_RendererFlip */
static int nat_SDL_RenderCopyEx(VM *vm, Value *args, int nargs)
{
    void *ren = nullptr, *tex = nullptr;
    if (nargs < 14
            || !expect_ptr_arg(vm, args[0], &ren, "SDL_RenderCopyEx", "renderer")
            || !expect_ptr_arg(vm, args[1], &tex, "SDL_RenderCopyEx", "texture"))
        return 0;
    SDL_Rect src = {(int)args[2].as.integer, (int)args[3].as.integer,
                    (int)args[4].as.integer, (int)args[5].as.integer};
    SDL_Rect dst = {(int)args[6].as.integer, (int)args[7].as.integer,
                    (int)args[8].as.integer, (int)args[9].as.integer};
    double angle = is_int(args[10]) ? (double)args[10].as.integer : args[10].as.number;
    SDL_Point centre = {(int)args[11].as.integer, (int)args[12].as.integer};
    SDL_RendererFlip flip = (SDL_RendererFlip)(int)args[13].as.integer;
    SDL_RenderCopyEx((SDL_Renderer *)ren, (SDL_Texture *)tex,
        (src.w == 0 && src.h == 0) ? nullptr : &src, &dst,
        angle, &centre, flip);
    args[0] = val_nil(); return 1;
}

/* SDL_UpdateTexture(texture, x, y, w, h, pixels_Uint8Array, pitch)
**   Upload CPU pixel data to a SDL_TEXTUREACCESS_STATIC texture.
**   pitch = bytes per row (usually w * bytes_per_pixel) */
static int nat_SDL_UpdateTexture(VM *vm, Value *args, int nargs)
{
    void *tex = nullptr;
    if (nargs < 7 || !expect_ptr_arg(vm, args[0], &tex, "SDL_UpdateTexture", "texture")
            || !is_int(args[1]) || !is_int(args[2]) || !is_int(args[3]) || !is_int(args[4])
            || !is_buffer(args[5]) || !is_int(args[6]))
    {
        if (!vm->had_error()) vm->runtime_error(
            "SDL_UpdateTexture expects (texture, x, y, w, h, pixels, pitch).");
        return 0;
    }
    SDL_Rect r = {(int)args[1].as.integer, (int)args[2].as.integer,
                  (int)args[3].as.integer, (int)args[4].as.integer};
    ObjBuffer *buf = as_buffer(args[5]);
    int pitch = (int)args[6].as.integer;
    SDL_UpdateTexture((SDL_Texture *)tex, &r, buf->data, pitch);
    args[0] = val_nil(); return 1;
}

/* SDL_FreeSurface(surface) */
static int nat_SDL_FreeSurface(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_FreeSurface", "surface")) return 0;
    SDL_FreeSurface((SDL_Surface *)p); args[0] = val_nil(); return 1;
}
