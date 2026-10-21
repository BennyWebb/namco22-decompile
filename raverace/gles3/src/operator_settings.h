#ifndef RR_OPERATOR_SETTINGS_H
#define RR_OPERATOR_SETTINGS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#define RR_OPERATOR_EEPROM_SIZE 8192
#define RR_OPERATOR_RECORDS 116

typedef enum {
    RR_OPERATOR_OK,
    RR_OPERATOR_SIZE,
    RR_OPERATOR_NO_HEADER,
    RR_OPERATOR_RECORD_MARKER,
    RR_OPERATOR_RECORD_CHECKSUM,
    RR_OPERATOR_RANGE,
    RR_OPERATOR_EXTRA_MODE,
    RR_OPERATOR_READ_ONLY,
    RR_OPERATOR_STALE_SOURCE,
    RR_OPERATOR_IO
} rr_operator_code;

typedef struct {
    rr_operator_code code;
    size_t offset; /* File offset, or SIZE_MAX when not applicable. */
    int system_error;
} rr_operator_error;

/* Only these fields have a setter. Boolean values use the game's storage:
 * units: 0=km/h, 1=mph; attract: 0=on, 1=off; free play: 0=off, 1=on. */
typedef enum {
    RR_OPERATOR_CAR,
    RR_OPERATOR_SOLO_DIFFICULTY,
    RR_OPERATOR_TIME_TRIAL_DIFFICULTY,
    RR_OPERATOR_RIGHT_VOLUME,
    RR_OPERATOR_LEFT_VOLUME,
    RR_OPERATOR_SPEED_UNITS,
    RR_OPERATOR_ATTRACT_SOUND,
    RR_OPERATOR_FREE_PLAY,
    RR_OPERATOR_FIELD_COUNT
} rr_operator_field;

typedef struct {
    uint16_t calibration[3];
    uint8_t game_cost, coin_value[2], free_play;
    uint8_t volume[2], volume_duplicate[2]; /* Right, left. */
    uint8_t cabinet, car, difficulty[3], laps[8]; /* Linked, solo, time trial. */
    uint8_t speed_units, attract_sound, extra_laps, easy_setting, feedback, extra_mode, ads;
} rr_operator_values;

typedef struct {
    uint8_t bytes[RR_OPERATOR_EEPROM_SIZE];
    size_t header, record[RR_OPERATOR_RECORDS];
    rr_operator_values values;
} rr_operator_image;

typedef struct {
    rr_operator_image image;
    struct stat identity;
} rr_operator_file;

const char *rr_operator_error_name(rr_operator_code code);
int rr_operator_decode(const void *bytes, size_t size, rr_operator_image *out, rr_operator_error *error);
/* On failure, out is unchanged. Source and out may alias. No repair/defaults. */
int rr_operator_edit(const rr_operator_image *source,
                     rr_operator_field field,
                     int value,
                     rr_operator_image *out,
                     rr_operator_error *error);
int rr_operator_load(const char *path, rr_operator_file *out, rr_operator_error *error);
/* Caller holds the launcher's exclusive session lock throughout load/edit/commit.
 * Checks inode/timestamps AND contents before rename. Not a multi-writer CAS.
 * After rename, a directory-fsync/readback failure returns IO; the new image may
 * already be installed. No rollback, automatic retry or temporary-file deletion.
 * A no-op still checks source identity, but does not replace the file. */
int rr_operator_commit(const char *path,
                       const rr_operator_file *source,
                       const rr_operator_image *updated,
                       rr_operator_error *error);

#endif
