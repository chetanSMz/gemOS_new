#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

// Function to calculate the total size of regular files in a directory
long long calculateTotalSize(const char *dir_path) {
    long long total_size = 0;
    struct dirent *entry;
    struct stat st;
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // Skip "." and ".." entries
        }

        char file_path[1024];  // Assuming a maximum path length of 1024 characters
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        if (lstat(file_path, &st) == -1) {
            perror("lstat");
            closedir(dir);
            return -1;
        }

        if (S_ISREG(st.st_mode)) {
            total_size += st.st_size;
        } else if (S_ISDIR(st.st_mode)) {
            long long sub_dir_size = calculateTotalSize(file_path);
            if (sub_dir_size == -1) {
                closedir(dir);
                return -1;
            }
            total_size += sub_dir_size;
        }

        else if(S_ISLNK(st.st_mode)) {
            char target_path[1500];
            ssize_t len = readlink(file_path, target_path, sizeof(target_path) - 1);
            if (len == -1) {
                perror("readlink");
                exit(EXIT_FAILURE);
            }
            target_path[len] = '\0';

            struct stat target_info;
            if (stat(target_path, &target_info) == -1) {
                perror("stat (target)");
                exit(EXIT_FAILURE);
            }

            if (S_ISDIR(target_info.st_mode)) {
                long long temp = calculateTotalSize(target_path);
                total_size += temp;
                
            } else if (S_ISREG(target_info.st_mode)) {
                total_size += target_info.st_size;
            }

        }
    }

    closedir(dir);
    return total_size;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    long long total_size = calculateTotalSize(path);

    if (total_size != -1) {
        printf("Total size of regular files in directory %s: %lld bytes\n", path, total_size);
    } else {
        printf("Error calculating total size.\n");
    }

    return 0;
}
