// Headless runner: plays a ROM with a scripted input file and saves screenshots.
// Script lines: <frames> <keys> [screenshot.ppm | @addr,addr (print) | !addr=val,addr=val (write)]
// Set HARNESS_WAV=path to record the game's sound (stereo, 32768 Hz) to a WAV file.
// A line "^" power-cycles the console (save memory survives, like a real cart).
// Set HARNESS_SAV=path to load save memory from that file at start (if it exists) and write it
// back at the end, so a save made by one ROM can be loaded by another (save migration tests).
// A line "&addr" names the game's frame counter. From then on, one script frame = one game loop
// (the emulator runs until the counter changes), so boot time and slow frames do not shift a script.
// Key bits: A=1 B=2 SELECT=4 START=8 RIGHT=16 LEFT=32 UP=64 DOWN=128
#include <mgba/flags.h>      // the library's build options: they change struct mCore's layout
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/blip_buf.h>
#include <stdio.h>
#include <stdlib.h>

static void nolog(struct mLogger* l, int c, enum mLogLevel lv, const char* f, va_list a) {
    (void)l; (void)c; (void)lv; (void)f; (void)a;
}
static struct mLogger logger = { .log = nolog };

static void save_ppm(const char* path, color_t* buf, unsigned w, unsigned h) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6 %u %u 255\n", w, h);
    for (unsigned i = 0; i < w * h; i++) {
        uint32_t p = buf[i];
        unsigned char rgb[3] = { p & 0xFF, (p >> 8) & 0xFF, (p >> 16) & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static long drain_audio(struct mCore* core, FILE* wav) {
    blip_t* l = core->getAudioChannel(core, 0);
    blip_t* r = core->getAudioChannel(core, 1);
    long total = 0;
    short buf[2048 * 2];
    int n;
    while ((n = blip_samples_avail(l)) > 0) {
        if (n > 2048) n = 2048;
        blip_read_samples(l, buf, n, 1);
        blip_read_samples(r, buf + 1, n, 1);
        fwrite(buf, sizeof(short), n * 2, wav);
        total += n;
    }
    return total;
}

static void finish_wav(FILE* wav, long samples) {
    unsigned data = samples * 4, rate = 32768;
    unsigned char h[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 1,0, 2,0,
                            0,0,0,0, 0,0,0,0, 4,0, 16,0, 'd','a','t','a', 0,0,0,0 };
    unsigned riff = 36 + data, byterate = rate * 4;
    for (int k = 0; k < 4; k++) {
        h[4 + k] = riff >> (8 * k); h[24 + k] = rate >> (8 * k);
        h[28 + k] = byterate >> (8 * k); h[40 + k] = data >> (8 * k);
    }
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fclose(wav);
}

int main(int argc, char** argv) {
    if (argc < 3) { puts("usage: harness rom script [frame_counter_addr]"); return 1; }
    mLogSetDefaultLogger(&logger);
    struct mCore* core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) { puts("core init failed"); return 1; }
    mCoreInitConfig(core, NULL);
    unsigned w, h;
    core->desiredVideoDimensions(core, &w, &h);
    color_t* buf = calloc(w * h, sizeof(color_t));
    core->setVideoBuffer(core, buf, w);
    if (!mCoreLoadFile(core, argv[1])) { puts("load failed"); return 1; }
    core->reset(core);

    // optional save memory file
    const char* sav_path = getenv("HARNESS_SAV");
    if (sav_path) {
        FILE* f = fopen(sav_path, "rb");
        if (f) {
            static unsigned char data[0x20000];
            size_t n = fread(data, 1, sizeof data, f);
            fclose(f);
            core->savedataRestore(core, data, n, false);
            core->reset(core);
        }
    }

    // optional sound recording
    const char* wav_path = getenv("HARNESS_WAV");
    FILE* wav = NULL;
    long wav_samples = 0;
    if (wav_path) {
        wav = fopen(wav_path, "wb");
        char header[44] = { 0 };
        fwrite(header, 1, 44, wav);                  // filled in at the end
        core->setAudioBufferSize(core, 2048);
        blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), 32768);
        blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), 32768);
    }

    FILE* s = fopen(argv[2], "r");
    char line[256];
    unsigned frame_addr = argc > 3 ? strtoul(argv[3], NULL, 16) : 0;   // optional: game frame counter
    while (fgets(line, sizeof line, s)) {
        if (line[0] == '&') { frame_addr = strtoul(line + 1, NULL, 16); continue; }
        if (line[0] == '^') { core->reset(core); continue; }   // power cycle; save memory stays
        int frames; unsigned keys; char shot[200] = "";
        if (line[0] == '#' || sscanf(line, "%d %u %199s", &frames, &keys, shot) < 2) continue;
        core->setKeys(core, keys);
        for (int i = 0; i < frames; i++) {
            if (!frame_addr) { { core->runFrame(core); if (wav) wav_samples += drain_audio(core, wav); } continue; }
            uint32_t f0 = core->busRead32(core, frame_addr);
            for (int guard = 0; guard < 240 && core->busRead32(core, frame_addr) == f0; guard++)
                { core->runFrame(core); if (wav) wav_samples += drain_audio(core, wav); }
        }
        if (shot[0] == '@') {   // @addr,addr: print 32-bit values from memory
            char* q = shot + 1;
            while (*q) {
                unsigned a = strtoul(q, &q, 16);
                printf("[%08X]=%d ", a, (int)core->busRead32(core, a));
                if (*q == ',') q++;
            }
            puts("");
        } else if (shot[0] == '!') {   // !addr=val,addr=val: write 32-bit values (test setup)
            char* q = shot + 1;
            while (*q) {
                unsigned a = strtoul(q, &q, 16);
                if (*q == '=') q++;
                core->busWrite32(core, a, (int32_t)strtol(q, &q, 10));
                if (*q == ',') q++;
            }
        } else if (shot[0]) save_ppm(shot, buf, w, h);
    }
    if (wav) finish_wav(wav, wav_samples);
    if (sav_path) {
        void* data = NULL;
        size_t n = core->savedataClone(core, &data);
        fprintf(stderr, "save memory: %zu bytes\n", n);
        FILE* f = fopen(sav_path, "wb");
        if (f && data) fwrite(data, 1, n, f);
        if (f) fclose(f);
        free(data);
    }
    puts("ok");
    core->deinit(core);
    return 0;
}
