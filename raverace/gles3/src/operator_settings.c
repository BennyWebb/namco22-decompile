/* World revision B operator EEPROM: ROM 2ABCC/2AD26/2AE7E/2AEB4.
 * All offsets are EEPROM file offsets; no live WRAM or rr_hw setters are used. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "operator_settings.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(rr_operator_error *error, rr_operator_code code, size_t offset)
{
    if (error)
        *error = (rr_operator_error){code, offset, code == RR_OPERATOR_IO ? errno : 0};
    return 0;
}

static int success(rr_operator_error *error)
{
    if (error)
        *error = (rr_operator_error){RR_OPERATOR_OK, SIZE_MAX, 0};
    return 1;
}

const char *rr_operator_error_name(rr_operator_code code)
{
    static const char *const names[] = {"OK", "SIZE", "NO_HEADER", "RECORD_MARKER", "RECORD_CHECKSUM", "RANGE",
                                       "EXTRA_MODE", "READ_ONLY", "STALE_SOURCE", "IO"};
    return (unsigned)code < sizeof names / sizeof *names ? names[code] : "UNKNOWN_ERROR";
}

static unsigned be16(const uint8_t *p)
{
    return p[0] * 256u + p[1];
}

static unsigned checksum(const uint8_t *p, size_t count)
{
    unsigned sum = 0;
    for (size_t i = 0; i < count; i++)
        sum += p[i];
    return (~sum) & 65535u;
}

static int marker(const uint8_t *p)
{
    return memcmp(p, "V154", 4) == 0;
}

int rr_operator_decode(const void *bytes, size_t size, rr_operator_image *out, rr_operator_error *error)
{
    if (size != RR_OPERATOR_EEPROM_SIZE)
        return fail(error, RR_OPERATOR_SIZE, SIZE_MAX);
    rr_operator_image image = {0};
    memcpy(image.bytes, bytes, size);
    image.header = SIZE_MAX;
    /* ROM 2ABCC: ordered scan, not a newest-header/generation heuristic. */
    for (size_t h = 0; h < 512; h += 64)
        if (marker(image.bytes + h + 58) && checksum(image.bytes + h, 62) == be16(image.bytes + h + 62)) {
            image.header = h;
            break;
        }
    if (image.header == SIZE_MAX)
        return fail(error, RR_OPERATOR_NO_HEADER, SIZE_MAX);
    for (size_t g = 0; g < RR_OPERATOR_RECORDS; g++) {
        unsigned selector = image.bytes[image.header + g / 2];
        unsigned bank = (selector >> (g % 2 ? 0 : 4)) & 1;
        size_t offset = 512 + 64 * g + 32 * bank;
        image.record[g] = offset;
        if (!marker(image.bytes + offset + 26))
            return fail(error, RR_OPERATOR_RECORD_MARKER, offset + 26);
        if (checksum(image.bytes + offset, 30) != be16(image.bytes + offset + 30))
            return fail(error, RR_OPERATOR_RECORD_CHECKSUM, offset + 30);
    }
    rr_operator_values *v = &image.values;
    const uint8_t *cal = image.bytes + image.record[0];
    const uint8_t *coin = image.bytes + image.record[2];
    const uint8_t *game = image.bytes + image.record[3];
    for (unsigned i = 0; i < 3; i++) {
        v->calibration[i] = be16(cal + 2 * i);
        if (v->calibration[i] > 4095)
            return fail(error, RR_OPERATOR_RANGE, image.record[0] + 2 * i);
    }
    /* TEST range tables 2E262, 2E6E8, 2D6AC, 2DA76, 2F5C6, 2C4FA.
     * Opaque/unexposed payload bytes are preserved, not assigned guessed ranges. */
    for (unsigned i = 0; i < 4; i++)
        if (coin[i] < (i == 3 ? 0 : 1) || coin[i] > (i == 3 ? 1 : 9))
            return fail(error, RR_OPERATOR_RANGE, image.record[2] + i);
    for (unsigned i = 11; i <= 14; i++)
        if (coin[i] > 63)
            return fail(error, RR_OPERATOR_RANGE, image.record[2] + i);
    for (unsigned i = 11; i <= 12; i++)
        if (coin[i] != coin[i + 2])
            return fail(error, RR_OPERATOR_RANGE, image.record[2] + i + 2);
    for (unsigned i = 0; i <= 1; i++)
        if (game[i] > 7)
            return fail(error, RR_OPERATOR_RANGE, image.record[3] + i);
    for (unsigned i = 13; i <= 18; i++)
        if (game[i] > 1)
            return fail(error, RR_OPERATOR_RANGE, image.record[3] + i);
    /* Extra mode presets are meaningful readable state, but never editable v1. */
    for (unsigned i = 2; i <= 12; i++) {
        unsigned low = i < 5 ? 1 : 0, high = i < 5 ? 8 : 3;
        if (game[18] == 0)
            low = high = i < 5 ? 9 : 4;
        if (game[i] < low || game[i] > high)
            return fail(error, RR_OPERATOR_RANGE, image.record[3] + i);
    }
    unsigned ads = image.bytes[image.record[4]];
    if (ads > 1)
        return fail(error, RR_OPERATOR_RANGE, image.record[4]);
    v->game_cost = coin[0];
    memcpy(v->coin_value, coin + 1, 2);
    v->free_play = coin[3];
    memcpy(v->volume, coin + 11, 2);
    memcpy(v->volume_duplicate, coin + 13, 2);
    v->cabinet = game[0];
    v->car = game[1];
    memcpy(v->difficulty, game + 2, 3);
    memcpy(v->laps, game + 5, 8);
    v->speed_units = game[13];
    v->attract_sound = game[14];
    v->extra_laps = game[15];
    v->easy_setting = game[16];
    v->feedback = game[17];
    v->extra_mode = game[18];
    v->ads = ads;
    *out = image;
    return success(error);
}

typedef struct {
    unsigned group, byte, duplicate, low, high;
} field_spec;

static const field_spec fields[RR_OPERATOR_FIELD_COUNT] = {
    {3, 1, 1, 0, 7}, {3, 3, 3, 1, 8}, {3, 4, 4, 1, 8}, {2, 11, 13, 0, 63},
    {2, 12, 14, 0, 63}, {3, 13, 13, 0, 1}, {3, 14, 14, 0, 1}, {2, 3, 3, 0, 1}};

int rr_operator_edit(const rr_operator_image *source,
                     rr_operator_field field,
                     int value,
                     rr_operator_image *out,
                     rr_operator_error *error)
{
    rr_operator_image image;
    if (!rr_operator_decode(source->bytes, sizeof source->bytes, &image, error))
        return 0;
    if ((unsigned)field >= RR_OPERATOR_FIELD_COUNT)
        return fail(error, RR_OPERATOR_READ_ONLY, SIZE_MAX);
    if (image.values.extra_mode == 0)
        return fail(error, RR_OPERATOR_EXTRA_MODE, image.record[3] + 18);
    const field_spec *f = fields + field;
    size_t offset = image.record[f->group];
    if (value < (int)f->low || value > (int)f->high)
        return fail(error, RR_OPERATOR_RANGE, offset + f->byte);
    image.bytes[offset + f->byte] = (uint8_t)value;
    image.bytes[offset + f->duplicate] = (uint8_t)value;
    unsigned sum = checksum(image.bytes + offset, 30);
    image.bytes[offset + 30] = (uint8_t)(sum >> 8);
    image.bytes[offset + 31] = (uint8_t)sum;
    return rr_operator_decode(image.bytes, sizeof image.bytes, out, error);
}

static int same_identity(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_size == b->st_size &&
           a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static int load_at(int directory, const char *path, rr_operator_file *out, rr_operator_error *error)
{
    int fd = openat(directory, path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return fail(error, RR_OPERATOR_IO, SIZE_MAX);
    rr_operator_file file;
    struct stat after;
    uint8_t bytes[RR_OPERATOR_EEPROM_SIZE];
    if (fstat(fd, &file.identity)) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return fail(error, RR_OPERATOR_IO, SIZE_MAX);
    }
    if (!S_ISREG(file.identity.st_mode) || file.identity.st_size != sizeof bytes) {
        close(fd);
        return fail(error, RR_OPERATOR_SIZE, SIZE_MAX);
    }
    int ok = 1;
    size_t used = 0;
    while (ok && used < sizeof bytes) {
        ssize_t n = read(fd, bytes + used, sizeof bytes - used);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0)
                errno = EIO;
            ok = 0;
        } else
            used += (size_t)n;
    }
    int saved_errno = errno;
    if (fstat(fd, &after)) {
        ok = 0;
        saved_errno = errno;
    } else if (!same_identity(&file.identity, &after)) {
        close(fd);
        return fail(error, RR_OPERATOR_STALE_SOURCE, SIZE_MAX);
    }
    if (close(fd) && ok) {
        ok = 0;
        saved_errno = errno;
    }
    if (!ok) {
        errno = saved_errno;
        return fail(error, RR_OPERATOR_IO, SIZE_MAX);
    }
    if (!rr_operator_decode(bytes, sizeof bytes, &file.image, error))
        return 0;
    *out = file;
    return success(error);
}

int rr_operator_load(const char *path, rr_operator_file *out, rr_operator_error *error)
{
    return load_at(AT_FDCWD, path, out, error);
}

static int unchanged(int directory, const char *name, const rr_operator_file *source, rr_operator_error *error)
{
    struct stat identity;
    if (fstatat(directory, name, &identity, AT_SYMLINK_NOFOLLOW))
        return fail(error, errno == ENOENT ? RR_OPERATOR_STALE_SOURCE : RR_OPERATOR_IO, SIZE_MAX);
    if (!same_identity(&source->identity, &identity))
        return fail(error, RR_OPERATOR_STALE_SOURCE, SIZE_MAX);
    rr_operator_file current;
    if (!load_at(directory, name, &current, error))
        return 0;
    if (!same_identity(&source->identity, &current.identity) ||
        memcmp(source->image.bytes, current.image.bytes, RR_OPERATOR_EEPROM_SIZE))
        return fail(error, RR_OPERATOR_STALE_SOURCE, SIZE_MAX);
    return 1;
}

int rr_operator_commit(const char *path,
                       const rr_operator_file *source,
                       const rr_operator_image *updated,
                       rr_operator_error *error)
{
    rr_operator_image allowed, verified;
    if (!rr_operator_decode(source->image.bytes, RR_OPERATOR_EEPROM_SIZE, &allowed, error) ||
        !rr_operator_decode(updated->bytes, RR_OPERATOR_EEPROM_SIZE, &verified, error))
        return 0;
    /* Reconstruct allowed edits; direct mutation of read-only bytes is rejected. */
    for (unsigned i = 0; i < RR_OPERATOR_FIELD_COUNT; i++) {
        const field_spec *f = fields + i;
        size_t a = allowed.record[f->group], b = verified.record[f->group];
        if (allowed.bytes[a + f->byte] != verified.bytes[b + f->byte] ||
            allowed.bytes[a + f->duplicate] != verified.bytes[b + f->duplicate])
            if (!rr_operator_edit(&allowed, (rr_operator_field)i, verified.bytes[b + f->byte], &allowed, error))
                return 0;
    }
    if (memcmp(allowed.bytes, verified.bytes, RR_OPERATOR_EEPROM_SIZE))
        return fail(error, RR_OPERATOR_READ_ONLY, SIZE_MAX);
    char parent[PATH_MAX], name[NAME_MAX + 1], temporary[NAME_MAX + 1];
    const char *slash = strrchr(path, '/');
    size_t parent_size = slash ? (size_t)(slash - path) : 1;
    const char *base = slash ? slash + 1 : path;
    if (parent_size >= sizeof parent || strlen(base) + sizeof ".operator-new" > sizeof temporary || !*base) {
        errno = ENAMETOOLONG;
        return fail(error, RR_OPERATOR_IO, SIZE_MAX);
    }
    if (!slash)
        strcpy(parent, ".");
    else if (!parent_size)
        strcpy(parent, "/");
    else {
        memcpy(parent, path, parent_size);
        parent[parent_size] = 0;
    }
    strcpy(name, base);
    snprintf(temporary, sizeof temporary, "%s.operator-new", base);
    int directory = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0)
        return fail(error, RR_OPERATOR_IO, SIZE_MAX);
    int ok = unchanged(directory, name, source, error);
    if (ok && memcmp(source->image.bytes, verified.bytes, RR_OPERATOR_EEPROM_SIZE)) {
        int fd = openat(directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0)
            ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
        else {
            size_t used = 0;
            while (ok && used < RR_OPERATOR_EEPROM_SIZE) {
                ssize_t n = write(fd, verified.bytes + used, RR_OPERATOR_EEPROM_SIZE - used);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    if (n == 0)
                        errno = EIO;
                    ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
                } else
                    used += (size_t)n;
            }
            if (ok && fsync(fd))
                ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
            if (close(fd) && ok)
                ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
            if (ok)
                ok = unchanged(directory, name, source, error);
            if (ok && renameat(directory, temporary, directory, name))
                ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
            if (ok && fsync(directory))
                ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
            rr_operator_file readback;
            if (ok)
                ok = load_at(directory, name, &readback, error);
            if (ok && memcmp(readback.image.bytes, verified.bytes, RR_OPERATOR_EEPROM_SIZE)) {
                errno = EIO;
                ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
            }
        }
    }
    if (close(directory) && ok)
        ok = fail(error, RR_OPERATOR_IO, SIZE_MAX);
    return ok ? success(error) : 0;
}
