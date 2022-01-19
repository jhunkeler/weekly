#include "weekly.h"

int dump_file(const char *filename, int style) {
    FILE *fp;
    fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }
    struct Record *record;
    while ((record = record_read(&fp)) != NULL) {
        record_show(record, style);
        record_free(record);
        puts("");
    }
    fclose(fp);
    return 0;
}

int dump_week(const char *root, int year, int week, int style) {
    char path_week[PATH_MAX] = {0};
    char path_year[PATH_MAX] = {0};
    const int max_days = 7;

    sprintf(path_year, "%s%c%d", root, DIRSEP_C, year);
    sprintf(path_week, "%s%c%d%c%d", root, DIRSEP_C, year, DIRSEP_C, week);

    if (!dir_empty(path_year)) {
        return -1;
    }

    for (int i = 0; i < max_days; i++) {
        char tmp[PATH_MAX];
        sprintf(tmp, "%s%c%d", path_week, DIRSEP_C, i);
        dump_file(tmp, style);
    }
    return 0;
}
