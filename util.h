#include <stdio.h>                  
#include <stdlib.h>                 // rand(), srand() support
#include <unistd.h>                 // getpid() support
#include <string.h>                 // strcmp() support
#include <time.h>                   // time() support
#include <signal.h>                 // signal(), SIGTERM, SIGUSR1, SIGUSR2 support
#include <sys/ipc.h>                
#include <sys/msg.h>                // message_queue support
#include <sys/shm.h>                // shared memory support
#include <stdbool.h>                // boolean variables support

// Dictionary Defines
#define MAX_CHARS 20                // max characters of word selected from dictionary

#define ATTEMPTS_ALLOWED 8          // total attemps allowed (vanilla game)
#define UNKNOWN_CHAR '*'            // used for displaying word info

// General Values
#define FTOK_PATH "dictionary.txt"
#define FTOK_PROJID 1

#define MSG_GET_ANY 0               // Read any new message from message queue
#define MSG_SERVER 1                // Read next message with type=1

#define STATUS_READY 1              // Client can now submit a letter
#define STATUS_SUBMITTED 2          // Client has just submitted a letter
#define STATUS_INFORM 3             // All Clients are informed about word status
#define STATUS_WON -1               // All Clients won!
#define STATUS_LOST -2              // All Clients lost!

// Structures for Message Queue
typedef struct msg_greet {          /* Used for Client's "hi" request */
    long type;                      // message type (Client Process ID)
    char text[3];                   // message text
} Greet;

typedef struct msg_word_info {      /* Used for Server's word info response */
    long type;                      // message type (= 1)
    long server_id;                 // pid of server process
    int chars;                      // character count of selected word
    char first_char;                // first character of selected word
    char last_char;                 // last character of selected word
    int attempts_allowed;           // number of total attempts allowed before losing
    key_t shm_id;                   // shared memory key identifier
} WordInfo;


// Structures for Shared Memory

typedef struct shm_letter_submit {  /* Used by Client to submit a new letter */
    char letter;                    // new letter guess by client
} LetterSubmit;

typedef struct shm_letter_results { /* Used by Server to respond about letter results */
    bool letter_found_at[MAX_CHARS];// indexes of found letters of selected word
    int attempts_remaining;         // attempts remaining
    int total_letters_found;        // total found letters of selected word
} LetterResults;

typedef struct shm_segment {        /* Used by Server and Client to communicated using shared memory */
    LetterSubmit submit;            // used by client to submit a new letter
    LetterResults results;          // used by server to respond about letter results
    long turn_id;                   // which player is playing (process ID)
    int status;                     // used to control flow (ready / not ready / submitted)
} Segment;
Segment *message;

// Functions

/**
 * Prints information and exits the program
 */
void perror_exit(const char *message){
    perror(message);
    exit(-1);
}