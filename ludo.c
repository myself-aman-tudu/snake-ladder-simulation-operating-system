/*
 * Snake Ludo – Operating Systems Lab Assignment
 *
 * Author  : Aman Tudu
 * GitHub  : https://github.com/myself-aman-tudu
 *
 * ------------------------------------------------------------
 * 
 * ludo.c
 * Coordinator Process (CP).
 * Creates and initializes shared memory, launches the board
 * and players processes via xterm, and manages overall game
 * synchronization and termination.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

int board_id = -1;
int player_pos_id = -1;
pid_t bp_pid = 0, pp_pid = 0;
int *position;

void set_board(int *board){
    FILE* fp = fopen("ludo.txt", "r");
    if(fp == NULL){
        printf("Could not open the file ludo.txt\n");
        exit(1);
    }
    char snake_ladder;
    int from, to;
    for(int i=0; i<101; i++) board[i] = 0;
    while((snake_ladder = fgetc(fp)) != EOF){
        if(snake_ladder == 'E'){
            break;
        }else if(snake_ladder == 'S' || snake_ladder == 'L'){
            fscanf(fp, "%d %d", &from, &to);
            board[from] = to - from;
        }
    }
    fclose(fp);
}

void init_position(int* position, int no_player){
    for(int i=0; i<no_player; i++){
        position[i] = 0;
    }
    position[no_player] = no_player;
}

void cleanup_handler(int sig) {
    printf("\nCleaning up shared memory...\n");
    if (board_id != -1) {
        shmctl(board_id, IPC_RMID, NULL);
    }
    if (player_pos_id != -1) {
        shmctl(player_pos_id, IPC_RMID, NULL);
    }
    exit(0);
}

int main(int argc, char* argv[]){
    signal(SIGINT, cleanup_handler);
    
    int no_player;
    if(argc == 1){
        no_player = 4;
    }else{
        no_player = atoi(argv[1]);
    }

    // shared memory
    key_t KEY1 = ftok("./ludo.c", 1);
    key_t KEY2 = ftok("./ludo.c", 2);
    
    board_id = shmget((key_t)KEY1, 101*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);
    if(board_id < 0){
        perror("shmget board - shared memory may already exist");
        printf("Run 'ipcrm -a' or 'make clean' to remove old shared memory\n");
        exit(1);
    }
    int *board = (int *)shmat(board_id, NULL, 0);

    player_pos_id = shmget((key_t)KEY2, (no_player+1)*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);
    if(player_pos_id < 0){
        perror("shmget position");
        shmctl(board_id, IPC_RMID, NULL);
        exit(1);
    }
    position = (int *)shmat(player_pos_id, NULL, 0);

    // pipe
    int pipefd[2];
    if(pipe(pipefd) < 0){
        perror("pipe");
        exit(1);
    }

    set_board(board);
    init_position(position, no_player);

    pid_t xbp_pid, xpp_pid;

    // fork: XBP 
    xbp_pid = fork();
    if(xbp_pid < 0){
        perror("fork XBP");
        exit(1);
    }
    if(xbp_pid == 0){
        close(pipefd[0]);
        
        char narg[10], pfdarg[10];
        sprintf(narg, "%d", no_player);
        sprintf(pfdarg, "%d", pipefd[1]);
        
        execlp("xterm", "xterm", "-T", "Board", "-fa", "Monospace", "-fs", "14", 
               "-geometry", "80x40+50+50", "-bg", "#003300", "-fg", "white",
               "-e", "./board", narg, pfdarg, NULL);
        
        perror("execlp board failed");
        exit(1);
    }

    char bp_pid_str[20];
    ssize_t n = read(pipefd[0], bp_pid_str, sizeof(bp_pid_str));
    if(n <= 0){
        perror("Failed to read BP PID");
        exit(1);
    }
    bp_pid = atoi(bp_pid_str);
    printf("CP: Received BP PID: %d\n", bp_pid);

    // fork: XPP
    xpp_pid = fork();
    if(xpp_pid < 0){
        perror("fork XPP");
        exit(1);
    }
    if(xpp_pid == 0){
        close(pipefd[0]);
        
        char narg[10], pfdarg[10], bparg[10];
        sprintf(narg, "%d", no_player);
        sprintf(pfdarg, "%d", pipefd[1]);
        sprintf(bparg, "%d", bp_pid);
        
        execlp("xterm", "xterm", "-T", "Players", "-fa", "Monospace", "-fs", "14",
               "-geometry", "80x40+750+50", "-bg", "#000033", "-fg", "white",
               "-e", "./players", narg, pfdarg, bparg, NULL);
        
        perror("execlp players failed");
        exit(1);
    }
    close(pipefd[1]);

    printf("Waiting for Players to initialize...\n");

    // get PP pid
    char pp_pid_str[20];
    n = read(pipefd[0], pp_pid_str, sizeof(pp_pid_str));
    if(n <= 0){
        perror("Failed to read PP PID");
        exit(1);
    }
    pp_pid = atoi(pp_pid_str);
    printf("CP: Received PP PID: %d\n", pp_pid);

    // wait for acknowledgment from BP
    char ack;
    n = read(pipefd[0], &ack, 1);
    if(n <= 0){
        perror("Failed to read BP acknowledgment");
        exit(1);
    }
    printf("CP: Board initialized\n\n");

    // main game loop
    printf("=== Snake Ludo Game Started ===\n");
    printf("Commands:\n");
    printf("\tnext     - make next move\n");
    printf("\tdelay N  - set delay to N ms (for autoplay)\n");
    printf("\tautoplay - start autoplay mode\n");
    printf("\tquit     - End game\n\n");
    printf("\t*** Once autoplay starts, the game cannot be quit manually.\n");
    printf("\t*** The delay cannot be changed after autoplay begins.\n\n");
    char command[100];
    int delay_ms = 1000;
    int autoplay = 0;

    while(1){
        if(!autoplay){
            printf("> ");
            fflush(stdout);
            if(scanf("%s", command) != 1){
                break;
            }

            if(strcmp(command, "quit") == 0){
                break;
            } else if(strcmp(command, "autoplay") == 0){
                autoplay = 1;
                printf("Starting autoplay (delay: %dms)...\n", delay_ms);
            } else if(strcmp(command, "delay") == 0){
                scanf("%d", &delay_ms);
                printf("Delay set to %dms\n", delay_ms);
                continue;
            } else if(strcmp(command, "next") != 0){
                printf("Unknown command\n");
                continue;
            }
        } else {
            usleep(delay_ms * 1000);
        }

        // check if game over
        if(position[no_player] == 0){
            printf("\nGAME OVER! All players finished!\n");
            break;
        }

        // SIGUSR1 to PP
        kill(pp_pid, SIGUSR1);

        n = read(pipefd[0], &ack, 1);
        if(n <= 0){
            printf("Communication error with BP\n");
            break;
        }
    }

    printf("\nPress Enter to exit...\n");
    while(getchar() != '\n'); // clear buffer
    getchar();

    printf("\nTerminating processes...\n");
    
    kill(pp_pid, SIGUSR2);
    waitpid(xpp_pid, NULL, 0);
    printf("Players process terminated\n");

    kill(bp_pid, SIGUSR2);
    
    waitpid(xbp_pid, NULL, 0);
    printf("Board process terminated\n");

    // cleanup shared memory
    shmdt(board);
    shmdt(position);
    shmctl(board_id, IPC_RMID, NULL);
    shmctl(player_pos_id, IPC_RMID, NULL);
    close(pipefd[0]);

    printf("Cleanup complete. Game Ends here.\n");
    return 0;
}