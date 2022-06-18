#ifndef WEEKLY_H
#define WEEKLY_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define HAVE_WINDOWS 1
#else
#define HAVE_WINDOWS 0
#endif

#if HAVE_WINDOWS && (defined(_MSC_BUILD) || defined(_MSC_VER) || defined(_MSC_EXTENSIONS))
#define HAVE_MSVC 1
#else
#define HAVE_MSVC 0
#endif

#if HAVE_WINDOWS && (defined(__MINGW32__) || defined(__MINGW64__))
#define HAVE_MINGW 1
#else
#define HAVE_MINGW 0
#endif

#if HAVE_WINDOWS
#if HAVE_MSVC
        #include <io.h>
        #if !defined(F_OK)
            #define F_OK 0
        #endif
        typedef long long ssize_t;
        typedef unsigned long long size_t;
    #endif

    #if HAVE_MINGW
        #include <dirent.h>
    #endif

    #include <direct.h>
    #include <windows.h>
    #define DIRSEP_C '\\'
    #define DIRSEP_S "\\"
    #define PATHSEP_C ';'
    #define PATHSEP_S ";"
    #define PATHVAR "path"
    #define mkdir(X, Y) mkdir(X)
    #define mkstemp _mktemp
#else
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#define DIRSEP_C '/'
#define DIRSEP_S "/"
#define PATHSEP_C ':'
#define PATHSEP_S ":"
#define PATHVAR "PATH"
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 1024
#endif

#define ARG(X) strcmp(argv[i], X) == 0
#define ARG_NEXT_EXISTS (argv[i+1] != NULL)
#define ARG_NEXT argv[i+1]
#define RECORD_STYLE_SHORT 0
#define RECORD_STYLE_LONG 1
#define RECORD_STYLE_CSV 2
#define RECORD_STYLE_DICT 3
#define WEEK_MAX 54

struct Record {
    char *date;
    char *time;
    char *user;
    char *host;
    char *data;
};

int edit_file(const char *filename);

void record_free(struct Record *record);
struct Record *record_parse(const char *content);
struct Record *record_read(FILE **fp);
void record_show(struct Record *record, int style);
int dump_file(const char *filename, int style);
int dump_week(const char *root, int year, int week, int style);

char *init_tempfile(const char *basepath, const char *ident, char *data);
ssize_t get_file_size(const char *filename);
int append_stdin(const char *filename);
int append_contents(const char *dest, const char *src);

int dir_empty(const char *path);
char *find_program(const char *name);
int make_path(char *basepath);
char *make_output_path(char *basepath, char *path, int year, int week, int day_of_week);

#endif // WEEKLY_H