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

#define PUBLIC      "/tmp/M_FIFO"
#define CLOSE_MESSAGE "Zamkniecie serwera"


#define WARNING "warning"
#define wiadomosc "Wiadomosc: "
#define LOGOUT "Wylogowano"

typedef struct user {
    char username[100];
    char download_path[100];
    char fifo_path[100];
} User;