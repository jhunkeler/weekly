#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>

#define ARG(X) strcmp(argv[i], X) == 0
#define ARG_VERIFY_NEXT() (argv[i+1] != NULL)

char *program_name;
char journalroot[1024] = {0};
char intermediates[1024] = {0};
const char *VERSION = "1.0.0";
const char *FMT_HEADER = "## date:   %s\n"
                         "## author: %s\n"
                         "## host:   %s\n";
const char *USAGE_STATEMENT = \
    "usage: %s [-h] [-V] [-dDy] [-]\n\n"
    "Weekly Report Generator v%s\n\n"
    "Options:\n"
    "--help             -h        Show this usage statement\n"
    "--dump-relative    -d        Dump records relative to current week\n"
    "--dump-absolute    -D        Dump records by week value\n"
    "--dump-year        -y        Set dump-[relative|absolute] year\n"
    "--version          -V        Show version\n";

void usage() {
    char *name;
    name = strrchr(program_name, '/');
    if (name == NULL) {
        name = program_name;
    }
    printf(USAGE_STATEMENT, name + 1, VERSION);
}

int edit_file(const char *filename) {
    char editor[255];
    char editor_cmd[1024];
    int result;
    const char *user_editor;

    // Allow the user to override the default editor (vi)
    user_editor = getenv("EDITOR");
    if (user_editor != NULL) {
        strcpy(editor, user_editor);
    } else {
        strcpy(editor, "vi");
    }

    // When using vi, set editor cursor to the end of the file
    if(strstr(editor, "vim") || strstr(editor, "vi")) {
        strcat(editor, " +99999");
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
    if (strlen(basepath) > 0 && strlen(basepath) - 1 != '/') {
        strcat(path, "/");
    }

    make_path(path);
    // year
    sprintf(path + strlen(path), "%d/", year);
    make_path(path);
    // week of year
    sprintf(path + strlen(path), "%d/", week);
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

char *init_tempfile(const char *basepath, char *data) {
    FILE *fp;
    static char tempfile[1024];
    sprintf(tempfile, "%s/weekly.XXXXXX", basepath);
    if ((mkstemp(tempfile)) < 0) {
        return NULL;
    }
    unlink(tempfile);

    fp = fopen(tempfile, "w+");
    if (data != NULL) {
        fprintf(fp, "%s\n", data);
    }
    fclose(fp);

    if (chmod(tempfile, 0600) < 0) {
        return NULL;
    }
    return tempfile;
}

int append_stdin(const char *filename) {
    FILE *fp;
    size_t bufsz;
    char *buf;

    buf = malloc(BUFSIZ);
    if (!buf) {
        return -1;
    }

    fp = fopen(filename, "a");
    if (!fp) {
        perror(filename);
        return -1;
    }

    while (getline(&buf, &bufsz, stdin) >= 0) {
        fprintf(fp, "%s", buf);
    }
    free(buf);
    fclose(fp);
    return 0;
}

int append_contents(const char *dest, const char *src) {
    char buf[BUFSIZ] = {0};
    FILE *fpi, *fpo;

    fpi = fopen(src, "r");
    if (!fpi) {
        perror(src);
        return -1;
    }

    fpo = fopen(dest, "a");
    if (!fpo) {
        perror(dest);
        return -1;
    }

    // Append source file to destination file
    while (fgets(buf, sizeof(buf), fpi) != NULL) {
        fprintf(fpo, "%s", buf);
    }
    fprintf(fpo, "\n");

    fclose(fpo);
    fclose(fpi);
    return 0;
}

int dump_file(const char *filename) {
    char buf[BUFSIZ] = {0};
    FILE *fp;
    fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }
    while ((fgets(buf, BUFSIZ, fp)) != NULL) {
        printf("%s", buf);
    }
    fclose(fp);
    return 0;
}

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

int dump_week(const char *root, int year, int week) {
    char path_week[1024] = {0};
    char path_year[1024] = {0};
    const int max_days = 7;

    sprintf(path_year, "%s/%d", root, year);
    sprintf(path_week, "%s/%d/%d", root, year, week);

    if (dir_empty(path_year)) {
        return -1;
    }

    for (int i = 0; i < max_days; i++) {
        char tmp[1024];
        sprintf(tmp, "%s/%d", path_week, i);
        dump_file(tmp);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // System info
    struct passwd *user;
    char sysname[255] = {0};
    char username[255] = {0};

    // Time and date
    time_t t;
    struct tm *tm_;
    int year, week, day_of_week;
    char date[255] = {0};

    // Path and data buffers
    char *tempfile;
    char journalfile[1024] = {0};
    char output[255];

    // Argument triggers
    int do_stdin;
    int do_dump;
    int do_year;
    int uyear;
    char *uyear_error;
    int uweek;
    char *uweek_error;

    // Set program name
    program_name = argv[0];
    // Get current time
    t = time(NULL);
    // Populate tm struct
    tm_ = localtime(&t);
    // Time data fix ups
    year = tm_->tm_year + 1900;
    week = (tm_->tm_yday + 7 - (tm_->tm_wday ? (tm_->tm_wday - 1) : 6)) / 7;
    day_of_week = tm_->tm_wday;

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

    strftime(date, sizeof(date) - 1, "%c", tm_);

    // Populate header string
    sprintf(output, FMT_HEADER, date, user->pw_name, sysname);
    sprintf(journalroot, "%s/.weekly", getenv("HOME"));
    sprintf(intermediates, "%s/tmp", journalroot);

    // Prime argument triggers
    do_stdin = 0;
    do_dump = 0;
    do_year = 0;
    uyear = year;
    uweek = week;

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
                uweek = (int) strtol(argv[i + 1], &uweek_error, 10);
                if (*uweek_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week -= uweek;
            }
            do_dump = 1;
        }
        if (ARG("-D") || ARG("--dump-absolute")) {
            if (ARG_VERIFY_NEXT()) {
                uweek = (int) strtol(argv[i + 1], &uweek_error, 10);
                if (*uweek_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week = uweek;
            }
            do_dump = 1;
        }
        if (ARG("-y") || ARG("--dump-year")) {
            if (!ARG_VERIFY_NEXT()) {
                fprintf(stderr, "--dump-year (-y) requires an integer year\n");
                exit(1);
            }
            uyear = (int) strtol(argv[i + 1], &uyear_error, 10);
            if (*uyear_error != '\0') {
                fprintf(stderr, "Invalid integer\n");
                exit(1);
            }
            year = uyear;
            do_year = 1;
        }
    }

    if (do_year && !do_dump) {
        fprintf(stderr, "Option --dump-year (-y) requires options -d or -D\n");
        exit(1);
    }

    if (do_dump) {
        if (week < 0) {
            week = 0;
        }
        if (dump_week(journalroot, year, week) < 0) {
            fprintf(stderr, "No entries found for week %d of %d\n", week, year);
            exit(1);
        }
        exit(0);
    }

    // Write header string to temporary file
    make_path(intermediates);
    if ((tempfile = init_tempfile(intermediates, output)) == NULL) {
        perror("Unable to create temporary file");
        exit(1);
    }

    // Create new weekly journalfile path
    if (make_output_path(journalroot, journalfile, year, week, day_of_week) < 0) {
        perror(journalfile);
        unlink(tempfile);
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
        fprintf(stderr, "Intermediate file unchanged, or smaller:\nOriginal size: %zi\nNew size: %zi\n",
                tempfile_size, tempfile_newsize);
        unlink(tempfile);
        exit(1);
    }

    // Copy data from the temporary file to the weekly journal path
    if (append_contents(journalfile, tempfile) < 0) {
        perror(journalfile);
        exit(1);
    }

    // Nuke the temporary file
    if (unlink(tempfile) < 0) {
        perror(tempfile);
        exit(1);
    }

    // Inform the user
    printf("Wrote: %s\n", journalfile);
    return 0;
}
