// Joseph Clauss
#include <stdio.h>
#include <dirent.h>
#include <string.h>

// Function to scan a single file for malware patterns
void scan_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    char line[256];

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "-rf *")) {
            printf("Warning: file %s is infected!\n", filename);
            break;
        }
    }

    fclose(file);
}

// Function to scan all files in a directory
void scan_directory(const char *dirname) {
    DIR *dir = opendir(dirname);
    struct dirent *entry;

    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            scan_file(entry->d_name);
        }
    }

    closedir(dir);
}

int main() {
    scan_directory(".");

    return 0;
}
