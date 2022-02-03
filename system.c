#include "weekly.h"

#if HAVE_MSVC
int dir_empty (const char *path) {
    HANDLE hd;
    WIN32_FIND_DATA data;
    size_t i;
    char winpath[PATH_MAX] = {0};

    i = 0;
    sprintf(winpath, "%s%c*.*", path, DIRSEP_C);
    if ((hd = FindFirstFile(path, &data)) != INVALID_HANDLE_VALUE) {
        do {
            i++;
        } while (FindNextFile(hd, &data) != 0);
        FindClose(hd);
    } else {
        return -1;
    }
    return i != 0;
}
#else
int dir_empty(const char *path) {
    DIR *dir;
    struct dirent *dp;
    size_t i;

    dir = opendir(path);
    if (!dir) {
        return -1;
    }

    i = 0;
    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }
        i++;
    }
    closedir(dir);
    return i != 0;
}
#endif

char *find_program(const char *name) {
#if HAVE_WINDOWS
    int found_extension;
#endif
    static char exe[PATH_MAX] = {0};
    char *token;
    char *pathvar;
    char *abs_prefix[] = {
            "/", "./", ".\\"
    };

    for (size_t i = 0; i < sizeof(abs_prefix) / sizeof(char *); i++) {
        if ((strlen(name) > 1 && name[1] == ':') || !strncmp(name, abs_prefix[i], strlen(abs_prefix[i]))) {
            strcpy(exe, name);
            return exe;
        }
    }

    pathvar = getenv(PATHVAR);
    if (!pathvar) {
        return NULL;
    }

    token = strtok(pathvar, PATHSEP_S);
    while (token != NULL) {
        char filename[PATH_MAX] = {0};
        sprintf(filename, "%s%c%s", token, DIRSEP_C, name);
#if HAVE_WINDOWS
        // I am aware of PATHEXT. Let's stick to binary executables and shell scripts
        char *ext[] = {".bat", ".cmd", ".com", ".exe"};
        char filename_orig[PATH_MAX] = {0};

        found_extension = 0;
        for (size_t i = 0; i < sizeof(ext) / sizeof(char *); i++) {
            if (strstr(filename, ext[i]) != NULL) {
                found_extension = 1;
                break;
            }
        }
        if (!found_extension) {
            for (size_t i = 0; i < sizeof(ext) / sizeof(char *); i++) {
                strcpy(filename_orig, filename);
                strcat(filename, ext[i]);
                if (access(filename, F_OK) == 0) {
                    strcpy(exe, filename);
                    return exe;
                }
                strcpy(filename, filename_orig);
            }
        } else {
            if (access(filename, F_OK) == 0) {
                strcpy(exe, filename);
                return exe;
            }
        }
#else
        if (access(filename, F_OK) == 0) {
            strcpy(exe, filename);
            return exe;
        }
#endif
        token = strtok(NULL, PATHSEP_S);
    }
    return NULL;
}

ssize_t get_file_size(const char *filename) {
    ssize_t result;
    FILE *fp;
    fp = fopen(filename, "rb+");
    if (!fp) {
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    result = ftell(fp);
    fclose(fp);
    return result;
}

char *init_tempfile(const char *basepath, const char *ident, char *data) {
    FILE *fp;
    char *filename;
    filename = calloc(PATH_MAX, sizeof(char));
    sprintf(filename, "%s%cweekly_%s.XXXXXX", basepath, DIRSEP_C, ident);
    if ((mkstemp(filename)) < 0) {
        return NULL;
    }
    unlink(filename);

    fp = fopen(filename, "w+b");
    if (!fp) {
        return NULL;
    }

    if (data != NULL) {
        fwrite(data, sizeof(char), strlen(data), fp);
        //fprintf(fp, "%s\n", data);
    }
    fclose(fp);

    if (chmod(filename, 0600) < 0) {
        return NULL;
    }
    return filename;
}

int make_path(char *basepath) {
    return mkdir(basepath, 0755);
}

char *make_output_path(char *basepath, char *path, int year, int week, int day_of_week) {
    strcpy(path, basepath);
    // Add trailing slash if it's not there
    if (strlen(basepath) > 0 && strlen(basepath) - 1 != DIRSEP_C) {
        strcat(path, DIRSEP_S);
    }

    make_path(path);
    // year
    sprintf(path + strlen(path), "%d%c", year, DIRSEP_C);
    make_path(path);
    // week of year
    sprintf(path + strlen(path), "%d%c", week, DIRSEP_C);
    make_path(path);
    // day of that week
    sprintf(path + strlen(path), "%d", day_of_week);

    return path;
}

