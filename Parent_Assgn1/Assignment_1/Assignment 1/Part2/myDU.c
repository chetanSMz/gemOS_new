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

#define MAX_PATH_LENGTH 4100

unsigned long handle_link(char *path);      // function prototype

unsigned long directory_size_creating_child_processes(char *path, int pipe_fd){
    struct dirent *entry;
    struct stat info;

    DIR *dir = opendir(path);
    if(dir == NULL){
        printf("Unable to execute\n");
        exit(-1);
    }

    unsigned long dir_size = 0;

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0){
            continue;
        }

        char child_path[MAX_PATH_LENGTH];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry -> d_name);

        if(lstat(child_path, &info) != 0){
            printf("Unable to execute\n");
            exit(-1);
        }

        if(S_ISREG(info.st_mode)){
            dir_size += info.st_size;
        }

        else if(S_ISDIR(info.st_mode)){
            dir_size += info.st_size;

            int pipe_fds[2];
            if(pipe(pipe_fds) == -1){
                printf("Unable to execute\n");
                exit(-1);
            }

            pid_t cpid = fork();
            if(cpid < 0){
                printf("Unable to execute\n");
                exit(-1);
            }

            if(cpid == 0){
                close(pipe_fds[0]);
                directory_size_creating_child_processes(child_path, pipe_fds[1]);
                close(pipe_fds[1]);
                exit(0);
            }

            else{
                close(pipe_fds[1]);
                unsigned long temp1 = 0;
                if((read(pipe_fds[0], &temp1, sizeof(temp1))) < 0){
                    printf("Unable to execute\n");
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
                printf("Unable to execute\n");
                exit(-1);
            }

            target_path[len] = '\0';
            if(target_path[strlen(target_path)-1] == '/'){
                target_path[strlen(target_path)-1] = '\0';
            }

            int i = strlen(child_path);
            i--;
            while(child_path[i] != '/'){
                i--;
            }
            child_path[i] = '\0';

            struct stat target_info;
            
            char absolute_path[MAX_PATH_LENGTH];
            if(target_path[0] != '/'){
                strcpy(absolute_path, child_path);
                strcat(absolute_path, "/");
                strcat(absolute_path, target_path);
            }

            else if(target_path[0] == '/'){
                strcpy(absolute_path, child_path);
                strcat(absolute_path, target_path);
            }

            unsigned long temp2 = handle_link(absolute_path);
            dir_size += temp2;
        }

    }

    closedir(dir);

    if(pipe_fd != -1){
        if(write(pipe_fd, &dir_size, sizeof(dir_size)) < 0){
            printf("Unable to execute\n");
            exit(-1);
        }
    }

return dir_size;
}

unsigned long directory_size_recursively(char *path){
    struct dirent *entry;
    struct stat info;

    DIR *dir = opendir(path);
    if(dir == NULL){
        printf("Unable to execute\n");
        exit(-1);
    }

    unsigned long dir_size = 0;

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0){
            continue;
        }

        char child_path[MAX_PATH_LENGTH];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry -> d_name);

        if(lstat(child_path, &info) != 0){
            printf("Unable to execute\n");
            exit(-1);
        }

        if(S_ISREG(info.st_mode)){
            dir_size += info.st_size;
        }

        else if(S_ISDIR(info.st_mode)){
            dir_size += info.st_size;

            unsigned long temp1 = directory_size_recursively(child_path);
            dir_size += temp1;
        }

        else if(S_ISLNK(info.st_mode)){
            char target_path[MAX_PATH_LENGTH];
            int len = readlink(child_path, target_path, sizeof(target_path)-1);

            if(len == -1){
                printf("Unable to execute\n");
                exit(-1);
            }

            target_path[len] = '\0';
            if(target_path[strlen(target_path)-1] == '/'){
                target_path[strlen(target_path)-1] = '\0';
            }

            int i = strlen(child_path);
            i--;
            while(child_path[i] != '/'){
                i--;
            }
            child_path[i] = '\0';

            struct stat target_info;
            
            char absolute_path[MAX_PATH_LENGTH];
            if(target_path[0] != '/'){
                strcpy(absolute_path, child_path);
                strcat(absolute_path, "/");
                strcat(absolute_path, target_path);
            }

            else if(target_path[0] == '/'){
                strcpy(absolute_path, child_path);
                strcat(absolute_path, target_path);
            }

            unsigned long temp2 = handle_link(absolute_path);
            dir_size += temp2;
        }
    }

    closedir(dir);

return dir_size;
}

unsigned long handle_link(char *path){
    struct stat info;
    unsigned long total_size = 0;

    if(lstat(path, &info) != 0){
        printf("Unable to execute\n");
        printf("%s\n", path);
        exit(-1);
    }

    if(S_ISREG(info.st_mode)){
        total_size += info.st_size;
    }

    else if(S_ISDIR(info.st_mode)){
        total_size += info.st_size;
        unsigned long temp1 = directory_size_recursively(path);
        total_size += temp1;
    }

    else if(S_ISLNK(info.st_mode)){
        char target_path[MAX_PATH_LENGTH];
        int len = readlink(path, target_path, sizeof(target_path)-1);

        if(len == -1){
            printf("Unable to execute\n");
            exit(-1);
        }

        target_path[len] = '\0';

        if(target_path[strlen(target_path)-1] == '/'){
            target_path[strlen(target_path)-1] = '\0';
        }

        int i = strlen(path);
        i--;
        while(path[i] != '/'){
            i--;
        }
        path[i] = '\0';

        struct stat target_info;
        
        char absolute_path[MAX_PATH_LENGTH];
        if(target_path[0] != '/'){
            strcpy(absolute_path, path);
            strcat(absolute_path, "/");
            strcat(absolute_path, target_path);
        }

        else if(target_path[0] == '/'){
            strcpy(absolute_path, path);
            strcat(absolute_path, target_path);
        }

        unsigned long temp2 = handle_link(absolute_path);
        total_size += temp2;
    }

return total_size;
}


int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Unable to execute\n");
        exit(-1);
    }

    struct stat root_info;
    if(argv[1][strlen(argv[1])-1] == '/'){
        argv[1][strlen(argv[1])-1] = '\0';
    }
    
    if(lstat(argv[1], &root_info) != 0){
        printf("Unable to execute\n");
        exit(-1);
    }

    if(!S_ISDIR(root_info.st_mode)){
        printf("Unable to execute\n");
        exit(-1);
    }

    unsigned long total_size = root_info.st_size;

    unsigned long dir_size = directory_size_creating_child_processes(argv[1], -1);
    
    total_size += dir_size;
    printf("%lu\n", total_size);

return 0;
}