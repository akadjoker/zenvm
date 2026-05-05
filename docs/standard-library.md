# BuLang / Zen Standard Library Notes

This page is based on the tutorial examples. Adjust names if the VM implementation changes.

## Global functions

### Output

```zen
print(value);
```

### Conversion

```zen
int(value);
float(value);
```

### Length

```zen
len(value);
```

Some types also support `.len()`.

## Math functions

These are compiled to dedicated bytecode opcodes in the VM, so they do not pay normal function call overhead in hot paths.

```zen
sin(x)
cos(x)
tan(x)
atan2(y, x)
rad(degrees)
deg(radians)
sqrt(x)
pow(a, b)
abs(x)
floor(x)
ceil(x)
log(x)
exp(x)
clock()
```

## String methods

```zen
s.len()
s.upper()
s.lower()
s.sub(start, end)
s.find(pattern)
s.replace(old, new)
s.starts_with(prefix)
s.ends_with(suffix)
s.trim()
s.char_at(index)
s.split(separator)
```

## Array methods

```zen
a.len()
a.push(value)
a.pop()
a.contains(value)
a.index_of(value)
a.reverse()
a.slice(start, end)
a.insert(index, value)
a.remove(index)
a.clear()
a.join(separator)
a.sort()
a.sort("desc")
```

## Map methods

```zen
m.set(key, value)
m.get(key)
m.has(key)
m.delete(key)
m.clear()
m.size()
m.keys()
m.values()
```

## Set methods

```zen
s.add(value)
s.has(value)
s.delete(value)
s.clear()
s.size()
s.values()
```

## Process/runtime functions and statements

Seen in tutorials:

```zen
process
frame
advance_process()
spawn
yield
resume(fiber)
father
son
```

These are useful for cooperative scheduling and game-like update flows.

`advance_process()` resumes suspended processes for one scheduler step. When embedding Zen in a host application, the C++ side can instead call the VM process tick functions directly.

## Typed buffers / arrays

The examples mention typed array names such as:

```zen
Int8Array
Uint8Array
Int16Array
Uint16Array
Int32Array
Uint32Array
Float32Array
Float64Array
```

Document their exact constructors and methods according to your C++ VM implementation.

## Bytecode notes

Script-facing standard-library usage round-trips through `.znb` bytecode, but any host-specific native registration still needs to happen before loading the bytecode in an embedding application.

---

# Native modules (`zen_gl` runner)

Available when running with `bin/zen_gl`. All use `import <name>`.

---

## `import image`

Load, save, encode, and transform pixel data.

```zen
var pixels, w, h, ch = image.load(path)           // → Uint8Array, w, h, channels
image.save(path, w, h, ch, pixels)                // PNG/JPG/BMP by extension
var encoded = image.encode("png", w, h, ch, pixels)           // → Uint8Array
var encoded = image.encode("jpg", w, h, ch, pixels, quality)  // quality 1-100
image.set_flip_on_load(true)                      // flip Y before load (OpenGL UVs)
var flipped = image.flip_v(pixels, w, h, ch)     // → Uint8Array (new buffer)
var flipped = image.flip_h(pixels, w, h, ch)     // → Uint8Array (new buffer)
```

---

## `import noise`

Procedural noise via stb_perlin.

```zen
var v = noise2(x, y)                              // 2D simplex-style, -1..1
var v = noise2(x, y, octaves, lacunarity, gain)
var v = noise3(x, y, z)                           // 3D, optional wrap: xw, yw, zw
var v = noise3(x, y, z, xw, yw, zw)
var v = noise3_seed(x, y, z, xw, yw, zw, seed)
var v = fbm(x, y, z, lacunarity, gain, octaves)           // fractal brownian motion
var v = ridge(x, y, z, lacunarity, gain, offset, octaves) // ridged multifractal
var v = turbulence(x, y, z, lacunarity, gain, octaves)
```

---

## `import rectpack`

Rectangle bin-packing via stb_rect_pack.

```zen
// sizes = Float32Array of [w0,h0, w1,h1, ...]
// returns Float32Array of [x0,y0,ok0, x1,y1,ok1, ...]  (ok=1 packed, 0=failed)
var result = rectpack.pack(atlas_w, atlas_h, sizes)
```

---

## `import font`

TTF font rasterisation via stb_truetype.

```zen
var ttf = font.load(path)                         // → Uint8Array (raw TTF bytes)

// bake: all args mandatory
// returns: bitmap (Uint8Array), atlas_w, atlas_h, chardata (Float32Array)
// chardata layout: 7 floats per char: [x0,y0,x1,y1, xoff,yoff, xadvance]
var bmp, aw, ah, cd = font.bake(ttf, px_height, atlas_w, atlas_h, first_char, num_chars)

// get quad UVs + position for one codepoint
// returns: x0,y0,x1,y1, s0,t0,s1,t1, xadvance  (9 values)
var x0,y0,x1,y1, s0,t0,s1,t1, xadv = font.quad(chardata, atlas_w, atlas_h, first_char, codepoint, xpos, ypos)

// vertical metrics
var ascent, descent, line_gap = font.metrics(ttf, px_height)

// measure a string (uses chardata)
var width, height = font.measure(chardata, first_char, text)
```

---

## `import audio`

Audio playback via miniaudio.

```zen
audio.init()
audio.update()      // call every frame
audio.shutdown()

// load files (OGG/WAV/MP3/FLAC)
var id = audio.sfx(path)         // one-shot sound effect
var id = audio.music(path)       // streaming music

// synthesis
var id = audio.waveform(wave_type, freq, amplitude)   // WAVE_SINE/SQUARE/TRIANGLE/SAW
var id = audio.noise_sound(noise_type, amplitude)     // NOISE_WHITE/PINK/BROWNIAN

audio.play(id)
audio.stop(id)
audio.pause(id)
audio.resume(id)
var ok = audio.playing(id)

audio.play_music(id)
audio.stop_music()
var ok = audio.music_playing()

audio.set_volume(id, 0.0..1.0)
audio.set_pitch(id, semitones)
audio.set_pan(id, -1.0..1.0)
audio.set_master_volume(v)
audio.set_sfx_volume(v)
audio.set_music_volume(v)

audio.sfx_delay(id, seconds)
audio.music_lowpass(id, cutoff_hz)

audio.stop_all()
audio.remove(id)
```

Constants: `WAVE_SINE=0`, `WAVE_SQUARE=1`, `WAVE_TRIANGLE=2`, `WAVE_SAW=3`,
`NOISE_WHITE=0`, `NOISE_PINK=1`, `NOISE_BROWNIAN=2`

---

## `import gl`

Raw OpenGL 3.3 core / GLES 3.0 bindings. All standard GL functions are exposed with exact C names. The functions below receive typed buffers — those are the ones that differ from plain C.

### Loader

```zen
LoadOpenGLExtensions()   // call once after creating the GL context (GLFW or SDL)
```

### Buffer upload — `glBufferData` / `glBufferSubData`

```zen
// Upload a typed buffer (size computed automatically from buffer element count)
glBufferData(GL_ARRAY_BUFFER, Float32Array([...]), GL_STATIC_DRAW)
glBufferData(GL_ARRAY_BUFFER, Uint8Array([...]),   GL_DYNAMIC_DRAW)

// Allocate without data (pass size as int)
glBufferData(GL_ARRAY_BUFFER, 1024, GL_DYNAMIC_DRAW)

// Partial update
glBufferSubData(GL_ARRAY_BUFFER, byte_offset, Float32Array([...]))
```

### Texture upload — `glTexImage2D` / `glTexSubImage2D`

```zen
// Upload pixels from a Uint8Array (e.g. from image.load)
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels)

// Pass nil to allocate without data
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nil)

// Partial update
glTexSubImage2D(GL_TEXTURE_2D, 0, xoff, yoff, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels)
```

### Readback — `glReadPixels`

```zen
// Buffer must be pre-allocated to the right size: w * h * channels * bytes_per_channel
var buf = Uint8Array(w * h * 4)
glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf)
```

### Uniform arrays / matrices

```zen
// loc = glGetUniformLocation(prog, "name")
// All array/matrix uniforms require a Float32Array buffer:
glUniform1fv(loc, count, Float32Array([...]))
glUniform2fv(loc, count, Float32Array([...]))
glUniform3fv(loc, count, Float32Array([...]))
glUniform4fv(loc, count, Float32Array([...]))

glUniformMatrix2fv(loc, count, Float32Array([...]))   // transpose always false
glUniformMatrix3fv(loc, count, Float32Array([...]))
glUniformMatrix4fv(loc, count, Float32Array([...]))   // 16 floats per matrix
```

---

## `import sdl2`

SDL2 window, OpenGL context, 2D renderer, events, and timer.

### Window

```zen
SDL_Init(SDL_INIT_VIDEO)
var win = SDL_CreateWindow(title, x, y, w, h, flags)  // flags: SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
SDL_DestroyWindow(win)
var (w, h) = SDL_GetWindowSize(win)
SDL_SetWindowSize(win, w, h)
SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP)
```

### OpenGL context (use with `import gl`)

```zen
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
var ctx = SDL_GL_CreateContext(win)
SDL_GL_MakeCurrent(win, ctx)
SDL_GL_SetSwapInterval(1)    // 1=vsync, 0=immediate, -1=adaptive
LoadOpenGLExtensions()       // from import gl
// ... draw ...
SDL_GL_SwapWindow(win)
SDL_GL_DeleteContext(ctx)
```

### 2D Renderer (no shaders needed)

```zen
var ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
SDL_SetRenderDrawColor(ren, r, g, b, a)
SDL_RenderClear(ren)
SDL_RenderFillRect(ren, x, y, w, h)
SDL_RenderDrawRect(ren, x, y, w, h)
SDL_RenderDrawLine(ren, x1, y1, x2, y2)
SDL_RenderPresent(ren)
SDL_DestroyRenderer(ren)

// Textures
var tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h)
SDL_UpdateTexture(tex, 0, 0, w, h, pixels_Uint8Array, w*4)   // pitch = bytes per row
SDL_RenderCopy(ren, tex, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h)
// pass src_w=0,src_h=0 to use full texture as source
SDL_DestroyTexture(tex)
```

### Events

```zen
var ev = SDL_PollEvent()     // 0 = queue empty
while (ev != 0) {
    if (ev == SDL_QUIT) { ... }
    if (ev == SDL_KEYDOWN) {
        var (scan, sym, mod, rep) = SDL_event_key()
        // scan = SDL_SCANCODE_*, rep=1 if auto-repeat
    }
    if (ev == SDL_MOUSEMOTION) {
        var (x, y, dx, dy, buttons) = SDL_event_mouse_motion()
    }
    if (ev == SDL_MOUSEBUTTONDOWN) {
        var (btn, x, y, clicks) = SDL_event_mouse_button()
        // btn = SDL_BUTTON_LEFT / MIDDLE / RIGHT
    }
    if (ev == SDL_MOUSEWHEEL) {
        var (wx, wy, dir) = SDL_event_mouse_wheel()
    }
    if (ev == SDL_WINDOWEVENT) {
        var (wid, wev, d1, d2) = SDL_event_window()
        // wev = SDL_WINDOWEVENT_RESIZED, d1=w, d2=h
    }
    if (ev == SDL_TEXTINPUT) {
        var text = SDL_event_text()    // UTF-8 string
    }
    ev = SDL_PollEvent()
}

// Polling (continuous state, outside event loop)
if (SDL_GetKeyboardState(SDL_SCANCODE_W)) { ... }
var (mx, my, mbtn) = SDL_GetMouseState()
```

### Timer

```zen
var ms = SDL_GetTicks()
SDL_Delay(16)
var t0 = SDL_GetPerformanceCounter()
var freq = SDL_GetPerformanceFrequency()
var dt = float(SDL_GetPerformanceCounter() - t0) / float(freq)
```
