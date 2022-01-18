#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define ARG_VERIFY_NEXT() (argv[i+1] != NULL)
#define RECORD_STYLE_SHORT 0
#define RECORD_STYLE_LONG 1
#define RECORD_STYLE_CSV 2
#define RECORD_STYLE_DICT 3

char *program_name;
char *homedir;
char journalroot[PATH_MAX] = {0};
char intermediates[PATH_MAX] = {0};
const char *VERSION = "1.0.0";
const char *FMT_HEADER = "\x01\x01\x01## date:   %s\n"
                         "## time:   %s\n"
                         "## author: %s\n"
                         "## host:   %s\n\x02\x02\x02";
const char *FMT_FOOTER = "\x03\x03\x03";
const char *USAGE_STATEMENT = \
    "usage: %s [-h] [-V] [-dDy] [-]\n\n"
    "Weekly Report Generator v%s\n\n"
    "Options:\n"
    "--help             -h        Show this usage statement\n"
    "--dump-relative    -d        Dump records relative to current week\n"
    "--dump-absolute    -D        Dump records by week value\n"
    "--dump-year        -y        Set dump-[relative|absolute] year\n"
    "--dump-style       -s        Set output style:\n"
    "                               long (default)\n"
    "                               short\n"
    "                               csv\n"
    "                               dict\n"
    "--version          -V        Show version\n";

void usage() {
    char *name;
    unsigned is_base;

    is_base = 0;
    name = strrchr(program_name, DIRSEP_C);
    if (name != NULL) {
        is_base = 1;
    }
    printf(USAGE_STATEMENT, is_base ? name + 1 : program_name, VERSION);
}

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

int edit_file(const char *filename) {
    char editor[255] = {0};
    char editor_cmd[PATH_MAX] = {0};
    int result;
    const char *user_editor;

    // Allow the user to override the default editor (vi/notepad)
    user_editor = getenv("EDITOR");
    char *editor_path;
    if (user_editor != NULL) {
        sprintf(editor, "%s", user_editor);
    } else {
        editor_path = find_program("vim");
        if (editor_path != NULL) {
            sprintf(editor, "\"%s\"", editor_path);
        }
#if HAVE_WINDOWS
        else {
            strcpy(editor, "notepad");
        }
#endif
    }

    if (!strlen(editor)) {
        fprintf(stderr, "Unable to find editor: %s\n", editor);
        exit(1);
    }

    // Tell editor to jump to the end of the file (when supported)
    // Standard 'vi' does not support '+'
    if(strstr(editor, "vim")) {
        strcat(editor, " +");
    } else if (strstr(editor, "nano") != NULL) {
        strcat(editor, " +9999");
    }

    sprintf(editor_cmd, "%s %s", editor, filename);
    result = system(editor_cmd);
    return result;
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

ssize_t get_file_size(const char *filename) {
    ssize_t result;
    FILE *fp;
    fp = fopen(filename, "r");
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

struct Record {
    char *date;
    char *time;
    char *user;
    char *host;
    char *data;
};

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

struct Record *get_records(FILE **fp) {
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

int dump_file(const char *filename, int style) {
    FILE *fp;
    fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }
    struct Record *record;
    while ((record = get_records(&fp)) != NULL) {
        record_show(record, style);
        record_free(record);
        puts("");
    }
    fclose(fp);
    return 0;
}

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

int main(int argc, char *argv[]) {
    // System info
    char sysname[255] = {0};
    char username[255] = {0};
#if HAVE_WINDOWS
    DWORD usernamesz;
    usernamesz = sizeof(username);
    DWORD sysnamesz;
    sysnamesz = sizeof(sysname);
    homedir = getenv("USERPROFILE");
#else
    struct passwd *user;
    homedir = getenv("HOME");
#endif

    // Time and datestamp
    time_t t;
    struct tm *tm_;
    int year, week, day_of_week;
    char datestamp[255] = {0};
    char timestamp[255] = {0};

    // Path and data buffers
    char *headerfile;
    char *tempfile;
    char *footerfile;
    char journalfile[PATH_MAX] = {0};
    char header[255];
    char footer[255];

    // Argument triggers
    int do_stdin;
    int do_dump;
    int do_year;
    int do_style;
    int user_year;
    char *user_year_error;
    int user_week;
    char *user_week_error;
    int style;

    // Set program name
    program_name = argv[0];
    // Get current time
    t = time(NULL);
    // Populate tm struct
    tm_ = localtime(&t);
    // Time data fix ups
    year = tm_->tm_year + 1900;
    week = (tm_->tm_yday + 7 - (tm_->tm_wday + 1 ? (tm_->tm_wday - 1) : 6)) / 7;
    day_of_week = tm_->tm_wday;
    // Set default output style
    style = RECORD_STYLE_LONG;

#if HAVE_WINDOWS
    // Get system name
    if(!GetComputerName(sysname, &sysnamesz)) {
        perror("Unable to get system host name");
        strcpy(sysname, "unknown");
    }
    // Get user information
    if(!GetUserName(username, &usernamesz)) {
        perror("Unable to read account information");
        strcpy(username, "unknown");
    }
#else
    // Get system name
    if (gethostname(sysname, sizeof(sysname) - 1) < 0) {
        perror("Unable to get system host name");
        strcpy(sysname, "unknown");
    }

    // Get user information
    user = getpwuid(getuid());
    if (user == NULL) {
        perror("Unable to read account information");
        strcpy(username, "unknown");
    } else {
        strcpy(username, user->pw_name);
    }
#endif
    // Get data;
    strftime(datestamp, sizeof(datestamp) - 1, "%m/%d/%Y", tm_);
    strftime(timestamp, sizeof(timestamp) - 1, "%H:%M:%S", tm_);

    // Populate header string
    sprintf(header, FMT_HEADER, datestamp, timestamp, username, sysname);
    // Populate footer string
    strcpy(footer, FMT_FOOTER);
    // Populate path(s)
    sprintf(journalroot, "%s%c.weekly", homedir, DIRSEP_C);
    sprintf(intermediates, "%s%ctmp", journalroot, DIRSEP_C);

    // Prime argument triggers
    do_stdin = 0;
    do_dump = 0;
    do_year = 0;

    // Parse user arguments
    for (int i = 1; i < argc; i++) {
        if (ARG("-h") || ARG("--help")) {
            usage();
            exit(0);
        }
        if (ARG("-V") || ARG("--version")) {
            puts(VERSION);
            exit(0);
        }
        if (ARG("-")) {
            do_stdin = 1;
        }
        if (ARG("-d") || ARG("--dump-relative")) {
            if (ARG_VERIFY_NEXT()) {
                user_week = (int) strtol(argv[i + 1], &user_week_error, 10);
                if (*user_week_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week -= user_week;
            }
            do_dump = 1;
        }
        if (ARG("-D") || ARG("--dump-absolute")) {
            if (ARG_VERIFY_NEXT()) {
                user_week = (int) strtol(argv[i + 1], &user_week_error, 10);
                if (*user_week_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week = user_week;
            }
            do_dump = 1;
        }
        if (ARG("-y") || ARG("--dump-year")) {
            if (!ARG_VERIFY_NEXT()) {
                fprintf(stderr, "--dump-year (-y) requires an integer year\n");
                exit(1);
            }
            user_year = (int) strtol(argv[i + 1], &user_year_error, 10);
            if (*user_year_error != '\0') {
                fprintf(stderr, "Invalid integer\n");
                exit(1);
            }
            year = user_year;
            do_year = 1;
        }
        if (ARG("-s") || ARG("--dump-style")) {
            if (!ARG_VERIFY_NEXT()) {
                fprintf(stderr, "--dump-style (-s) requires a style argument (i.e. short, long, csv, dict)\n");
                exit(1);
            }
            if (!strcmp(argv[i + 1], "short")) {
                style = RECORD_STYLE_SHORT;
            } else if (!strcmp(argv[i + 1], "long")) {
                style = RECORD_STYLE_LONG;
            } else if (!strcmp(argv[i + 1], "csv")) {
                style = RECORD_STYLE_CSV;
            } else if (!strcmp(argv[i + 1], "dict")) {
                style = RECORD_STYLE_DICT;
            }
            do_style = 1;
        }
    }

    if (do_year && !do_dump) {
        fprintf(stderr, "Option --dump-year (-y) requires options -d or -D\n");
        exit(1);
    }

    if (do_style && !do_dump) {
        fprintf(stderr, "Option --dump-style (-s) requires options -d or -D\n");
        exit(1);
    }

    if (do_dump) {
        if (week < 1) {
            week = 1;
        }
        if (dump_week(journalroot, year, week, style) < 0) {
            fprintf(stderr, "No entries found for week %d of %d\n", week, year);
            exit(1);
        }
        exit(0);
    }

    // Create weekly root directory
    make_path(journalroot);

    // Write header string to temporary file
    make_path(intermediates);

    if ((headerfile = init_tempfile(intermediates, "header", header)) == NULL) {
        perror("Unable to create temporary header file");
        exit(1);
    }

    char nothing[1] = {0};
    if ((tempfile = init_tempfile(intermediates, "tempfile", nothing)) == NULL) {
        perror("Unable to create temporary file");
        exit(1);
    }

    if ((footerfile = init_tempfile(intermediates, "footer", footer)) == NULL) {
        perror("Unable to create footer file");
        exit(1);
    }

    // Create new weekly journalfile path
    if (!make_output_path(journalroot, journalfile, year, week, day_of_week)) {
        fprintf(stderr, "Unable to create output path: %s (%s)\n", journalfile, strerror(errno));
        perror(journalfile);
        unlink(headerfile);
        unlink(tempfile);
        unlink(footerfile);
        exit(1);
    }

    // Get original size of the temporary file
    ssize_t tempfile_size;
    tempfile_size = get_file_size(tempfile);

    if (do_stdin) {
        if (append_stdin(tempfile) < 0) {
            fprintf(stderr, "Failed to read from stdin\n");
            exit(1);
        }
    } else {
        // Open the temporary file with an editor so the user can write their notes
        if (edit_file(tempfile) != 0) {
            fprintf(stderr, "Non-zero exit status from editor. Aborting.\n");
            fprintf(stderr, "Dead entry file: %s\n", tempfile);
            exit(1);
        }
    }

    // Test whether temporary file size increased. If not, die.
    ssize_t tempfile_newsize;
    tempfile_newsize = get_file_size(tempfile);
    if (tempfile_newsize <= tempfile_size) {
        fprintf(stderr, "Empty message, aborting.\n");
        unlink(headerfile);
        unlink(tempfile);
        unlink(footerfile);
        exit(1);
    }

    // Copy data from the header file to the weekly journal path
    if (append_contents(journalfile, headerfile) < 0) {
        fprintf(stderr, "Unable to append contents of '%s' to '%s' (%s)\n", headerfile, journalfile, strerror(errno));
        exit(1);
    }

    // Copy data from the temporary file to the weekly journal path
    if (append_contents(journalfile, tempfile) < 0) {
        fprintf(stderr, "Unable to append contents of '%s' to '%s' (%s)\n", tempfile, journalfile, strerror(errno));
        exit(1);
    }

    // Copy data from the footer file to the weekly journal path
    if (append_contents(journalfile, footerfile) < 0) {
        fprintf(stderr, "Unable to append contents of '%s' to '%s' (%s)\n", footerfile, journalfile, strerror(errno));
        exit(1);
    }

    // Nuke the temporary files (report on error, but keep going)
    if (access(headerfile, F_OK) == 0 && unlink(headerfile) < 0) {
        fprintf(stderr, "Unable to remove header file: %s (%s)\n", headerfile, strerror(errno));
    }
    if (access(tempfile, F_OK) == 0 && unlink(tempfile) < 0) {
        fprintf(stderr, "Unable to remove temporary file: %s (%s)\n", tempfile, strerror(errno));
    }
    if (access(footerfile, F_OK) == 0 && unlink(footerfile) < 0) {
        fprintf(stderr, "Unable to remove footer file: %s (%s)\n", footerfile, strerror(errno));
    }

    // Inform the user
    printf("Message written to: %s\n", journalfile);
    return 0;
}
