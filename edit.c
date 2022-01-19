#include "weekly.h"

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
