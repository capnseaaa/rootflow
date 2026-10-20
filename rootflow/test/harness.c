// Headless runner: plays a ROM with a scripted input file and saves screenshots.
// Script lines: <frames> <keys> [screenshot.ppm | @addr,addr (print) | !addr=val,addr=val (write)]
// Key bits: A=1 B=2 SELECT=4 START=8 RIGHT=16 LEFT=32 UP=64 DOWN=128
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
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

int main(int argc, char** argv) {
    if (argc < 3) { puts("usage: harness rom script"); return 1; }
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

    FILE* s = fopen(argv[2], "r");
    char line[256];
    while (fgets(line, sizeof line, s)) {
        int frames; unsigned keys; char shot[200] = "";
        if (line[0] == '#' || sscanf(line, "%d %u %199s", &frames, &keys, shot) < 2) continue;
        core->setKeys(core, keys);
        for (int i = 0; i < frames; i++) core->runFrame(core);
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
    puts("ok");
    core->deinit(core);
    return 0;
}
