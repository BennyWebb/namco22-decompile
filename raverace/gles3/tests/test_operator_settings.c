/* Host driver: generated EEPROM files only; no embedded seed/payload. */
#include "operator_settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int report(int ok, const rr_operator_error *error)
{
    printf("%s offset=%zu errno=%d\n", rr_operator_error_name(error->code), error->offset, error->system_error);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: test_operator_settings check FILE | edit FILE FIELD VALUE | stale FILE FIELD VALUE\n");
        return 2;
    }
    rr_operator_file source;
    rr_operator_error error;
    int ok = rr_operator_load(argv[2], &source, &error);
    if (!ok || !strcmp(argv[1], "check"))
        return report(ok, &error);
    if (!strcmp(argv[1], "api")) {
        rr_operator_image out, untouched;
        memset(&out, 0xA5, sizeof out);
        untouched = out;
        assert(!rr_operator_edit(&source.image, RR_OPERATOR_FIELD_COUNT, 0, &out, &error));
        assert(error.code == RR_OPERATOR_READ_ONLY && !memcmp(&out, &untouched, sizeof out));
        assert(!rr_operator_edit(&source.image, RR_OPERATOR_CAR, 8, &out, &error));
        assert(error.code == RR_OPERATOR_RANGE && !memcmp(&out, &untouched, sizeof out));
        assert(!rr_operator_decode(source.image.bytes, 8191, &out, &error));
        assert(error.code == RR_OPERATOR_SIZE && !memcmp(&out, &untouched, sizeof out));
        out = source.image;
        assert(rr_operator_edit(&out, RR_OPERATOR_CAR, 7, &out, &error));
        assert(out.values.car == 7 && source.image.values.car == 0);
        return report(1, &error);
    }
    if (argc != 5)
        return 2;
    char *end;
    if (!strcmp(argv[1], "readonly")) {
        long offset = strtol(argv[3], &end, 10);
        if (*end || offset < 512 || offset >= 8192)
            return 2;
        long value = strtol(argv[4], &end, 10);
        if (*end || value < 0 || value > 255)
            return 2;
        rr_operator_image updated = source.image;
        updated.bytes[offset] = (uint8_t)value;
        size_t record = 512 + ((offset - 512) / 32) * 32;
        unsigned sum = 0;
        for (size_t i = 0; i < 30; i++)
            sum += updated.bytes[record + i];
        sum = (~sum) & 65535u;
        updated.bytes[record + 30] = (uint8_t)(sum >> 8);
        updated.bytes[record + 31] = (uint8_t)sum;
        return report(rr_operator_commit(argv[2], &source, &updated, &error), &error);
    }
    long field = strtol(argv[3], &end, 10);
    if (*end || field < -1 || field > RR_OPERATOR_FIELD_COUNT)
        return 2;
    long value = strtol(argv[4], &end, 10);
    if (*end || value < -1 || value > 65536)
        return 2;
    rr_operator_image updated;
    ok = rr_operator_edit(&source.image, (rr_operator_field)field, (int)value, &updated, &error);
    if (!ok)
        return report(ok, &error);
    if (!strcmp(argv[1], "stale")) {
        /* Valid independent edit after load, to exercise snapshot/identity CAS. */
        rr_operator_image other;
        assert(rr_operator_edit(&source.image, RR_OPERATOR_CAR, source.image.values.car ^ 1, &other, &error));
        FILE *f = fopen(argv[2], "wb");
        assert(f && fwrite(other.bytes, 1, sizeof other.bytes, f) == sizeof other.bytes);
        assert(!fclose(f));
    } else if (!strcmp(argv[1], "stale-token"))
        source.identity.st_ino ^= 1;
    else if (strcmp(argv[1], "edit"))
        return 2;
    ok = rr_operator_commit(argv[2], &source, &updated, &error);
    return report(ok, &error);
}
