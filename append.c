#include "weekly.h"

int append_stdin(const char *filename) {
    FILE *fp;
    size_t bufsz;
    char *buf;

    bufsz = BUFSIZ;
    buf = malloc(bufsz);
    if (!buf) {
        return -1;
    }

    fp = fopen(filename, "a");
    if (!fp) {
        perror(filename);
        free(buf);
        return -1;
    }

#if HAVE_WINDOWS
    while (fgets(buf, (int) bufsz, stdin) != NULL) {
        fprintf(fp, "%s", buf);
    }
#else
    while (getline(&buf, &bufsz, stdin) >= 0) {
        fprintf(fp, "%s", buf);
    }
#endif
    free(buf);
    fclose(fp);
    return 0;
}

int append_contents(const char *dest, const char *src) {
    char buf[BUFSIZ] = {0};
    FILE *fpi, *fpo;

    fpi = fopen(src, "rb+");
    if (!fpi) {
        perror(src);
        return -1;
    }

    fpo = fopen(dest, "ab+");
    if (!fpo) {
        perror(dest);
        fclose(fpi);
        return -1;
    }

    // Append source file to destination file
    while (fread(buf, sizeof(char), sizeof(buf), fpi) > 0) {
        fwrite(buf, sizeof(char), strlen(buf), fpo);
    }
    buf[0] = '\n';
    fwrite(buf, sizeof(char), 1, fpo);

    fclose(fpo);
    fclose(fpi);
    return 0;
}

