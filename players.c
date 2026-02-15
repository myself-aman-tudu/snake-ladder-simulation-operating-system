/*
 * Snake Ludo – Operating Systems Lab Assignment
 *
 * Author  : Aman Tudu
 * GitHub  : https://github.com/myself-aman-tudu
 *
 * ------------------------------------------------------------
 * 
 * players.c
 * Player-Parent Process (PP) and individual player processes.
 * Handles turn sequencing, dice rolls, movement logic, and
 * communication with the board process.
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
pid_t bp_pid;
int current_player = 0;
pid_t player_pids[100];
int pipefd_write;

void player_process(int player_id);

void handle_next_move(int sig){
    int attempts = 0;
    while(position[current_player] == 100 && attempts < no_player){
        current_player = (current_player + 1) % no_player;
        attempts++;
    }
    if(attempts >= no_player){
        return;
    }
    kill(player_pids[current_player], SIGUSR1);
    current_player = (current_player + 1) % no_player;
}

void handle_end_game(int sig){
    printf("Players parent: Terminating all players\n");
    
    // send SIGUSR2 to all players still in game
    for(int i = 0; i < no_player; i++){
        if(player_pids[i] > 0){
            kill(player_pids[i], SIGUSR2);
        }
    }
    for(int i = 0; i < no_player; i++){
        if(player_pids[i] > 0){
            waitpid(player_pids[i], NULL, 0);
            printf("  Player %c terminated\n", 'A' + i);
            sleep(1);
        }
    }
    
    // detach shared memory
    shmdt(board);
    shmdt(position);

    printf("\nPlayers parent terminating...\n");
    exit(0);
}

volatile sig_atomic_t player_turn = 0;

void player_handle_turn(int sig){
    player_turn = 1;
}

void player_handle_termination(int sig){
    shmdt(board);
    shmdt(position);
    exit(0);
}

void player_process(int player_id){
    srand(time(NULL) ^ (getpid() << 16)); // separate seed for random dice rolls
    
    signal(SIGUSR1, player_handle_turn);
    signal(SIGUSR2, player_handle_termination);
    
    char player_name = 'A' + player_id;
    
    printf("  Player %c initialized (PID: %d)\n", player_name, getpid());
    
    while(1){
        pause();
        
        if(!player_turn) continue;
        player_turn = 0;
        int pos = position[player_id];
        
        if(pos == 100){
            continue;
        }
        
        int total = 0;
        int rolls[3];
        int num_rolls = 0;
        
        printf("\n*************************************\n");
        printf("Player %c (at %d): ", player_name, pos);
        
        while(1){
            int roll = (rand() % 6) + 1;
            rolls[num_rolls] = roll;
            num_rolls++;
            total += roll;
            printf("%d", roll);
            
            if(roll != 6){
                break;
            }
            
            if(num_rolls >= 3){
                break;
            }
            
            printf("+");
        }
        
        // three 6s
        if(num_rolls == 3 && rolls[0] == 6 && rolls[1] == 6 && rolls[2] == 6){
            printf(" Cancelled\n");
            printf("Player %c stays at cell %d\n", player_name, pos);
            kill(bp_pid, SIGUSR1);
            continue;
        }
        printf("\n");
        
        int nextpos = pos + total;
        
        // snakes and ladders
        int moved_by_snake_ladder = 0;
        while(nextpos <= 100 && board[nextpos] != 0){
            int jump = board[nextpos];
            if(jump > 0){
                printf("Ladder at cell %d Jump to %d\n", nextpos, nextpos + jump);
            } else {
                printf("Snake at cell %d Jump to %d\n", nextpos, nextpos + jump);
            }
            nextpos += jump;
            moved_by_snake_ladder = 1;
        }
        
        if(nextpos > 100){
            printf("Move not permitted (cannot go beyond 100), stays at %d\n", pos);
            kill(bp_pid, SIGUSR1);
            continue;
        }
        
        // check if cell is occupied
        int occupied = 0;
        char occupied_by;
        for(int p = 0; p < no_player; p++){
            if(p != player_id && position[p] == nextpos && nextpos != 100){
                occupied = 1;
                occupied_by = ('A' + p);
                break;
            }
        }
        
        if(occupied){
            printf("Move not permitted (cell already occupied by %c), stays at %d\n", occupied_by, pos);
            kill(bp_pid, SIGUSR1);
            continue;
        }
        
        position[player_id] = nextpos;
        
        if(nextpos == 100){
            printf("\t\t\tPlayer %c exits with rank = %d\n", player_name, 1+no_player-position[no_player]);
            position[no_player]--;
            kill(bp_pid, SIGUSR1);   
            shmdt(board);
            shmdt(position);
            exit(0);
        } else {
            if(moved_by_snake_ladder){
                printf("Player %c moves to cell %d\n", player_name, nextpos);
            } else {
                printf("Player %c moves to cell %d\n", player_name, nextpos);
            }
            
            kill(bp_pid, SIGUSR1);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <no_players> <pipe_fd> <bp_pid>\n", argv[0]);
        exit(1);
    }
    
    no_player = atoi(argv[1]);
    pipefd_write = atoi(argv[2]);
    bp_pid = atoi(argv[3]);
    
    printf("=====   Players Parent Process (PP)  =====\n");
    printf("Number of players: %d\n", no_player);
    
    // Attach to shared memory
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
    
    printf("PP PID: %d\n", getpid());
    printf("BP PID: %d\n\n", bp_pid);
    
    printf("Forking player processes...\n");
    for(int i = 0; i < no_player; i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("fork player");
            exit(1);
        }
        if(pid == 0){
            player_process(i);
            exit(0);
        }
        player_pids[i] = pid;
    }
    
    printf("\nAll %d players forked successfully!\n\n", no_player);
    
    signal(SIGUSR1, handle_next_move);
    signal(SIGUSR2, handle_end_game);
    
    printf("Players ready. Waiting for game commands...\n\n");
    printf("════════════════════════════════════════\n");
    
    // wait for signals
    while(1){
        pause();
    }
    
    return 0;
}