/*
 * Snake Ludo – Operating Systems Lab Assignment
 *
 * Author  : Aman Tudu
 * GitHub  : https://github.com/myself-aman-tudu
 *
 * ------------------------------------------------------------
 * 
 * board.c
 * Board Process (BP).
 * Attaches to shared memory and displays the current board
 * state after each player move.
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

int *board;
int *position;
int no_player;
int pipefd_write;
volatile sig_atomic_t should_print = 0;

void print_board() {
    printf("\033[2J\033[H");   // Clear screen

    printf("========== SNAKE LUDO ==========\n\n");
    printf("Snakes:\n");
    int count = 0;
    for (int i = 100; i >= 1; i--) {

        if (board[i] < 0) {

            int dest = i + board[i];

            printf("S(%2d->%2d)", i, dest);
            count++;

            if (count % 3 == 0) {
                printf("\n");
            } else {
                printf("   ||   ");
            }
        }
    }
    if (count == 0) {
        printf("None");
    }
    printf("\n\nLadders:\n");
    count = 0;
    for (int i = 1; i <= 100; i++) {
        if (board[i] > 0) {
            int dest = i + board[i];
            printf("L(%2d->%2d)", i, dest);
            count++;
            if (count % 3 == 0) {
                printf("\n");
            } else {
                printf("   ||   ");
            }
        }
    }
    if (count == 0) {
        printf("None");
    }
    printf("\n\n");

    for (int row = 9; row >= 0; row--) {
        for (int col = 0; col < 10; col++) {
            int cell;
            if(row % 2){
                cell = row * 10 + (10 - col);
            }
            else{
                cell = row * 10 + (col + 1);
            }
            char player_char = ' ';
            for(int p = 0; p < no_player; p++) {
                if (position[p] == cell) {
                    player_char = 'A' + p;
                    break;
                }
            }
            if(player_char != ' '){
                printf("   %c", player_char);
            }else{
                printf("%4d", cell);
            }
        }
        printf("\n");
    }

    printf("\nFinished: ");
    int found = 0;
    for (int p = 0; p < no_player; p++) {
        if (position[p] == 100) {
            printf("%c ", 'A' + p);
            found = 1;
        }
    }
    if (!found) printf("none");

    printf("\nHome: ");
    found = 0;
    for (int p = 0; p < no_player; p++) {
        if (position[p] == 0) {
            printf("%c ", 'A' + p);
            found = 1;
        }
    }
    if (!found) printf("none");
    printf("\nPlayers remaining: %d\n", position[no_player]);
}

void handle_print_request(int sig){
    should_print = 1;
}

void handle_termination(int sig){
    printf("\nBoard process terminating...\n");
    shmdt(board);
    shmdt(position);
    exit(0);
}

int main(int argc, char* argv[]){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <no_players> <pipe_fd>\n", argv[0]);
        exit(1);
    }
    
    no_player = atoi(argv[1]);
    pipefd_write = atoi(argv[2]);
    
    printf("Board process starting (PID: %d)\n", getpid());
    printf("Players: %d\n", no_player);
    
    // attach to shared memory
    key_t KEY1 = ftok("./ludo.c", 1);
    key_t KEY2 = ftok("./ludo.c", 2);
    
    int board_id = shmget((key_t)KEY1, 101*sizeof(int), 0777);
    if(board_id < 0){
        perror("shmget board");
        fprintf(stderr, "Make sure ./ludo is running!\n");
        exit(1);
    }
    board = (int *)shmat(board_id, NULL, 0);
    
    int player_pos_id = shmget((key_t)KEY2, (no_player+1)*sizeof(int), 0777);
    if(player_pos_id < 0){
        perror("shmget position");
        exit(1);
    }
    position = (int *)shmat(player_pos_id, NULL, 0);
    
    char buffer[20];
    sprintf(buffer, "%d", getpid());
    write(pipefd_write, buffer, 20);
    
    sleep(1);
    
    // signal handlers
    signal(SIGUSR1, handle_print_request);
    signal(SIGUSR2, handle_termination);
    
    print_board();
    
    // acknowledgment
    char ack = 'A';
    write(pipefd_write, &ack, 1);
    
    printf("\nBoard ready. Waiting for game updates...\n\n");
    
    // Main loop
    while(1){
        pause();
        
        if(should_print){
            should_print = 0;
            print_board();
            
            // sending acknowledgment
            write(pipefd_write, &ack, 1);
        }
    }
    
    return 0;
}