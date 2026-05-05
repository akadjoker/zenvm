#include "zen/module_audio.h"

#ifdef ZEN_ENABLE_AUDIO

#include "object.h"
#include "vm.h"
#include "bugl_audio.hpp"

#include <cstring>

namespace zen
{
    /* -------------------------------------------------------
    ** Global engine singleton (one per VM process)
    ** ------------------------------------------------------ */
    static bugl::audio::Engine g_audio;

    /* -------------------------------------------------------
    ** Helpers
    ** ------------------------------------------------------ */
    static bool to_float(Value v, float *out)
    {
        if (is_float(v)) { *out = (float)v.as.number;   return true; }
        if (is_int(v))   { *out = (float)v.as.integer;  return true; }
        return false;
    }

    /* -------------------------------------------------------
    ** audio.init() → bool
    ** ------------------------------------------------------ */
    static int nat_audio_init(VM *vm, Value *args, int)
    {
        args[0] = val_bool(g_audio.init());
        return 1;
    }

    /* audio.shutdown() */
    static int nat_audio_shutdown(VM *vm, Value *args, int)
    {
        g_audio.shutdown();
        return 0;
    }

    /* audio.update() — call every frame to reap finished voices */
    static int nat_audio_update(VM *vm, Value *args, int)
    {
        g_audio.update();
        return 0;
    }

    /* -------------------------------------------------------
    ** Sound bank creation
    ** audio.sfx(path) → sound_id (int)
    ** audio.music(path) → sound_id (int)
    ** audio.waveform(type, amplitude, frequency) → sound_id
    ** audio.noise(type, seed, amplitude) → sound_id
    ** audio.remove(sound_id)
    ** ------------------------------------------------------ */
    static int nat_audio_sfx(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("audio.sfx() expects (path).");
            return 0;
        }
        int id = g_audio.createSfx(as_string(args[0])->chars);
        args[0] = val_int(id);
        return 1;
    }

    static int nat_audio_music(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("audio.music() expects (path).");
            return 0;
        }
        int id = g_audio.createMusic(as_string(args[0])->chars);
        args[0] = val_int(id);
        return 1;
    }

    static int nat_audio_waveform(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3)
        {
            vm->runtime_error("audio.waveform() expects (type, amplitude, frequency).");
            return 0;
        }
        if (!is_int(args[0]))
        {
            vm->runtime_error("audio.waveform(): type must be WAVE_SINE/SQUARE/TRIANGLE/SAW.");
            return 0;
        }
        float amp = 0.5f, freq = 440.0f;
        to_float(args[1], &amp);
        to_float(args[2], &freq);
        int id = g_audio.createWaveform((int)args[0].as.integer, amp, freq);
        args[0] = val_int(id);
        return 1;
    }

    static int nat_audio_noise_sound(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3)
        {
            vm->runtime_error("audio.noise_sound() expects (type, seed, amplitude).");
            return 0;
        }
        if (!is_int(args[0]) || !is_int(args[1]))
        {
            vm->runtime_error("audio.noise_sound(): type and seed must be integers.");
            return 0;
        }
        float amp = 0.5f;
        to_float(args[2], &amp);
        int id = g_audio.createNoise((int)args[0].as.integer, (int)args[1].as.integer, amp);
        args[0] = val_int(id);
        return 1;
    }

    static int nat_audio_remove(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.remove() expects (sound_id).");
            return 0;
        }
        args[0] = val_bool(g_audio.removeSound((int)args[0].as.integer));
        return 1;
    }

    /* -------------------------------------------------------
    ** Playback
    ** audio.play(sound_id, volume?, pitch?, pan?) → handle
    ** audio.play_music(sound_id, loop?, volume?) → handle
    ** audio.stop(handle) → bool
    ** audio.pause(handle) → bool
    ** audio.resume(handle) → bool
    ** audio.playing(handle) → bool
    ** audio.stop_music()
    ** audio.stop_all()
    ** audio.music_playing() → bool
    ** ------------------------------------------------------ */
    static int nat_audio_play(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.play() expects (sound_id, volume?, pitch?, pan?).");
            return 0;
        }
        float vol = 1.0f, pitch = 1.0f, pan = 0.0f;
        if (nargs >= 2) to_float(args[1], &vol);
        if (nargs >= 3) to_float(args[2], &pitch);
        if (nargs >= 4) to_float(args[3], &pan);
        int h = g_audio.playSfx((int)args[0].as.integer, vol, pitch, pan);
        args[0] = val_int(h);
        return 1;
    }

    static int nat_audio_play_music(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.play_music() expects (sound_id, loop?, volume?).");
            return 0;
        }
        bool loop = true;
        float vol = 1.0f;
        if (nargs >= 2 && is_bool(args[1])) loop = args[1].as.boolean;
        if (nargs >= 3) to_float(args[2], &vol);
        int h = g_audio.playMusic((int)args[0].as.integer, loop, vol);
        args[0] = val_int(h);
        return 1;
    }

    static int nat_audio_stop(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.stop() expects (handle).");
            return 0;
        }
        args[0] = val_bool(g_audio.stop((int)args[0].as.integer));
        return 1;
    }

    static int nat_audio_pause(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.pause() expects (handle).");
            return 0;
        }
        args[0] = val_bool(g_audio.pause((int)args[0].as.integer));
        return 1;
    }

    static int nat_audio_resume(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.resume() expects (handle).");
            return 0;
        }
        args[0] = val_bool(g_audio.resume((int)args[0].as.integer));
        return 1;
    }

    static int nat_audio_playing(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("audio.playing() expects (handle).");
            return 0;
        }
        args[0] = val_bool(g_audio.isPlaying((int)args[0].as.integer));
        return 1;
    }

    static int nat_audio_stop_music(VM *vm, Value *args, int)
    {
        g_audio.stopMusic();
        return 0;
    }

    static int nat_audio_stop_all(VM *vm, Value *args, int)
    {
        g_audio.stopAll();
        return 0;
    }

    static int nat_audio_music_playing(VM *vm, Value *args, int)
    {
        args[0] = val_bool(g_audio.isMusicPlaying());
        return 1;
    }

    /* -------------------------------------------------------
    ** Per-voice controls
    ** audio.set_volume(handle, v)
    ** audio.set_pitch(handle, v)
    ** audio.set_pan(handle, v)
    ** ------------------------------------------------------ */
    static int nat_audio_set_volume(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]))
        {
            vm->runtime_error("audio.set_volume() expects (handle, volume).");
            return 0;
        }
        float v = 1.0f; to_float(args[1], &v);
        args[0] = val_bool(g_audio.setVolume((int)args[0].as.integer, v));
        return 1;
    }

    static int nat_audio_set_pitch(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]))
        {
            vm->runtime_error("audio.set_pitch() expects (handle, pitch).");
            return 0;
        }
        float v = 1.0f; to_float(args[1], &v);
        args[0] = val_bool(g_audio.setPitch((int)args[0].as.integer, v));
        return 1;
    }

    static int nat_audio_set_pan(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]))
        {
            vm->runtime_error("audio.set_pan() expects (handle, pan).");
            return 0;
        }
        float v = 0.0f; to_float(args[1], &v);
        args[0] = val_bool(g_audio.setPan((int)args[0].as.integer, v));
        return 1;
    }

    /* -------------------------------------------------------
    ** Master/group volumes
    ** audio.set_master_volume(v)
    ** audio.set_sfx_volume(v)
    ** audio.set_music_volume(v)
    ** ------------------------------------------------------ */
    static int nat_audio_set_master_volume(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("audio.set_master_volume() expects (volume)."); return 0; }
        float v = 1.0f; to_float(args[0], &v);
        g_audio.setMasterVolume(v);
        return 0;
    }

    static int nat_audio_set_sfx_volume(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("audio.set_sfx_volume() expects (volume)."); return 0; }
        float v = 1.0f; to_float(args[0], &v);
        g_audio.setSfxVolume(v);
        return 0;
    }

    static int nat_audio_set_music_volume(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("audio.set_music_volume() expects (volume)."); return 0; }
        float v = 1.0f; to_float(args[0], &v);
        g_audio.setMusicVolume(v);
        return 0;
    }

    /* -------------------------------------------------------
    ** Effects
    ** audio.sfx_delay(enable, decay?)
    ** audio.music_lowpass(enable, cutoff?)
    ** ------------------------------------------------------ */
    static int nat_audio_sfx_delay(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_bool(args[0]))
        {
            vm->runtime_error("audio.sfx_delay() expects (bool, decay?).");
            return 0;
        }
        float decay = 0.5f;
        if (nargs >= 2) to_float(args[1], &decay);
        g_audio.enableSfxDelay(args[0].as.boolean, decay);
        return 0;
    }

    static int nat_audio_music_lowpass(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_bool(args[0]))
        {
            vm->runtime_error("audio.music_lowpass() expects (bool, cutoff?).");
            return 0;
        }
        float cutoff = 1000.0f;
        if (nargs >= 2) to_float(args[1], &cutoff);
        g_audio.enableMusicLowPass(args[0].as.boolean, cutoff);
        return 0;
    }

    /* -------------------------------------------------------
    ** Constants table (injected as globals on import)
    ** ------------------------------------------------------ */
    static const NativeConst audio_consts[] = {
        {"WAVE_SINE",     val_int(bugl::audio::Engine::WAVE_SINE)},
        {"WAVE_SQUARE",   val_int(bugl::audio::Engine::WAVE_SQUARE)},
        {"WAVE_TRIANGLE", val_int(bugl::audio::Engine::WAVE_TRIANGLE)},
        {"WAVE_SAW",      val_int(bugl::audio::Engine::WAVE_SAW)},
        {"NOISE_WHITE",   val_int(bugl::audio::Engine::NOISE_WHITE)},
        {"NOISE_PINK",    val_int(bugl::audio::Engine::NOISE_PINK)},
        {"NOISE_BROWNIAN",val_int(bugl::audio::Engine::NOISE_BROWNIAN)},
    };

    /* -------------------------------------------------------
    ** Table
    ** ------------------------------------------------------ */
    static const NativeReg audio_funcs[] = {
        {"init",              nat_audio_init,             0},
        {"shutdown",          nat_audio_shutdown,         0},
        {"update",            nat_audio_update,           0},
        /* sound bank */
        {"sfx",               nat_audio_sfx,              1},
        {"music",             nat_audio_music,            1},
        {"waveform",          nat_audio_waveform,         3},
        {"noise_sound",       nat_audio_noise_sound,      3},
        {"remove",            nat_audio_remove,           1},
        /* playback */
        {"play",              nat_audio_play,            -1},
        {"play_music",        nat_audio_play_music,      -1},
        {"stop",              nat_audio_stop,             1},
        {"pause",             nat_audio_pause,            1},
        {"resume",            nat_audio_resume,           1},
        {"playing",           nat_audio_playing,          1},
        {"stop_music",        nat_audio_stop_music,       0},
        {"stop_all",          nat_audio_stop_all,         0},
        {"music_playing",     nat_audio_music_playing,    0},
        /* per-voice */
        {"set_volume",        nat_audio_set_volume,       2},
        {"set_pitch",         nat_audio_set_pitch,        2},
        {"set_pan",           nat_audio_set_pan,          2},
        /* master/group */
        {"set_master_volume", nat_audio_set_master_volume, 1},
        {"set_sfx_volume",    nat_audio_set_sfx_volume,   1},
        {"set_music_volume",  nat_audio_set_music_volume,  1},
        /* effects */
        {"sfx_delay",         nat_audio_sfx_delay,       -1},
        {"music_lowpass",     nat_audio_music_lowpass,   -1},
    };

    const NativeLib zen_lib_audio = {
        "audio", audio_funcs, 25, audio_consts, 7, nullptr
    };
}

#endif /* ZEN_ENABLE_AUDIO */
