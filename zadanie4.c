#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <pthread.h>
#include <syslog.h>
#include <signal.h>
#include <linux/limits.h>
#include <dirent.h>
#include <signal.h>
#include "dane.h"
// Tworzymy funkcje serwer demon dzialajacego w tle
// Tworzymy klienta poprzez przeslanie informacji do M_FIFO w ustalonej formie
// Za pomoca polecenia kill mozemy wyslac sygnaly do dziedzicznych procesow
// getppid proces ojca
// Wysylamy wiadomosci do potoku serwera o logowaniu w ustalonej formie
// Serwer pozniej sie komunikuje z fifo1 klienta(klient nie widzi tego fifo, tylko go nasluchuje)
// Klient tworzy podproces ktory slucha ten potok czy ktos cos napisal

int sygnal;
int fd, fd_server;
User user;
int index;
pid_t pid;

void odbierz_sygnal(int signum)
{
    sygnal = 1;
}

void send_to_log(char *info) // info to zmienna tekstowa do zapisywania w logach
{
    time_t t = time(NULL);
    struct tm date = *localtime(&t);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%d-%d-%d %d:%d:%d - %s", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, info);
    openlog("projekt", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, out, getuid());
    closelog();
}

void daemon()
{
    pid_t pid;
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (pid != 0)
    {
        exit(0);
    }
    umask(0);
    signal(SIGHUP, SIG_IGN);
    if (setsid() == -1)
    {
        exit(1);
    }
    if (chdir("/") == -1)
    {
        perror("chdir");
        exit(1);
    }
    for (int i = 0; i < NR_OPEN; i++)
        close(i);
    open("/dev/null", O_RDWR); // stdin
    dup(0);                    // stdout
    dup(0);                    // stderror
}

void handler(int sig)
{
    send_to_log("Wylogowano");
    kill(pid, SIGKILL);
    // termination here
    exit(0);
}

int main(int argc, char **argv)
{
    struct sigaction akcja;
    sigset_t sygnaly;
    sigemptyset(&sygnaly);
    sigaddset(&sygnaly, SIGQUIT);
    sigaddset(&sygnal, SIGKILL);
    akcja.sa_handler = odbierz_sygnal;
    akcja.sa_flags = 0;
    if (sigaction(SIGQUIT, &akcja, NULL))
        exit(1);
    // sigprocmask(SIG_BLOCK, &sygnaly, NULL);
    if (sigaction(SIGQUIT, &akcja, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
    char return_fifo[20][100];
    int licznik = 0;
    if (argc < 2)
    {
        printf("Opcje logowania: %s [--start | --login <nazwa_uzytkownika> | --download <sciezka_do_katalogu]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    User user;
    Message message;
    int bytes_read;
    int bufor[4096];
    if (!strcmp(argv[1], "--start"))
    {
        daemon();
        int fd_server;
        if (mkfifo(PUBLIC, 0666) < 0)
        {
            if (errno != EEXIST)
            {
                perror("EEXIST");
            }
            else
            {
                perror(PUBLIC);
            }
            return -1;
        }
        if ((fd_server = open(PUBLIC, O_RDWR)) == -1)
        {
            perror("fd PUBLIC");
            return -1;
        }
        while (1)
        {
            for (int i = 0; i < 20; i++)
            {
                strcpy(return_fifo[i], "");
            }
            memset(bufor, 0x0, sizeof(bufor));
            if ((bytes_read = read(fd_server, bufor, sizeof(bufor))) == -1)
            {
                perror("bytes_read_server");
                exit(1);
            }
            if (bytes_read > 0)
            {
                // Podzial stringa na mniejsze porcje
                char *token;
                char *rest = bufor;
                int i = 0;
                while ((token = strtok_r(rest, " ", &rest)) && strlen(token) != 0)
                {
                    strcpy(return_fifo[i], token);
                    i++;
                }
            }
            if (!strcmp(return_fifo[0], "login"))
            {
                for (int i = 0; i < 20; i++)
                {
                    if (!strcmp(return_fifo[1], user.nazwa_uzytkownika[i]) && user.nazwa_uzytkownika[i] != "")
                        break;
                    else
                        index = i;
                }
                strncpy(user.nazwa_uzytkownika[index], return_fifo[2], sizeof(return_fifo[2]));
            }
            else if (!strcmp(return_fifo[0], "send"))
            {
                char adres_nadawcy[4096];
                strncpy(message.odbiorca[index], return_fifo[2], sizeof(return_fifo[2]));
                syslog(LOG_INFO, "Pomyslnie utworzono fifo_klienta o nazwie: %s", message.odbiorca[index]);
                for (int i = 3; i < 20; i++)
                {
                    if (strlen(return_fifo[i]) != 0)
                    {
                        strcat(message.message, return_fifo[i]);
                        strcat(message.message, " ");
                    }
                }
                char string[1024];
                send_to_log("Odbiorca: ");
                send_to_log(message.odbiorca[index]);
                send_to_log("Wiadomosc: ");
                send_to_log(message.message);
                char dir[1024];
                sprintf(dir, "/home/kali/Desktop/Komunikator/%s", message.odbiorca[index]);
                strcat(string, "<Od: ");
                strcat(string, return_fifo[1]);
                send_to_log("return_fifo[1]: ");
                send_to_log(return_fifo[1]);
                strcat(string, " : ");
                strcat(string, message.message);
                send_to_log(string);
                send_to_log(dir);
                send_to_log(message.message);
                if ((fd = open(dir, O_WRONLY)) == -1)
                {
                    perror("fd_open");
                    exit(1);
                }
                if ((write(fd, string, sizeof(string))) == -1)
                {
                    perror("write");
                    exit(1);
                }
                memset(string, 0x0, sizeof(string));
                send_to_log("koniec");
            }
            memset(message.message, 0x0, sizeof(message.message));
        }
    }
    // FUNCKJA READ ZCZYTAJACA WSZYSTKIE WIADOMOSCI
    else if (!strcmp(argv[1], "--login"))
    {
        char user_[HALFPIPE_BUF];
        char path[HALFPIPE_BUF];
        char newstring[1024];
        if (argc > 3)
        {
            if (!strcmp(argv[3], "--download"))
                strncpy(path, argv[4], sizeof(argv[4]));
            else
                strncpy(path, "", sizeof(""));
        }
        strcat(newstring, " login ");
        strcat(newstring, argv[2]);
        send_to_log(newstring);
        fd_server = open(PUBLIC, O_WRONLY);
        if (fd_server == -1)
        {
            perror("fd_server");
            return -1;
        }
        if (write(fd_server, newstring, sizeof(newstring)) == -1)
        {
            perror("write_user");
            exit(1);
        }
        int child_status;
        pid_t pid;
        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid == 0)
        {
            char dir[1024];
            sprintf(dir, "/home/kali/Desktop/Komunikator/%s", argv[2]);
            if (mkfifo(dir, 0666) == -1)
            {
                perror("user_mkfifo");
                exit(1);
            }
            if ((fd = open(dir, O_RDONLY)) == -1)
            {
                perror("user_fifo_open");
                exit(1);
            }
            memset(bufor, 0x0, sizeof(bufor));
            while ((bytes_read = read(fd, bufor, sizeof(bufor))) > 0)
            {
                write(fileno(stdout), bufor, bytes_read);
            }
        }
        else if (pid > 0)
        {
            while (1)
            {
                signal(SIGQUIT, handler);
                memset(bufor, 0x0, sizeof(bufor));
                write(fileno(stdout), wiadomosc, sizeof(wiadomosc));
                fgets(bufor, sizeof(bufor), stdin);
                strncpy(message.message, bufor, HALFPIPE_BUF - 1);
                message.message[HALFPIPE_BUF - 1] = '\0';
                write(fd_server, message.message, sizeof(message.message));
            }
        }
    }
}