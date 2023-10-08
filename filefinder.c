#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/time.h>

typedef struct child_info{
    char job_info[10000];
    int pid;
    int serial_num;
} child_info;

void cyan();
void default_color();

void extract_input(char **command1_ptr, char **file_ptr, char **ext1_ptr, char **ext2_ptr, char *user_input);
int find_file(char *file, char *search_dir, char **paths, int *num_paths, int sub_dir);
int find_text(char *str, char *search_dir, char **paths, int *num_paths, char *file_ext, int sub_dir);

void print_time(time_t start, time_t end);
void list_children();
void kill_all();
void signal_handler(int signal); /*signal handler/pipe redirection*/

int get_entry(int *ticket, int *choosing, int id);
int kill(pid_t pid, int sig);

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

int fd[2], d, *signal_received; /*global*/

child_info *child_info_arr;

int *ticket;
int *choosing;

int main(){
    size_t len;/*for removing quotes*/

    int pid, i, status, num_children;
    char user_input[1000], original[1000];
     
    char *command1, *file, *ext1, *ext2;

    d = dup(STDIN_FILENO);

    pipe(fd);

    signal_received = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    child_info_arr = (child_info*)mmap(NULL, sizeof(child_info)*10, PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    ticket = (int*)mmap(NULL, sizeof(int) * 10, PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    choosing = (int*)mmap(NULL, sizeof(int) * 10, PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);

    *signal_received = 0;
    num_children = 0;

    cyan();
    fprintf(stderr, "findstuff$ ");
    default_color();

    read(STDIN_FILENO, user_input, 1000);
    user_input[strcspn(user_input, "\n")] = '\0';


    while(strcmp(user_input, "q") != 0 && strcmp(user_input, "quit") != 0){
        strcpy(original, user_input); /*for putting into child_info struct*/
        extract_input(&command1, &file, &ext1, &ext2, user_input);

        /*set up signal handler*/
        signal(SIGUSR1, signal_handler);

        /*fprintf(stderr, "\nfile: %s\n", file);*/

        if (command1 != NULL && strcmp(command1, "find") == 0){
            /*fork a child*/
            int total;
            char *paths[100];
            if (num_children < 10){
                pid = fork();
            }
            else{
                fprintf(stderr, "10 processes already running. Please wait.\n");
                cyan();
                fprintf(stderr, "findstuff$ ");
                default_color();

                read(STDIN_FILENO, user_input, 1000);
                user_input[strcspn(user_input, "\n")] = '\0';
                continue;
            }

            if (pid == 0 && file != NULL){
                int id, num_paths, x;
                time_t start, end;
        
                /*for getting file paths*/ 
                num_paths = 0;

                start = time(NULL);
                if (file[0] != '"'){
                    /*find file*/
                    if (ext1 == NULL){
                        total = find_file(file, ".", paths, &num_paths, 0);
                    }
                    else if (ext1 != NULL && strcmp(ext1, "-s") == 0){
                        total = find_file(file, ".", paths, &num_paths, 1);
                    }
                }

                else{
                    
                    /*find text*/
                    len = strlen(file);
                    if (len >= 2 && file[0] == '"' && file[len - 1] == '"') {
                        memmove(file, file + 1, len - 2);
                        file[len - 2] = '\0';
                    }

                    /*fprintf(stderr, "after quote removal: %s\n", file);*/

                    if (ext1 != NULL && strcmp(ext1, "-s") == 0){
                        total = find_text(file, ".", paths, &num_paths, NULL, 1);
                    }

                    else if (ext1 != NULL){
                        char *ext = ext1 + 3;
                        if (ext2 != NULL && strcmp(ext2, "-s") == 0){
                            total = find_text(file, ".", paths, &num_paths, ext, 1);
                        }

                        else{         
                            total = find_text(file, ".", paths, &num_paths, ext, 0);
                        }
                    }

                    else{
                        /*no extensions, find in cur dir in every file*/
                        total = find_text(file, ".", paths, &num_paths, NULL, 0);   
                    }

                }
                /*i = (rand()%(20 - 10 + 1)) + 10;*/
                
                /*REMOVE*/
                sleep(10); 

                end = time(NULL);

                /*get entry*/
                id = get_entry(ticket, choosing, getpid());

                close(fd[0]);
                
                /*write pid*/
                x = getpid();
                write(fd[1], &x, sizeof(int));

                /*write time*/
                write(fd[1], &start, sizeof(time_t));
                write(fd[1], &end, sizeof(time_t));

                if (num_paths == 0){
                    total = 14;
                    write(fd[1], &total, sizeof(int));
                    write(fd[1], "nothing found", 14);
                }
                else{
                    
                    /*write how much to read
                    total = total + (4*num_paths);
                    write(fd[1], &total, sizeof(int));*/
                    for(i = 0; i < num_paths; i++){
                        x = strlen(paths[i]);
                        write(fd[1], &x, sizeof(int));
                        write(fd[1], paths[i], strlen(paths[i]));
                        free(paths[i]);
                    }

                }

                x = 0;
                write(fd[1], &x, sizeof(int));
   
                close(fd[1]);

                kill(getppid(), SIGUSR1);

                /*reset ticket[id]*/
                ticket[id] = 0;

                exit(0);

            }
            else{
                for(i = 0; i < 10; i++){
                    if (child_info_arr[i].pid == 0){
                        child_info_arr[i].pid = pid;
                        child_info_arr[i].serial_num = i + 1;
                        strcpy(child_info_arr[i].job_info, original);
                        num_children++;
                        break;
                    }
                }
            }   
        }       
        else if (command1 != NULL && strcmp(command1, "list") == 0){
            /*list all children child_info_arr*/
            list_children();
        }

        else if (command1 != NULL && file != NULL && strcmp(command1, "kill") == 0){
            /*terminate certain child process*/
            /*(Example: findstuff$ kill 1) //kills child process number 1 (start counting at 1)*/

            /*file is the child number, turn to int*/
            int x = atoi(file);

            /*fprintf(stderr, "kill number: %d\n", x);*/
               
            for (i = 0; i < 10; i++){
                if (i == x - 1){
                    x = child_info_arr[i].pid;
                    /*remove from child_info_arr*/ 
                    child_info_arr[i].pid = 0;
                    break;
                }
            }

            /*fprintf(stderr, "kill pid: %d\n", x);*/

            kill(x, SIGKILL); /*terminates whole program*/
            waitpid(x, &status, 0);
        }

        /*handle signals if another not already beign handled*/
        if (*signal_received == 1){
            char res[1000];
            time_t start, end;
            int pid, i;
            int total_read = 1;

            /*solved clearing the buffer*/
            memset(res, '\0', 1000);

            read(fd[0], &pid, sizeof(int));

            for (i = 0; i < 10; i++){
                if (child_info_arr[i].pid == pid){
                    pid = i + 1;
                }
            }

            fprintf(stderr, "\nPaths found by child process %d:\n", pid);

            read(STDIN_FILENO, &start, sizeof(time_t));
            read(STDIN_FILENO, &end, sizeof(time_t));

            print_time(start, end);

            while(total_read != 0){
                read(STDIN_FILENO, &total_read, sizeof(int));
                if (total_read == 0){
                    break;
                }

                read(STDIN_FILENO, res, total_read);

                fprintf(stderr, "%s\n", res);

                memset(res, '\0', 1000);
            }

            

            /*child done, remove*/
            num_children--;

            *signal_received = 0; 
        }
        
        /*waitpid in loop*/
        for(i = 0; i < 10; i++){
            if (child_info_arr[i].pid != 0){
                int res = waitpid(child_info_arr[i].pid, &status, WNOHANG);
                if (res == -1){
                    perror("error in waitpid(). exiting\n");
                    munmap(ticket, sizeof(int)*10);
                    munmap(choosing, sizeof(int)*10);
                    munmap(child_info_arr, sizeof(child_info)*10);
                    munmap(signal_received, sizeof(int));
                    exit(EXIT_FAILURE);
                }
                else if (res == 0){
                    /*still running*/
                }
                else{
                    /*empty space in the table*/
                    child_info_arr[i].pid = 0;
                }
            }
        } 


        for (i = 0; i < 1000; i++){
            user_input[i] = 0;
        }

  

        dup2(d, STDIN_FILENO);
        
        cyan();
        fprintf(stderr, "findstuff$ ");
        default_color();

        read(STDIN_FILENO, user_input, 1000);
        user_input[strcspn(user_input, "\n")] = '\0';
    }
    
    close(fd[0]);

    munmap(ticket, sizeof(int)*10);
    munmap(choosing, sizeof(int)*10);
    munmap(child_info_arr, sizeof(child_info)*10);
    munmap(signal_received, sizeof(int));
    return 0;
}

void cyan(){
    fprintf(stderr,"\033[0;36m");
}

void default_color(){
    fprintf(stderr,"\033[0m");
}

void signal_handler(int signal){
    /*redirect STDIN*/
    dup2(fd[0], STDIN_FILENO);
    *signal_received = 1;
}

/*------------------------------------------------------------------------------------------------------*/
int find_file(char *file, char *search_dir, char **paths, int *num_paths, int sub_dir){
    /*sub_dir will act as a boolean for sub dir search or not*/
    /*
    Once the search is done, the child interrupts* the read/scanf from the parent process
    and prints its result.

    The result should be something like >nothing found< in case nothing was found or if
    successful, print the file path(s). Print several lines if several files with the same name
    were found
    */

    /*SKIP .. and . in the search*/

    struct dirent *entry;
    struct stat file_stats;
    DIR *dir;
    char path[256];
    int i, total;

    total = 0;

    dir = opendir(search_dir); /*open current directory*/
    if (dir == NULL){
        closedir(dir);
        perror("opendir() error. Exiting.\n");
        munmap(ticket, sizeof(int)*10);
        munmap(choosing, sizeof(int)*10);
        munmap(child_info_arr, sizeof(child_info)*10);
        munmap(signal_received, sizeof(int));
        exit(EXIT_FAILURE);
    }

    while((entry = readdir(dir)) != NULL){
        sprintf(path, "%s/%s", search_dir, entry->d_name);

        stat(path, &file_stats);
        
        if (strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name, "..") == 0){
        /*skip*/
            continue;
        }

        if (S_ISDIR(file_stats.st_mode) && sub_dir == 1){
            /*recursively search sub dir*/
            find_file(file, path, paths, num_paths, 0);
        }

        if(strcmp(entry->d_name, file) == 0){
            /*file found, add to array*/
            paths[*num_paths] = malloc(strlen(path) + 1);

            strcpy(paths[*num_paths], path);
            (*num_paths)++;
        }
    }

    closedir(dir);
    /*fprintf(stderr, "num_paths: %d\n", *num_paths);*/

    for (i = 0; i < *num_paths; i++){
        total += strlen(paths[i]) + 1;
    }

    return total;
}

int find_text(char *str, char *search_dir, char **paths, int *num_paths, char *file_ext, int sub_dir){
    /*check if you need to search subdirectories and/or specified file extensions*/
    char path[256];
    struct dirent *entry;
    struct stat file_stats;
    int i, total;
    char temp[1000];
    DIR *dir;

    /*fprintf(stderr, "str: %s\n", str);*/
    strcpy(temp, str);

    /*fprintf(stderr, "temp: %s\n", temp);*/

    dir = opendir(search_dir);


    /*fprintf(stderr, "str: %s\n", str);
    fprintf(stderr, "temp: %s\n", temp);*/

    if (dir == NULL){
        closedir(dir);
        perror("opendir() error in find_text. Exiting.\n");
        munmap(ticket, sizeof(int)*10);
        munmap(choosing, sizeof(int)*10);
        munmap(child_info_arr, sizeof(child_info)*10);
        munmap(signal_received, sizeof(int));
        exit(EXIT_FAILURE);
    }

    while((entry = readdir(dir)) != NULL){
        sprintf(path, "%s/%s", search_dir, entry->d_name);
        stat(path, &file_stats);
        
        if (strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name, "..") == 0){
        /*skip*/
            continue;
        }
        if (S_ISDIR(file_stats.st_mode) && sub_dir == 1){
            /*if at a directory and need to seach sub dir, recursion*/
            /*recursively search sub dir*/
            find_text(temp, path, paths, num_paths, file_ext, 0);
           
        }
        else if (S_ISREG(file_stats.st_mode)){
            char *cur_ext = NULL;
            char *line = NULL;
            size_t text_len = 0;
            int found_text = 0;
            /*open file*/
            FILE *file = fopen(path, "r");

            /*find text within files*/
            if (file_ext != NULL){
                cur_ext = strrchr(entry->d_name, '.');
                if (cur_ext != NULL && strcmp(cur_ext + 1, file_ext) != 0){
                    continue;
                }
            }   

            if (file == NULL){
                perror("error in fopen. exiting.\n");
                munmap(ticket, sizeof(int)*10);
                munmap(choosing, sizeof(int)*10);
                munmap(child_info_arr, sizeof(child_info)*10);
                munmap(signal_received, sizeof(int));
                exit(EXIT_FAILURE);
            }

            
            while (getline(&line, &text_len, file) != -1) {
                if (strstr(line, temp) != NULL) { /*find str inside line (needle in haystack)*/
                    found_text = 1;
                    break;
                }
         
            }

            free(line);
            fclose(file);

            if (found_text == 1){
                paths[*num_paths] = malloc(strlen(path) + 1);
                strcpy(paths[*num_paths], path);
                (*num_paths)++;
            }
        }
    }
    closedir(dir);
    /*i = (rand()%(20 - 10 + 1)) + 10;*/
    for (i = 0; i < *num_paths; i++){
        total += strlen(paths[i]) + 1;
    }

    return total;
}

void extract_input(char **command1_ptr, char **file_ptr, char **ext1_ptr, char **ext2_ptr, char *user_input){
    char concat[1000];
    char *temp = NULL;

    temp = strtok(user_input, " ");
    if (temp == NULL){
       *command1_ptr = NULL;
    }

    *command1_ptr = temp;

    temp = strtok(NULL, " ");

    if (temp == NULL){
        *file_ptr = NULL;
    }

    if (temp != NULL && temp[0] == '"' && temp[strlen(temp)-1] != '"'){
        /*at a quote*/
        while(temp[strlen(temp)-1] != '"'){
            strcat(concat, temp);
            strcat(concat, " ");
            temp = strtok(NULL, " ");
        }
        strcat(concat, temp);

        *file_ptr = concat;
    }
    else{
        *file_ptr = temp;
    }

    

    /*check if additional flags are present (up to 2: -f:ext, -s), start with "-"*/
    temp = strtok(NULL, " ");
    if (temp == NULL){
        *ext1_ptr = NULL;
    }

    *ext1_ptr = temp;

    temp = strtok(NULL, " ");
    if (temp == NULL){
        *ext2_ptr = NULL; 
    }

    *ext2_ptr = temp;

    

}

void list_children(){
    int i;
    for(i = 0; i < 10; i++){
        if (child_info_arr[i].pid != 0){
            fprintf(stderr, "Child process: %d | ", child_info_arr[i].serial_num);
            fprintf(stderr, "Job: %s | ", child_info_arr[i].job_info);
            fprintf(stderr, "PID: %d\n", child_info_arr[i].pid);
        }
    }
}

void kill_all(){
    int i;
    for (i = 0; i < 10; i++){
        if (child_info_arr[i].pid != 0){
            kill(child_info_arr[i].pid, SIGKILL);
        }
    }
}

int get_entry(int *ticket, int *choosing, int id){
    int max_ticket, ticket_val, i, other;

    /*id will be the pid, find in struct arr*/
    for (i = 0; i< 10; i++){
        if (child_info_arr[i].pid == id){
            id = i;
            break;
        }
    }

    choosing[id] = 1;

    __sync_synchronize();

    max_ticket = 0;

    for (i = 0; i < 10; i++){
        ticket_val = ticket[i];

        if (ticket_val > max_ticket){
            max_ticket = ticket_val;
        }
    }

    ticket[id] = max_ticket + 1;

    __sync_synchronize();
    choosing[id] = 0;
    __sync_synchronize();

    for (other = 0; other < 10; other++){
        while(choosing[other] == 1){}

        __sync_synchronize();

        while(ticket[other] != 0 && (ticket[other] < ticket[id] || (ticket[other] == ticket[id] && other < id))){}
    }

    return id;
}

/*from chat gpt*/
void print_time(time_t start, time_t end){

    /*Calculate the elapsed time in seconds*/
    double elapsedSeconds = difftime(end, start);

    /*Extract hours, minutes, seconds, and milliseconds*/
    int hours = (int)(elapsedSeconds / 3600);
    int minutes = (int)((elapsedSeconds - (hours * 3600)) / 60);
    int seconds = (int)(elapsedSeconds - (hours * 3600) - (minutes * 60));
    int milliseconds = (int)((elapsedSeconds - (int)elapsedSeconds) * 1000);

    /*Print the formatted elapsed time*/
    fprintf(stderr, "Time: %02d:%02d:%02d:%03d\n", hours, minutes, seconds, milliseconds);
}