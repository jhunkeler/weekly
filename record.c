#include "weekly.h"

void record_free(struct Record *record) {
    if (record != NULL) {
        free(record->date);
        free(record->time);
        free(record->host);
        free(record->user);
        free(record->data);
        free(record);
    }
}

struct Record *record_parse(const char *content) {
    char *next;
    struct Record *result;

    result = calloc(1, sizeof(*result));
    if (!result) {
        perror("Unable to allocate record");
        return NULL;
    }

    char *p = strdup(content);
    next = strtok(p, "\n");
    while (next != NULL) {
        char key[10] = {0};
        char value[255] = {0};

        sscanf(next, "## %9[^: ]:%254s[^\n]", key, value);
        if (strncmp(next, "## ", 2) != 0) {
            break;
        }
        if (!strcmp(key, "date"))
            result->date = strdup(value);
        else if (!strcmp(key, "time"))
            result->time = strdup(value);
        else if (!strcmp(key, "author"))
            result->user = strdup(value);
        else if (!strcmp(key, "host"))
            result->host = strdup(value);

        next = strtok(NULL, "\n");
    }

    if (next != NULL) {
        if (memcmp(next, "\x02\x02\x02", 3) == 0) {
            next += 4;
        }
        result->data = strdup(next);
    } else {
        // Empty data record, die
        record_free(result);
        result = NULL;
    }

    free(p);
    return result;
}

struct Record *record_read(FILE **fp) {
    ssize_t soh, sot, eot;
    size_t record_size;
    char task[2] = {0};
    char *buf = calloc(BUFSIZ, sizeof(char));

    if (!buf) {
        return NULL;
    }

    if (!*fp) {
        free(buf);
        return NULL;
    }

    // Start of header offset
    soh = 0;
    // Start of text offset
    sot = 0;
    // End of text offset
    eot = 0;

    while (fread(task, sizeof(char), 1, *fp) > 0) {
        if (task[0] == '\x01' && fread(task, sizeof(char), 2, *fp) > 0) {
            // start header
            if (memcmp(task, "\x01\x01", 2) == 0) {
                soh = ftell(*fp);
            }
        } else if (task[0] == '\x02' && fread(task, sizeof(char), 2, *fp) > 0) {
            if (memcmp(task, "\x02\x02", 2) == 0) {
                sot = ftell(*fp);
            }
            // start of text
        } else if (task[0] == '\x03' && fread(task, sizeof(char), 2, *fp) > 0) {
            if (memcmp(task, "\x03\x03", 2) == 0) {
                eot = ftell(*fp);
                break;
            }
        } else {
            continue;
        }
        memset(task, '\0', sizeof(task));
    }

    // Verify the record is not too small, and contained a start of text marker
    record_size = eot - soh;
    if (record_size < 1 && !sot) {
        free(buf);
        return NULL;
    }

    // Go back to start of header
    fseek(*fp, soh, SEEK_SET);
    // Read the entire record
    fread(buf, sizeof(char), record_size, *fp);
    // Remove end of text marker
    memset(buf + (record_size - 4), '\0', 4);
    // Truncate buffer at end of line
    buf[strlen(buf) - 1] = '\0';

    // Emit record
    struct Record *result;
    result = record_parse(buf);

    free(buf);

    return result;
}

void record_show(struct Record *record, int style) {
    const char *fmt;
    switch (style) {
        case RECORD_STYLE_LONG:
            fmt = "## Date: %s\n## Time: %s\n## User: %s\n## Host: %s\n%s\n";
            break;
        case RECORD_STYLE_CSV:
            fmt = "%s,%s,%s,%s,\"%s\"";  // Trailing linefeed omitted for visual clarity
            break;
        case RECORD_STYLE_DICT:
            fmt = "{"
                  "\"date\": \"%s\",\n"
                  "\"time\": \"%s\",\n"
                  "\"user\": \"%s\",\n"
                  "\"host\": \"%s\",\n"
                  "\"data\": \"%s\"}\n";
            break;
        case RECORD_STYLE_SHORT:
        default:
            fmt = "%s - %s - %s (%s):\n%s\n";
            break;
    }
    printf(fmt, record->date, record->time, record->user, record->host, record->data);
}
