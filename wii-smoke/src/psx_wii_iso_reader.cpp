/*
 * Native Wii CUE/BIN reader for the PSXRecomp CD-ROM device.
 *
 * The desktop ISOReader currently includes CHD/libchdr and mod-runtime
 * integration. This Wii adapter deliberately targets the private boot path's
 * CUE + raw BIN files and avoids desktop/CHD dependencies. It preserves raw
 * Mode 2 sectors for CD-ROM reads and returns the 2048-byte user-data payload
 * for normal sector requests.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
static const uint32_t kRawSectorBytes = 2352u;
static const uint32_t kCookedSectorBytes = 2048u;

struct Segment {
    std::string path;
    FILE *file;
    uint32_t start_lba;
    uint32_t sectors;
    uint32_t sector_bytes;
    Segment() : file(nullptr), start_lba(0), sectors(0), sector_bytes(kRawSectorBytes) {}
};

struct Track {
    int number;
    bool audio;
    int segment;
    uint32_t index01;
    uint32_t index00;
    bool has_index01;
    bool has_index00;
    uint32_t pregap;
    uint32_t postgap;
    uint32_t start_lba;
    uint32_t pregap_lba;
    Track() : number(0), audio(false), segment(0), index01(0), index00(0),
        has_index01(false), has_index00(false), pregap(0), postgap(0),
        start_lba(0), pregap_lba(0) {}
};

struct Disc {
    std::vector<Segment> segments;
    std::vector<Track> tracks;
    uint32_t sectors;
    Disc() : sectors(0) {}
    ~Disc() {
        for (size_t i = 0; i < segments.size(); ++i) {
            if (segments[i].file) fclose(segments[i].file);
        }
    }
};

static uint32_t time_to_frames(const char *time) {
    unsigned mm = 0, ss = 0, ff = 0;
    if (!time || sscanf(time, "%u:%u:%u", &mm, &ss, &ff) != 3) return 0;
    if (ss >= 60u) ss = 59u;
    if (ff >= 75u) ff = 74u;
    return (mm * 60u + ss) * 75u + ff;
}

static std::string parent_path(const char *path) {
    std::string s = path ? path : "";
    size_t pos = s.find_last_of("/\\");
    return pos == std::string::npos ? std::string("") : s.substr(0, pos + 1);
}

static std::string resolve_file_path(const std::string& dir, const char *name) {
    std::string p = name ? name : "";
    if (p.empty()) return p;
    if (p[0] == '/' || p[0] == '\\' || p.find(':') != std::string::npos)
        return p;
    return dir + p;
}

static void skip_space(char **p) {
    while (**p == ' ' || **p == '\t') ++(*p);
}

static bool parse_file_name(char *line, char *out, size_t cap) {
    char *p = strstr(line, "FILE");
    if (!p) return false;
    p += 4;
    skip_space(&p);
    char *end = nullptr;
    if (*p == '"') {
        ++p;
        end = strchr(p, '"');
        if (!end) return false;
    } else {
        end = p;
        while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') ++end;
    }
    size_t len = (size_t)(end - p);
    if (len == 0 || len >= cap) return false;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static int active_track_index(const Disc *d, uint32_t lba) {
    if (!d || d->tracks.empty()) return -1;
    int found = 0;
    for (size_t i = 0; i < d->tracks.size(); ++i) {
        if (d->tracks[i].start_lba <= lba) found = (int)i;
        else break;
    }
    return found;
}

static bool locate_sector(Disc *d, uint32_t lba, Segment **segment_out,
                          uint32_t *file_lba_out) {
    if (!d || lba >= d->sectors) return false;
    for (size_t i = 0; i < d->segments.size(); ++i) {
        Segment *s = &d->segments[i];
        if (lba >= s->start_lba && lba < s->start_lba + s->sectors) {
            *segment_out = s;
            *file_lba_out = lba - s->start_lba;
            return true;
        }
    }
    return false;
}

static bool read_raw(Disc *d, uint32_t lba, uint8_t *out, int capacity) {
    if (!d || !out || capacity < (int)kRawSectorBytes) return false;
    Segment *s = nullptr;
    uint32_t file_lba = 0;
    if (!locate_sector(d, lba, &s, &file_lba) || !s || !s->file) return false;

    if (s->sector_bytes >= kRawSectorBytes) {
        if (fseek(s->file, (long)((uint64_t)file_lba * s->sector_bytes), SEEK_SET) != 0)
            return false;
        return fread(out, 1, kRawSectorBytes, s->file) == kRawSectorBytes;
    }

    if (s->sector_bytes == kCookedSectorBytes) {
        uint8_t user[kCookedSectorBytes];
        if (fseek(s->file, (long)((uint64_t)file_lba * kCookedSectorBytes), SEEK_SET) != 0)
            return false;
        if (fread(user, 1, kCookedSectorBytes, s->file) != kCookedSectorBytes)
            return false;
        memset(out, 0, kRawSectorBytes);
        out[0] = 0;
        memset(out + 1, 0xFF, 10);
        out[11] = 0;
        auto bcd = [](uint32_t v) -> uint8_t { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); };
        uint32_t absolute = lba + 150u;
        out[12] = bcd(absolute / (60u * 75u));
        out[13] = bcd((absolute / 75u) % 60u);
        out[14] = bcd(absolute % 75u);
        out[15] = 2; /* Mode 2 */
        memset(out + 16, 0, 8);
        memcpy(out + 24, user, kCookedSectorBytes);
        return true;
    }
    return false;
}

static uint16_t q_crc(const uint8_t *p, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
    }
    return (uint16_t)~crc;
}

static uint8_t to_bcd(uint32_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static bool parse_cue(Disc *d, const char *cue_path) {
    FILE *f = fopen(cue_path, "rb");
    if (!f) return false;
    const std::string dir = parent_path(cue_path);
    int current_segment = -1;
    int current_track = -1;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        skip_space(&p);
        if (strncmp(p, "FILE", 4) == 0) {
            char name[512];
            if (!parse_file_name(p, name, sizeof(name))) { fclose(f); return false; }
            Segment seg;
            seg.path = resolve_file_path(dir, name);
            d->segments.push_back(seg);
            current_segment = (int)d->segments.size() - 1;
            current_track = -1;
        } else if (strncmp(p, "TRACK", 5) == 0 && current_segment >= 0) {
            int number = 0;
            char mode[64] = {};
            if (sscanf(p + 5, "%d %63s", &number, mode) < 2) continue;
            Track t;
            t.number = number;
            t.audio = (strcmp(mode, "AUDIO") == 0);
            t.segment = current_segment;
            d->tracks.push_back(t);
            current_track = (int)d->tracks.size() - 1;
            uint32_t bytes = t.audio || strstr(mode, "/2352") ? kRawSectorBytes : kCookedSectorBytes;
            Segment& seg = d->segments[(size_t)current_segment];
            if (seg.sector_bytes == kRawSectorBytes || bytes == kRawSectorBytes)
                seg.sector_bytes = bytes;
        } else if (strncmp(p, "INDEX", 5) == 0 && current_track >= 0) {
            int idx = -1;
            char tm[32] = {};
            if (sscanf(p + 5, "%d %31s", &idx, tm) < 2) continue;
            Track& t = d->tracks[(size_t)current_track];
            if (idx == 0) { t.index00 = time_to_frames(tm); t.has_index00 = true; }
            if (idx == 1) { t.index01 = time_to_frames(tm); t.has_index01 = true; }
        } else if (strncmp(p, "PREGAP", 6) == 0 && current_track >= 0) {
            char tm[32] = {};
            if (sscanf(p + 6, "%31s", tm) == 1)
                d->tracks[(size_t)current_track].pregap = time_to_frames(tm);
        } else if (strncmp(p, "POSTGAP", 7) == 0 && current_track >= 0) {
            char tm[32] = {};
            if (sscanf(p + 7, "%31s", tm) == 1)
                d->tracks[(size_t)current_track].postgap = time_to_frames(tm);
        }
    }
    fclose(f);

    if (d->segments.empty()) {
        /* Also tolerate a path directly to a BIN or ISO as a single data track. */
        Segment seg;
        seg.path = cue_path;
        seg.sector_bytes = kRawSectorBytes;
        d->segments.push_back(seg);
        Track t;
        t.number = 1;
        t.segment = 0;
        t.has_index01 = true;
        t.index01 = 0;
        d->tracks.push_back(t);
    }
    if (d->tracks.empty()) {
        Track t;
        t.number = 1;
        t.segment = 0;
        t.has_index01 = true;
        t.index01 = 0;
        d->tracks.push_back(t);
    }

    for (size_t i = 0; i < d->segments.size(); ++i) {
        Segment& s = d->segments[i];
        s.file = fopen(s.path.c_str(), "rb");
        if (!s.file) return false;
        if (fseek(s.file, 0, SEEK_END) != 0) return false;
        long size = ftell(s.file);
        if (size < 0 || fseek(s.file, 0, SEEK_SET) != 0) return false;
        if ((uint64_t)size < s.sector_bytes) return false;
        s.sectors = (uint32_t)((uint64_t)size / s.sector_bytes);
    }

    uint32_t base = 0;
    for (size_t i = 0; i < d->segments.size(); ++i) {
        d->segments[i].start_lba = base;
        base += d->segments[i].sectors;
    }
    d->sectors = base;
    for (size_t i = 0; i < d->tracks.size(); ++i) {
        Track& t = d->tracks[i];
        if (t.segment < 0 || (size_t)t.segment >= d->segments.size()) return false;
        const Segment& s = d->segments[(size_t)t.segment];
        uint32_t offset = t.has_index01 ? t.index01 : 0u;
        if (offset >= s.sectors && s.sectors != 0) offset = 0;
        t.start_lba = s.start_lba + offset;
        if (t.has_index00) t.pregap_lba = s.start_lba + t.index00;
        else if (t.pregap > 0) t.pregap_lba = t.start_lba > t.pregap ? t.start_lba - t.pregap : 0;
        else t.pregap_lba = t.start_lba;
    }
    return d->sectors > 0 && !d->tracks.empty();
}
} // namespace

extern "C" {

void* iso_open(const char *path) {
    if (!path || !*path) return nullptr;
    Disc *d = new Disc();
    if (!parse_cue(d, path)) { delete d; return nullptr; }
    return d;
}

int iso_read_raw_sector(void *handle, uint32_t lba, uint8_t *buffer, int size) {
    return read_raw(static_cast<Disc*>(handle), lba, buffer, size) ? 1 : 0;
}

int iso_read_sector(void *handle, uint32_t lba, uint8_t *buffer, int size) {
    if (!handle || !buffer || size < (int)kCookedSectorBytes) return 0;
    uint8_t raw[kRawSectorBytes];
    if (!read_raw(static_cast<Disc*>(handle), lba, raw, sizeof(raw))) return 0;
    uint32_t offset = raw[15] == 2 ? 24u : 16u;
    memcpy(buffer, raw + offset, kCookedSectorBytes);
    return 1;
}

int iso_read_subq(void *handle, uint32_t lba, uint8_t *buffer, int size, int *valid) {
    Disc *d = static_cast<Disc*>(handle);
    if (!d || !buffer || size < 12 || !valid || lba >= d->sectors) return 0;
    int ti = active_track_index(d, lba);
    if (ti < 0) return 0;
    const Track& t = d->tracks[(size_t)ti];
    uint32_t relative = lba >= t.start_lba ? lba - t.start_lba : 0u;
    uint32_t absolute = lba + 150u;
    memset(buffer, 0, 12);
    buffer[0] = t.audio ? 0x01 : 0x41;
    buffer[1] = to_bcd((uint32_t)t.number);
    buffer[2] = (lba < t.start_lba) ? 0x00 : 0x01;
    buffer[3] = to_bcd(relative / (60u * 75u));
    buffer[4] = to_bcd((relative / 75u) % 60u);
    buffer[5] = to_bcd(relative % 75u);
    buffer[6] = to_bcd(absolute / (60u * 75u));
    buffer[7] = to_bcd((absolute / 75u) % 60u);
    buffer[8] = to_bcd(absolute % 75u);
    uint16_t crc = q_crc(buffer, 10);
    buffer[10] = (uint8_t)crc;
    buffer[11] = (uint8_t)(crc >> 8);
    *valid = 1;
    return 1;
}

int iso_has_subq_replacements(void *handle) { (void)handle; return 0; }
uint32_t iso_sector_count(void *handle) { return handle ? static_cast<Disc*>(handle)->sectors : 0; }
uint32_t iso_cdda_sector_count(void *handle) { return iso_sector_count(handle); }
int iso_track_count(void *handle) { return handle ? (int)static_cast<Disc*>(handle)->tracks.size() : 0; }
int iso_cdda_track_count(void *handle) { return iso_track_count(handle); }

uint32_t iso_cdda_track_start_lba(void *handle, int track) {
    Disc *d = static_cast<Disc*>(handle);
    if (!d || track < 1 || (size_t)track > d->tracks.size()) return 0;
    return d->tracks[(size_t)track - 1].start_lba;
}

uint32_t iso_cdda_track_pregap_lba(void *handle, int track) {
    Disc *d = static_cast<Disc*>(handle);
    if (!d || track < 1 || (size_t)track > d->tracks.size()) return 0;
    return d->tracks[(size_t)track - 1].pregap_lba;
}

int iso_cdda_track_is_audio(void *handle, int track) {
    Disc *d = static_cast<Disc*>(handle);
    if (!d || track < 1 || (size_t)track > d->tracks.size()) return 0;
    return d->tracks[(size_t)track - 1].audio ? 1 : 0;
}

int iso_read_cdda_sector(void *handle, uint32_t lba, uint8_t *buffer, int size) {
    return iso_read_raw_sector(handle, lba, buffer, size);
}

void iso_close(void *handle) { delete static_cast<Disc*>(handle); }

} /* extern "C" */
