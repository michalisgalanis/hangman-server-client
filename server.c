#include <util.h>

void handler_submitted(int sig){ // SIGUSR1 is used to inform server about submit
    printf("\n[Server %d]: Received Submit Signal from Client %ld.", getpid(), message->turn_id);
}

void handler_informed(int sig){ // SIGUSR2 is used to inform server about inform
    printf("\n[Server %d]: Received Informed Signal.", getpid());
}

bool stop_requested = false;
void handler_term(int sig){ // SIGTERM is used to terminate clients
    stop_requested = true;
    printf("\n[Server %d]: Received Termination Signal from Client %ld.", getpid(), message->turn_id);
}

int main(int argc, char *argv[]){ //input params: input_file
    // 0. Checking parameters
    if (argc != 3) perror_exit("usage: input players!");


    // 1. Create message queue and receive "hi" client requests. 
    key_t msg_key = ftok(FTOK_PATH, FTOK_PROJID); // create message queue identifier (key)
    if (msg_key == -1) perror_exit("Error creating key!");

    int msg_id = msgget(msg_key, 0666 | IPC_CREAT); // create message queue
    if (msg_id == -1) perror_exit("Erorr creating message queue!");

    int players = atoi(argv[2]);
    long player_ids[players];
    for (int i = 0; i < players; i++){ // server must receive #players x "hi" messages
        Greet init;
        int rcv_status = msgrcv(msg_id, &init, sizeof(init), MSG_GET_ANY, 0); // receive next client request
        if (rcv_status == -1) perror_exit("Error receiveing message!");

        if (strncmp(init.text, "hi", sizeof(init.text)) != 0){ // check if message is "hi"
            printf("[Server %d]: Received Message from Client %ld is : %s. Discarding player. \n", getpid(), init.type, init.text);
            players --;
            continue;
        }
        player_ids[i] = init.type;
        printf("[Server %d]: Received Request from Client %ld is : %s \n", getpid(), init.type, init.text);
    }
    
    printf("[Server %d]: Initial Phase Completed!\n", getpid());

    // 2. Select RANDOM word from dictionary text file
    // -- a. count number of lines in dictionary text file
    int lines = 0;
    FILE *dictionary_fp = fopen(argv[1], "r");
    if (!dictionary_fp) perror_exit("Error opening file!");

    for (char c = getc(dictionary_fp); c != EOF; c = getc(dictionary_fp))
        if (c == '\n') lines++;
    rewind(dictionary_fp);

    // -- b. extract random line from text file (read & trim)
    srand(time(NULL));
    int rand_index = rand() % lines;
    char rand_word_buffered[MAX_CHARS];

    int i = 0;
    while(fgets(rand_word_buffered, sizeof(rand_word_buffered), dictionary_fp)){
        if (i++ == rand_index) break;
    }
    fclose(dictionary_fp);

    // -- c. remove new line and whitespaces from selected word
    for (i = 0; i < MAX_CHARS; i++) if (rand_word_buffered[i] == 10) break;
    int real_length = i;
    for (; i < MAX_CHARS; i++) rand_word_buffered[i] = 0;
    //for (i = 0; i < MAX_CHARS; i++) printf("%d ", rand_word_buffered[i]);
    char word[real_length];
    for (i = 0; i < real_length; i++) word[i] = rand_word_buffered[i];
    word[real_length - 1] = 0;

    printf("[Server %d]: Random Word Selected: \"%s\"!\n", getpid(), word);
    

    // 3. Create shared memory segment and attach to it
    message = (Segment *)malloc(sizeof(Segment));

    key_t shm_key = ftok(FTOK_PATH, FTOK_PROJID); // create shared memory identifier (key)
    if (msg_key == -1) perror_exit("Error creating key!");

    int shm_id = shmget(shm_key, sizeof(Segment), 0666 | IPC_CREAT); // create shared memory segment
    if (msg_id == -1) perror_exit("Erorr creating shared memory segment!");

    Segment *segment = shmat(shm_id, NULL, 0); // attach to shared memory segment created
    if (segment == (void *) -1) perror_exit("Error attaching to shared memory segment!");
    message = segment;


    // 4. Inform clients about selected word information through message
    WordInfo info = {MSG_SERVER, getpid(), real_length, word[0], word[real_length - 2], ATTEMPTS_ALLOWED, shm_id};
    for (int i = 0; i < players; i++){
        int snd_status = msgsnd(msg_id, &info, sizeof(info), 0); //send message
        if (snd_status == -1) perror_exit("Error sending message!");
        sleep(0.001);
    }

    // 5. Create local word from client's perspective
    char word_public[info.chars];
    word_public[0] = info.first_char;
    word_public[info.chars - 2] = info.last_char;
    word_public[info.chars - 1] = 0;
    for (int i = 1; i < info.chars - 2; i++){
        word_public[i] = UNKNOWN_CHAR;
    }

    // ======================== GAME START ======================== 


    // 6. Setup parameters
    printf("\n[Server %d]: GAME START! =========================", getpid());
    int playing_idx = -1;
    int attempts_remaining = ATTEMPTS_ALLOWED;
    message->status = STATUS_INFORM;
    message->submit.letter = 0;
    message->results.attempts_remaining = attempts_remaining;
    for (int i = 0; i < MAX_CHARS; i++)
        message->results.letter_found_at[i] = false;
    message->results.letter_found_at[0] = true;
    message->results.letter_found_at[info.chars - 2] = true;
    message->results.total_letters_found = 2; 

    signal(SIGUSR1, handler_submitted);
    signal(SIGUSR2, handler_informed);

    
    // 7. Check submits and perform checks repeatedly until game is either won or lost
    while (1){
        if (stop_requested) break; // terminate

        if (message->status == STATUS_SUBMITTED && message->submit.letter != 0){ //check submit and perform checks
            printf("\n[Server %d]: Client %ld submitted letter \'%c\'.", getpid(), message->turn_id, message->submit.letter);

            // check matches with submitted letter
            int temp_letters_found = 0;
            for (int i = 0; i < real_length; i++){
                if (word[i] == message->submit.letter && !message->results.letter_found_at[i]){
                    message->results.letter_found_at[i] = true;
                    word_public[i] = message->submit.letter;
                    temp_letters_found++;
                }
            }

            message->results.total_letters_found += temp_letters_found;
            if (temp_letters_found > 0) printf("\n[Server %d]: Client %ld found %d new \'%c\' letters in selected word. %d Attempts remaining.", getpid(), message->turn_id, temp_letters_found, message->submit.letter, attempts_remaining);
            else {
                attempts_remaining--;
                message->results.attempts_remaining = attempts_remaining;
                printf("\n[Server %d]: Client %ld did not find any new \'%c\' letters in selected word. %d Attempts remaining.", getpid(), message->turn_id, message->submit.letter, attempts_remaining);
            }

            // Check if game should be terminated
            if (attempts_remaining == 0){ // check if lost
                for (int i = 0; i < players; i++){
                    message->status = STATUS_LOST;
                    int kill_status = kill(player_ids[playing_idx], SIGTERM);
                    if (kill_status == -1) perror_exit("Error sending signal to server!");
                }
                printf("\n\n[Server %d]: GAME LOST!", getpid());
                break;
            } else if (message->results.total_letters_found == real_length - 1){ // check if won
                for (int i = 0; i < players; i++){
                    message->status = STATUS_WON;
                    int kill_status = kill(player_ids[i], SIGTERM);
                    if (kill_status == -1) perror_exit("Error sending signal to server!");
                }
                printf("\n\n[Server %d]: GAME WON!", getpid());
                break;
            }

            // inform every player about word status
            message->status = STATUS_INFORM;
            for (int i = 0; i < players; i++){
                int kill_status = kill(player_ids[i], SIGUSR2);
                if (kill_status == -1) perror_exit("Error sending signal to client!");
                pause(); // wait for answer
            }

        } else if (message->status == STATUS_INFORM){
            // proceed to next turn and send signal to that client
            playing_idx = (playing_idx + 1) % players;
            message->turn_id = player_ids[playing_idx];
            printf("\n[Server %d]: Client %ld's turn!",getpid(), message->turn_id);
            message->status = STATUS_READY;

            int kill_status = kill(player_ids[playing_idx], SIGUSR1);
            if (kill_status == -1) perror_exit("Error sending signal to client!");
            pause();
        }
    }


    // 7. Cleanup
    sleep(0.5); // give some time to clients to terminate

    int detach_status = shmdt(segment); // detach from shared memory segment
    if (detach_status == -1) perror_exit("Error detaching from shared memory segment!");

    int msgctl_status = msgctl(msg_id, IPC_RMID, NULL); //destroy the message queue
    if (msgctl_status == -1) perror_exit("Error destroying the message queue!");

    printf("[Client %d]: Terminating!", getpid());
    return 0;
}