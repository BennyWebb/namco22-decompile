#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "title_input.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
const char *const ti_labels[TI_ACTIONS] = {"Accelerate primary",
                                           "Accelerate secondary",
                                           "Brake primary",
                                           "Brake secondary",
                                           "Shift up",
                                           "Shift down",
                                           "View",
                                           "Overlay",
                                           "Steering left"};
/* On the builtin device SDL raw 3 is physical A (ES a), raw 4 is B (ES b). */
static const char *const actions[TI_ACTIONS] = {"b", "r2", "a", "l2", "pagedown", "pageup", "y", "r3", "joystick1left"};
static const char *const required[] = {"a",
                                       "b",
                                       "down",
                                       "hotkey",
                                       "joystick1left",
                                       "joystick1up",
                                       "joystick2left",
                                       "joystick2up",
                                       "l2",
                                       "l3",
                                       "left",
                                       "pagedown",
                                       "pageup",
                                       "r2",
                                       "r3",
                                       "right",
                                       "select",
                                       "start",
                                       "up",
                                       "x",
                                       "y"};
static bool bad(char *e, size_t n, const char *name)
{
    snprintf(e, n, "%s", name);
    return false;
}
static bool reserved(int id)
{
    return id == 6 || id == 9 || id == 10;
}
/* Deliberately a strict fragment grammar, not a permissive XML search. */
static bool attrs(const char *line, const char *tag, char keys[][32], char vals[][128], unsigned *count)
{
    const char *p = line;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, tag, strlen(tag)))
        return false;
    p += strlen(tag);
    if (*p != ' ' && *p != '\t')
        return false;
    *count = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    while (*p && *p != '/' && *p != '>') {
        if (*count == 8)
            return false;
        unsigned k = 0, v = 0;
        while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
            if (k == 31)
                return false;
            keys[*count][k++] = *p++;
        }
        keys[*count][k] = 0;
        if (!k || *p++ != '=' || *p++ != '"')
            return false;
        while (*p && *p != '"') {
            if ((unsigned char)*p < 32 || *p == '<' || *p == '>' || *p == '&' || v == 127)
                return false;
            vals[*count][v++] = *p++;
        }
        vals[*count][v] = 0;
        if (*p++ != '"')
            return false;
        for (unsigned i = 0; i < *count; i++)
            if (!strcmp(keys[i], keys[*count]))
                return false;
        (*count)++;
        if (*p != ' ' && *p != '\t' && *p != '/' && *p != '>')
            return false;
        while (*p == ' ' || *p == '\t')
            p++;
    }
    if (!strcmp(tag, "<input")) {
        if (*p++ != '/')
            return false;
    }
    if (*p++ != '>')
        return false;
    while (*p == ' ' || *p == '\t')
        p++;
    return !*p;
}
static const char *attr(char keys[][32], char vals[][128], unsigned n, const char *key)
{
    for (unsigned i = 0; i < n; i++)
        if (!strcmp(keys[i], key))
            return vals[i];
    return NULL;
}
static bool number(const char *p, int *out)
{
    if (!p || !*p || (*p == '-' && !p[1]))
        return false;
    const char *q = p + (*p == '-');
    for (; *q; q++)
        if (*q < '0' || *q > '9')
            return false;
    char *end;
    errno = 0;
    long v = strtol(p, &end, 10);
    if (errno || *end || v < -1 || v > 65535)
        return false;
    *out = (int)v;
    return true;
}
static bool binding(const ti_row *r, bool steering)
{
    if (!strcmp(r->type, "axis"))
        return steering && r->id >= 0 && r->id < 4 && abs(r->value) == 1;
    if (steering)
        return false;
    if (!strcmp(r->type, "button"))
        return r->id >= 0 && r->id < 16 && r->value == 1 && !reserved(r->id);
    return !strcmp(r->type, "hat") && r->id == 0 && (r->value == 1 || r->value == 2 || r->value == 4 || r->value == 8);
}
static bool validate(ti_profile *p, char *e, size_t n)
{
    for (unsigned i = 0; i < TI_ACTIONS; i++) {
        p->action[i] = -1;
        for (unsigned j = 0; j < p->count; j++)
            if (!strcmp(p->rows[j].name, actions[i]))
                p->action[i] = (int)j;
        if (p->action[i] < 0 || !binding(&p->rows[p->action[i]], i == TI_STEER))
            return bad(e, n, "BINDING");
        for (unsigned j = 0; j < i; j++) {
            ti_row *a = &p->rows[p->action[i]], *b = &p->rows[p->action[j]];
            if (!strcmp(a->type, b->type) && a->id == b->id && a->value == b->value)
                return bad(e, n, "DUPLICATE");
        }
    }
    return true;
}
bool ti_parse(const char *text, ti_profile *out, char *e, size_t n)
{
    size_t length = strnlen(text, TITLE_CONFIG_SIZE);
    if (!length || length >= TITLE_CONFIG_SIZE)
        return bad(e, n, "CONFIG_SIZE");
    ti_profile p = {0};
    memcpy(p.text, text, length + 1);
    bool root = false, closed = false;
    size_t offset = 0;
    while (offset < length) {
        const char *end = strchr(text + offset, '\n');
        size_t len = end ? (size_t)(end - text - offset) : length - offset;
        if (len >= 1024)
            return bad(e, n, "MALFORMED");
        char line[1024];
        memcpy(line, text + offset, len);
        line[len] = 0;
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s) {
            if (closed)
                return bad(e, n, "MALFORMED");
            if (!strcmp(s, "</inputConfig>")) {
                if (!root)
                    return bad(e, n, "MALFORMED");
                closed = true;
            } else {
                char keys[8][32], vals[8][128];
                unsigned count;
                if (!root) {
                    if (!attrs(line, "<inputConfig", keys, vals, &count) || count != 4)
                        return bad(e, n, "MALFORMED");
                    const char *type = attr(keys, vals, count, "type"), *guid = attr(keys, vals, count, "deviceGUID"),
                               *name = attr(keys, vals, count, "deviceName"),
                               *internal = attr(keys, vals, count, "internal");
                    if (!type || strcmp(type, "joystick") || !internal || strcmp(internal, "1"))
                        return bad(e, n, "MALFORMED");
                    if (!guid || strcmp(guid, TITLE_GUID) || !name || strcmp(name, TITLE_DEVICE))
                        return bad(e, n, "DEVICE_IDENTITY");
                    strcpy(p.guid, guid);
                    strcpy(p.name, name);
                    root = true;
                } else {
                    if (p.count == TITLE_ROWS || !attrs(line, "<input", keys, vals, &count) ||
                        (count != 4 && count != 5))
                        return bad(e, n, "MALFORMED");
                    ti_row *r = &p.rows[p.count];
                    const char *name = attr(keys, vals, count, "name"), *type = attr(keys, vals, count, "type");
                    int code;
                    if (!name || strlen(name) >= sizeof r->name || !type || strlen(type) >= sizeof r->type ||
                        !number(attr(keys, vals, count, "id"), &r->id) ||
                        !number(attr(keys, vals, count, "value"), &r->value) ||
                        (count == 5 && (!number(attr(keys, vals, count, "code"), &code) || code < 0)))
                        return bad(e, n, "MALFORMED");
                    bool known = false;
                    for (unsigned i = 0; i < sizeof required / sizeof *required; i++)
                        if (!strcmp(name, required[i]))
                            known = true;
                    if (!known)
                        return bad(e, n, "UNKNOWN_ROW");
                    strcpy(r->name, name);
                    strcpy(r->type, type);
                    r->begin = offset;
                    r->end = offset + len;
                    for (unsigned i = 0; i < p.count; i++)
                        if (!strcmp(name, p.rows[i].name))
                            return bad(e, n, "DUPLICATE_ROW");
                    bool ok = (!strcmp(type, "button") && r->id >= 0 && r->id < 16 && r->value == 1) ||
                              (!strcmp(type, "axis") && r->id >= 0 && r->id < 4 && abs(r->value) == 1) ||
                              (!strcmp(type, "hat") && r->id == 0 &&
                               (r->value == 1 || r->value == 2 || r->value == 4 || r->value == 8));
                    if (!ok)
                        return bad(e, n, "BINDING");
                    if ((!strcmp(name, "start") && (strcmp(type, "button") || r->id != 10)) ||
                        (!strcmp(name, "select") && (strcmp(type, "button") || r->id != 9)) ||
                        (!strcmp(name, "x") && (strcmp(type, "button") || r->id != 6)))
                        return bad(e, n, "RESERVED");
                    const char *dirs[] = {"up", "right", "down", "left"};
                    const int masks[] = {1, 2, 4, 8};
                    for (unsigned i = 0; i < 4; i++)
                        if (!strcmp(name, dirs[i]) && (strcmp(type, "hat") || r->id != 0 || r->value != masks[i]))
                            return bad(e, n, "FIXED_MENU");
                    p.count++;
                }
            }
        }
        offset += len + (end != NULL);
    }
    if (!root || !closed || p.count != sizeof required / sizeof *required || !validate(&p, e, n))
        return bad(e, n, "MALFORMED_BINDING");
    *out = p;
    return true;
}
bool ti_load(const char *path, ti_profile *out, char *e, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return bad(e, n, "IO_OPEN");
    char text[TITLE_CONFIG_SIZE];
    size_t size = fread(text, 1, sizeof text, f);
    bool ok = !ferror(f);
    if (fclose(f))
        ok = false;
    if (!ok)
        return bad(e, n, "IO_READ");
    if (size == sizeof text || memchr(text, 0, size))
        return bad(e, n, "CONFIG_SIZE");
    text[size] = 0;
    return ti_parse(text, out, e, n);
}
bool ti_bind(ti_profile *p, ti_action action, const char *type, int id, int value, char *e, size_t n)
{
    if ((unsigned)action >= TI_ACTIONS || strlen(type) >= 12)
        return bad(e, n, "BINDING");
    if (!strcmp(type, "button") && reserved(id))
        return bad(e, n, "RESERVED");
    ti_profile copy = *p;
    ti_row *r = &copy.rows[copy.action[action]];
    strcpy(r->type, type);
    r->id = id;
    r->value = value;
    if (!validate(&copy, e, n))
        return false;
    /* Rewrite only this row; root and unused rows stay byte-identical. */
    char text[TITLE_CONFIG_SIZE];
    int len = snprintf(text,
                       sizeof text,
                       "%.*s        <input name=\"%s\" type=\"%s\" id=\"%d\" value=\"%d\" />%s",
                       (int)r->begin,
                       p->text,
                       r->name,
                       r->type,
                       r->id,
                       r->value,
                       p->text + r->end);
    if (len < 0 || (size_t)len >= sizeof text)
        return bad(e, n, "CONFIG_SIZE");
    return ti_parse(text, p, e, n);
}
bool ti_write(const char *path, const ti_profile *p, char *e, size_t n)
{
    ti_profile check;
    if (!ti_parse(p->text, &check, e, n))
        return false;
    char temp[4096], parent[4096];
    if (snprintf(temp, sizeof temp, "%s.controller-new", path) >= (int)sizeof temp || strlen(path) >= sizeof parent)
        return bad(e, n, "PATH_SIZE");
    strcpy(parent, path);
    char *slash = strrchr(parent, '/');
    if (slash)
        *slash = 0;
    else
        strcpy(parent, ".");
    int fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
        return bad(e, n, "IO_CREATE");
    size_t at = 0, len = strlen(p->text);
    bool ok = true;
    while (at < len) {
        ssize_t w = write(fd, p->text + at, len - at);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0) {
            ok = false;
            break;
        }
        at += (size_t)w;
    }
    if (ok && fsync(fd))
        ok = false;
    if (close(fd))
        ok = false;
    if (!ok)
        return bad(e, n, "IO_WRITE");
    if (rename(temp, path))
        return bad(e, n, "IO_RENAME");
    int dir = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0)
        return bad(e, n, "IO_DIRECTORY");
    ok = fsync(dir) == 0;
    if (close(dir))
        ok = false;
    if (!ok || !ti_load(path, &check, e, n) || strcmp(check.text, p->text))
        return bad(e, n, "IO_READBACK");
    return true;
}
static bool pressed(const ti_row *r, const ti_sample *s)
{
    if (!strcmp(r->type, "button"))
        return (s->buttons & (1u << r->id)) != 0;
    return (s->hat & r->value) == r->value;
}
void ti_poll(const ti_profile *p, const ti_sample *s, ti_controls *o)
{
    *o = (ti_controls){0};
    bool a[TI_ACTIONS];
    for (unsigned i = 0; i < TI_STEER; i++)
        a[i] = pressed(&p->rows[p->action[i]], s);
    o->gas = a[TI_GAS] || a[TI_GAS2];
    o->brake = a[TI_BRAKE] || a[TI_BRAKE2];
    o->up = a[TI_UP];
    o->down = a[TI_DOWN];
    o->view = a[TI_VIEW];
    o->overlay = a[TI_OVERLAY];
    o->coin = (s->buttons & (1u << 9)) != 0;
    o->start = (s->buttons & (1u << 10)) != 0;
    o->exit = o->coin && o->start;
    o->x = (s->buttons & (1u << 6)) != 0;
    const ti_row *r = &p->rows[p->action[TI_STEER]];
    int axis = s->axes[r->id];
    if (axis < -32767)
        axis = -32767;
    axis *= -r->value;
    o->wheel = abs(axis) <= 4096 ? 0 : (axis > 0 ? axis - 4096 : axis + 4096) * 32767 / (32767 - 4096);
    if (s->hat & 2)
        o->wheel = 32767;
    if (s->hat & 8)
        o->wheel = -32767;
    if (o->exit)
        *o = (ti_controls){.exit = true};
}
bool ti_hold_update(ti_hold *h, bool x, bool in_game, uint64_t now)
{
    if (!in_game || !x) {
        *h = (ti_hold){0};
        return false;
    }
    if (!h->held) {
        h->held = true;
        h->since = now;
    }
    if (!h->fired && now - h->since >= 1000) {
        h->fired = true;
        return true;
    }
    return false;
}
bool ti_released(const ti_sample *s)
{
    if (s->buttons || s->hat)
        return false;
    for (unsigned i = 0; i < 4; i++)
        if (abs(s->axes[i]) > 4096)
            return false;
    return true;
}
