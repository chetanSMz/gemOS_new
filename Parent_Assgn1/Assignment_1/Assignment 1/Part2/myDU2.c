#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <limits.h>

#define MAX_PATH_LENGTH 4096

unsigned long handle_link(char *path, int len);     // function prototype

unsigned long handle_directory(char *path, int len, int pipe_fd){
    struct dirent *entry;
    struct stat info;

    DIR *dir = opendir(path);
    if(dir == NULL){
        printf("Unable to execute - failure in opendir\n");
        exit(-1);
    }

    unsigned long dir_size = 0;

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0){
            continue;
        }

        char child_path[MAX_PATH_LENGTH];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry -> d_name);
        // printf("entry d_name = %s\n", entry -> d_name);

        if(lstat(child_path, &info) != 0){
            printf("Unable to execute - lstat error on children files/directories\n");
            exit(-1);
        }

        if(S_ISREG(info.st_mode)){
            dir_size += info.st_size;
        }

        else if(S_ISDIR(info.st_mode)){
            dir_size += info.st_size;

            int pipe_fds[2];
            if(pipe(pipe_fds) == -1){
                printf("Unable to execute - error in pipe\n");
                exit(-1);
            }

            pid_t cpid = fork();
            if(cpid < 0){
                printf("Unable to execute - error in fork\n");
                exit(-1);
            }

            if(cpid == 0){
                close(pipe_fds[0]);
                handle_directory(child_path, strlen(child_path), pipe_fds[1]);      // here you are not taking the return value;
                close(pipe_fds[1]);
                exit(0);
            }

            else{
                close(pipe_fds[1]);
                unsigned long temp1;
                if((read(pipe_fds[0], &temp1, sizeof(temp1))) < 0){
                    printf("Unable to execute - error in read\n");
                    exit(-1);
                }
                close(pipe_fds[0]);
                dir_size += temp1;
                wait(NULL);
            }
        }

        else if(S_ISLNK(info.st_mode)){
            char target_path[MAX_PATH_LENGTH];
            int len = readlink(child_path, target_path, sizeof(target_path)-1);

            if(len == -1){
                printf("Unable to execute - cannot find the target path from the symbolic link\n");
                exit(-1);
            }

            target_path[len] = '\0';

            char *final_target_path = (char *)malloc(MAX_PATH_LENGTH);

            int j = strlen(target_path);
            j--;
            if(target_path[j] == '/'){
                target_path[j] = '\0';
            }

            j = 0;

            while(1){
                int present = 0;
                int i = strlen(child_path);
                i--;

                while(i >= 0 && child_path[i] != '/'){
                    i--;
                }

                if(i < 0){
                    i = 0;
                }

                child_path[i] = '\0';

                if(j < strlen(target_path) && target_path[j] == '.' && target_path[j+1] == '.' && target_path[j+2] == '/'){
                    j += 3;
                    present = 1;
                }

                if(present == 0){
                    break;
                }
            }

            char *target_path_modified = (char *)malloc(sizeof(char) * (strlen(target_path) - j + 1));

            int k = j;
            for(k = j; k < strlen(target_path); k++){
                target_path_modified[k-j] = target_path[k];
            }

            target_path_modified[k] = '\0';

            strcpy(final_target_path, child_path);
            strcat(final_target_path, target_path_modified);

            final_target_path[strlen(final_target_path)] = '\0';

            // printf("final target path = %s\n", final_target_path);
            // printf("target_path_modified = %s\n", target_path_modified);

            // printf("path = %s\n", child_path);
            // printf("path pointed by symlink = %s\n", target_path);

            unsigned long temp2 = handle_link(final_target_path, strlen(final_target_path));
            dir_size += temp2;
        }

    }

    closedir(dir);

    if(pipe_fd != -1){
        if(write(pipe_fd, &dir_size, sizeof(dir_size)) < 0){
            printf("Unable to execute - error in write\n");
            exit(-1);
        }
    }

return dir_size;
}

// think about creating a separate function for handling sub directory

unsigned long handle_link(char *path, int len){
    struct stat info;

    unsigned long dir_size = 0;
    if(lstat(path, &info) != 0){
        // printf("Unable to execute - error in lstat of directory/file pointed by symlink\n");
        // perror("error in lstat of directory/file pointed by symlink");
        printf("path = %s\n", path);
        exit(-1);
    }

    if(S_ISDIR(info.st_mode)){
        dir_size += info.st_size;
        unsigned long temp1 = handle_directory(path, len, -1);
        dir_size += temp1;
    }

    else if(S_ISREG(info.st_mode)){
        dir_size += info.st_size;
    }

    else if(S_ISLNK(info.st_mode)){
        char target_path[MAX_PATH_LENGTH];
        int len2 = readlink(path, target_path, sizeof(target_path)-1);

        if(len2 == -1){
            printf("Unable to execute - cannot find the target path from the symbolic link\n");
            exit(-1);
        }

        target_path[len2] = '\0';

            char *final_target_path = (char *)malloc(MAX_PATH_LENGTH);

            int j = strlen(target_path);
            j--;
            if(target_path[j] == '/'){
                target_path[j] = '\0';
            }

            j = 0;

            while(1){   
                int present = 0;
                int i = strlen(path);
                i--;

                while(i >= 0 && path[i] != '/'){
                    i--;
                }

                if(i < 0){
                    i = 0;
                }

                path[i] = '\0';

                if(j < strlen(target_path) && target_path[j] == '.' && target_path[j+1] == '.' && target_path[j+2] == '/'){
                    j += 3;
                    present = 1;
                }

                if(present == 0){
                    break;
                }
            }

            char *target_path_modified = (char *)malloc(sizeof(char) * (strlen(target_path) - j + 1));  // what is I use MAX_PATH_LENGTH for the space allocation

            int k = j;
            for(k = j; k < strlen(target_path); k++){
                target_path_modified[k-j] = target_path[k];
            }

            target_path_modified[k] = '\0';

            strcpy(final_target_path, path);
            strcat(final_target_path, target_path_modified);

            final_target_path[strlen(final_target_path)] = '\0';

            // printf("final target path = %s\n", final_target_path);
            // printf("target_path_modified = %s\n", target_path_modified);

            // printf("path = %s\n", child_path);
            // printf("path pointed by symlink = %s\n", target_path);

        unsigned long temp2 = handle_link(final_target_path, strlen(final_target_path));
        dir_size += temp2;
    }

return dir_size;
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Unable to execute - incorrect number of arguments\n");
        exit(-1);
    }

    struct stat root_info;
    if(argv[1][strlen(argv[1])-1] == '/'){
        argv[1][strlen(argv[1])-1] = '\0';
    }

    if(lstat(argv[1], &root_info) != 0){
        printf("Unable to execute - lstat error on root directory\n");
        exit(-1);
    }

    if(!S_ISDIR(root_info.st_mode)){
        printf("Unable to execute - the path given is not a directory\n");
        exit(-1);
    }

    unsigned long total_size = root_info.st_size;

    unsigned long dir_size = handle_directory(argv[1], strlen(argv[1]), -1);
    
    total_size += dir_size;
    printf("%lu\n", total_size);

return 0;
}