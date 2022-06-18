#include "weekly.h"

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
    "usage: %s [-h] [-V] [-dDys] [-]\n\n"
    "Weekly Report Generator v%s\n\n"
    "Environment Variables:\n"
    "WEEKLY_JOURNAL_ROOT          Override journal destination\n"
    "                               (e.g., /shared/weeklies/$USER)\n\n"
    "Options:\n"
    "--help             -h        Show this usage statement\n"
    "--all              -a        Dump all records\n"
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
    int do_all;
    int user_year;
    char *user_year_error;
    int user_week;
    char *user_week_error;
    int style;
    char *user_journalroot;

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
    // Get date and time
    strftime(datestamp, sizeof(datestamp) - 1, "%m/%d/%Y", tm_);
    strftime(timestamp, sizeof(timestamp) - 1, "%H:%M:%S", tm_);

    // Populate header string
    sprintf(header, FMT_HEADER, datestamp, timestamp, username, sysname);
    // Populate footer string
    strcpy(footer, FMT_FOOTER);
    // Populate path(s)
    if ((user_journalroot = getenv("WEEKLY_JOURNAL_ROOT")) != NULL) {
        strcpy(journalroot, user_journalroot);
    } else {
        sprintf(journalroot, "%s%c.weekly", homedir, DIRSEP_C);
    }
    sprintf(intermediates, "%s%ctmp", journalroot, DIRSEP_C);

    // Prime argument triggers
    do_stdin = 0;
    do_dump = 0;
    do_year = 0;
    do_style = 0;
    do_all = 0;

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
        if (ARG("-a") || ARG("--all")) {
            do_all = 1;
            do_dump = 1;
        }
        if (ARG("-d") || ARG("--dump-relative")) {
            if (ARG_NEXT_EXISTS) {
                user_week = (int) strtol(ARG_NEXT, &user_week_error, 10);
                if (*user_week_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week -= user_week;
            }
            do_dump = 1;
        }
        if (ARG("-D") || ARG("--dump-absolute")) {
            if (ARG_NEXT_EXISTS) {
                user_week = (int) strtol(ARG_NEXT, &user_week_error, 10);
                if (*user_week_error != '\0') {
                    fprintf(stderr, "Invalid integer\n");
                    exit(1);
                }
                week = user_week;
            }
            do_dump = 1;
        }
        if (ARG("-y") || ARG("--dump-year")) {
            if (!ARG_NEXT_EXISTS) {
                fprintf(stderr, "--dump-year (-y) requires an integer year\n");
                exit(1);
            }
            user_year = (int) strtol(ARG_NEXT, &user_year_error, 10);
            if (*user_year_error != '\0') {
                fprintf(stderr, "Invalid integer\n");
                exit(1);
            }
            year = user_year;
            do_year = 1;
        }
        if (ARG("-s") || ARG("--dump-style")) {
            if (!ARG_NEXT_EXISTS) {
                fprintf(stderr, "--dump-style (-s) requires a style argument (i.e. short, long, csv, dict)\n");
                exit(1);
            }
            if (!strcmp(ARG_NEXT, "short")) {
                style = RECORD_STYLE_SHORT;
            } else if (!strcmp(ARG_NEXT, "long")) {
                style = RECORD_STYLE_LONG;
            } else if (!strcmp(ARG_NEXT, "csv")) {
                style = RECORD_STYLE_CSV;
            } else if (!strcmp(ARG_NEXT, "dict")) {
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
        if (access(journalroot, F_OK) < 0) {
            fprintf(stderr, "Unable to access %s: %s\n", journalroot, strerror(errno));
            exit(1);
        }
        if (do_all) {
            struct dirent *dp;
            DIR *dir;
            dir = opendir(journalroot);
            while ((dp = readdir(dir)) != NULL) {
                if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                    continue;
                if (isdigit(*dp->d_name)) {
                    for (int w = week; w < WEEK_MAX; w++)
                        dump_week(journalroot, year, w, style);
                }
            }
        } else {
            if (dump_week(journalroot, year, week, style) < 0) {
                fprintf(stderr, "No entries found for week %d of %d\n", week, year);
                exit(1);
            }
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
