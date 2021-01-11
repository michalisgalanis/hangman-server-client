#include <util.h>

void perror_exit(const char *message);

void handler_turn(int sig){ // SIGUSR1 is used to inform client about it's turn
    printf("\n[Client %d]: Received My Turn Signal from Server.", getpid());
}

void handler_results(int sig){ // SIGUSR2 is used to inform client about word result
    printf("\n[Client %d]: Received Results Signal from Server.", getpid());
}

bool stop_requested = false;
void handler_term(int sig){ // SIGTERM is used to terminate clients
    stop_requested = true;
    if (message->status == STATUS_WON) printf("[Client %d]: GAME WON!", getpid());
    else if (message->status == STATUS_LOST) printf("[Client %d]: GAME LOST", getpid());
    else printf("\n[Client %d]: Received Termination Signal from Server.", getpid());
}

int main(int argc, char *argv[]){ // no input params
    // 0. Checking Parameters
    if (argc != 1) perror_exit("No input parameters needed!");

    // 1. Connect to Message Queue and send a "hi" request to server
    key_t msg_key = ftok(FTOK_PATH, FTOK_PROJID); // create message queue identifier (key)
    if (msg_key == -1) perror_exit("Error creating key!");

    int msg_id = msgget(msg_key, 0666 | IPC_CREAT); // create message queue
    if (msg_id == -1) perror_exit("Erorr creating message queue!");

    Greet init = {getpid(), "hi"}; // create message and send it
    int snd_status = msgsnd(msg_id, &init, sizeof(init), 0); //send message
    if (snd_status == -1) perror_exit("Error sending message!");

    printf("[Client %d]: Sent message is : %s \n", getpid(), init.text);

    
    // 2. Receive word info from server
    WordInfo info;
    int rcv_status = msgrcv(msg_id, &info, sizeof(info), MSG_SERVER, 0);
    if (rcv_status == -1) perror_exit("Error receiveing message!");
    
    char word[info.chars + 1];
    word[0] = info.first_char;
    word[info.chars - 1] = info.last_char;
    word[info.chars] = 0;
    for (int i = 1; i < info.chars - 1; i++){
        word[i] = UNKNOWN_CHAR;
    }
    printf("[Client %d]: Word received from server is \"%s\" (%d Attemps allowed)! \n", getpid(), word, info.attempts_allowed);

    // 3. Connect to shared memory segment
    Segment *segment = shmat(info.shm_id, NULL, 0); // attach to shared memory segment created
    if (segment == (void *) -1) perror_exit("Error attaching to shared memory segment!");
    message = segment;


    // ======================== GAME START ======================== 


    // 4. Setup, submit a letter and check results repeatedly until game is either won or lost
    printf("\n[Client %d]: GAME START! =========================", getpid());
    bool letter_found_at[MAX_CHARS];
    letter_found_at[0] = true;
    letter_found_at[info.chars - 1] = true;
    
    signal(SIGUSR1, handler_turn);
    signal(SIGUSR2, handler_results);
    signal(SIGTERM, handler_term);
    
    while (1){
        pause(); // wait for any signal (for my turn)
        if (stop_requested) break; // terminate

        if (!message) continue; // message needs to be allocated by server

        if (message->status == STATUS_READY && message->turn_id == getpid()){
            // it's my turn. ask for new letter and submit
            printf("\n\n[Client %d]: MY TURN! =========================", getpid());
            printf("\n[Client %d]: \"%s\" (%d letters found, %d attempts remaining)", getpid(), word, message->results.total_letters_found, message->results.attempts_remaining);
            printf("\n[Client %d]: Enter new letter: ", getpid());
            message->submit.letter = getc(stdin);
            message->status = STATUS_SUBMITTED;

            int kill_status = kill(info.server_id, SIGUSR1);
            if (kill_status == -1) perror_exit("Error sending signal to server!");

        } else if (message->status == STATUS_INFORM){
            // check results and make changes to local word
            printf("\n[Client %d]: Client %ld submitted letter \'%c\'.", getpid(), message->turn_id, message->submit.letter);
            for (int i = 0; i < info.chars; i++){
                    if (message->results.letter_found_at[i] != letter_found_at[i]){
                        word[i] = message->submit.letter;
                        letter_found_at[i] = message->results.letter_found_at[i];
                    }
            }
            
            printf("\n[Client %d]: Client %ld results: \"%s\" (%d letters found, %d attempts remaining)", getpid(), message->turn_id, word, message->results.total_letters_found, message->results.attempts_remaining);
            int kill_status = kill(info.server_id, SIGUSR2);
            if (kill_status == -1) perror_exit("Error sending signal to server!");
        }
    }


    // 5. Cleanup
    int detach_status = shmdt(segment); // detach from shared memory segment
    if (detach_status == -1) perror_exit("Error detaching from shared memory segment!");

    printf("\n[Client %d]: Terminating!", getpid());
    return 0;
}