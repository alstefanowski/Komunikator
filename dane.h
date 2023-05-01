#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <linux/limits.h>
#include <syslog.h>
#include <errno.h>

#define PUBLIC      "/home/kali/Desktop/Komunikator/M_FIFO"
#define PRIVATE     "/home/kali/Projekt/FIFO_CLIENT"
#define HALFPIPE_BUF (PIPE_BUF/2)
#define MAXTRIES 5
#define WARNING "warning"
#define MAX_USERS 100
#define SHM_KEY 12345
#define wiadomosc "Wiadomosc: "

typedef struct message {
    char message[HALFPIPE_BUF];
    char odbiorca[20][100];
    char nadawca[20][100];
    char fifo_name[20][100];
    int pid_id;
} Message;


typedef struct user {
    char nazwa_uzytkownika[20][100];
    char operation[1024];
    char fifo[20][100];
    char path[20][100];
} User;