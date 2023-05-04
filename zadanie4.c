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

int sig;

void signal_handler(int signum)
{
    sig = signum;
}

void send_to_log(char *info)
{
    time_t t = time(NULL);
    struct tm date = *localtime(&t);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%d-%d-%d %d:%d:%d - %s", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, info);
    openlog("zadanie4", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_LOCAL1);
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


int client_child(User user)
{
    if (mkfifo(user.fifo_path, 0666) == -1)
    {
        send_to_log("Problem z utworzeniem fifo klienta");
        perror("user_mkfifo");
        exit(1);
    }

    int fd;
    if ((fd = open(user.fifo_path, O_RDONLY)) == -1)
    {
        perror("user_fifo_open");
        exit(1);
    }

    char buffer[4096];
    int bytes_read;
    memset(buffer, 0x0, sizeof(buffer));

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
        write(fileno(stdout), buffer, bytes_read);
}

int client_parent(User user)
{
    signal(SIGQUIT, &signal_handler);
    pid_t pid = getpid();
    sprintf(user.fifo_path, "/tmp/%d", pid); ///home/kali/Desktop/Komunikator/
    chmod(user.fifo_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

    char output[4096];
    memset(output, 0x0, sizeof(output));
    strcat(output, "|login|");
    strcat(output, user.username);
    strcat(output, "|");
    strcat(output, user.fifo_path);
    int fd;
    if ((fd = open(PUBLIC, O_WRONLY)) == -1)
    {
        perror("fd_open");
        exit(1);
    }

    if ((write(fd, output, sizeof(output))) == -1)
    {
        send_to_log("problem z pisaniem na serwer");
        perror("write");
        exit(1);
    }
    close(fd);

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }
    else if (pid == 0)
    {
        client_child(user);
    }
    else if (pid > 0)
    {
        char buffer[4096];
        memset(buffer, 0x0, sizeof(buffer));
        while (1)
        {
            char message[4096];
            fgets(buffer, sizeof(buffer), stdin);
            strncpy(message, buffer, 4096 - 1);
            message[4096 - 1] = '\0';

            memset(output, 0x0, sizeof(output));
            strcat(output, "send|");
            strcat(output, user.username);
            strcat(output, "|");
            strcat(output, message);
            if ((fd = open(PUBLIC, O_WRONLY)) == -1)
            {
                perror("fd_open");
                exit(1);
            }
            
            if (sig == SIGQUIT)
            {
                int fd = open(PUBLIC, O_WRONLY);
                if ((write(fd, LOGOUT, sizeof(LOGOUT))) == -1)
                {
                    perror("write_signal");
                    return -1;
                }
                sig = 0;
                kill(pid, SIGKILL);
                exit(0);
            }
            if ((write(fd, output, sizeof(output))) == -1)
            {
                perror("write");
                exit(1);
            }
        }
    }
}

int server()
{
    int fd_fifo;
    User users[20];
    int user_count = 0;

    daemon();

    if (mkfifo(PUBLIC, 0666) < 0)
    {
        if (errno != EEXIST)
            send_to_log("Plik nie istnieje");
        else
            send_to_log("Problem z utworzeniem potoku serwera");
        return -1;
    }

    if ((fd_fifo = open(PUBLIC, O_RDWR)) == -1)
    {
        send_to_log("Problem z otwarciem potoku serwera");
        return -1;
    }

    while (1)
    {
        char current_operation[20][100];

        int bytes_read;
        char bufor[4096];
        memset(bufor, 0x0, sizeof(bufor));

        if ((bytes_read = read(fd_fifo, bufor, sizeof(bufor))) == -1)
        {
            send_to_log("Problem z czytaniem z fifo_serwera");
            return -1;
        }

        if (bytes_read > 0)
        {
            // Podzial stringa na mniejsze porcje
            char *token;
            char *rest = bufor;
            int i = 0;
            while ((token = strtok_r(rest, "|", &rest)) && strlen(token) != 0)
            {
                strcpy(current_operation[i], token);
                i++;
            }
            char *operation_type = current_operation[0];
            if (!strcmp(operation_type, LOGOUT))
            {
                send_to_log("Pomyslnie wylogowano uzytkownika ");
            }
            if (!strcmp(operation_type, "login"))
            {
                User new_user;
                strncpy(new_user.username, current_operation[1], sizeof(current_operation[1]));
                strncpy(new_user.fifo_path, current_operation[2], sizeof(current_operation[2]));
                int i;
                for (i = 0; i < user_count; i++)
                {
                    if (!strcmp(new_user.username, users[i].username))
                        break;
                }
                if (i == user_count)
                    users[user_count++] = new_user;
                send_to_log("Pomyslnie zalogowano uzytkownika");
            }
            else if (!strcmp(operation_type, "send"))
            {
                char sender[4096];
                char recipient[4096];
                char recipient_fifo_path[4096];
                char message[4096];

                strncpy(sender, current_operation[1], sizeof(current_operation[1]));
                strncpy(recipient, current_operation[2], sizeof(current_operation[2]));
                strncpy(message, current_operation[3], sizeof(current_operation[3]));

                int i;

                for (i = 0; i < user_count; i++)
                {
                    if (!strcmp(sender, users[i].username))
                        break;
                }
                char output[4096];
                memset(output, 0x0, sizeof(output));
                strcat(output, sender);
                strcat(output, ": ");
                strcat(output, message);
                if (!strcmp(recipient, "all"))
                {
                    for (i = 0; i < user_count; i++)
                    {
                        int fd;
                        if ((fd = open(users[i].fifo_path, O_WRONLY)) == -1)
                        {
                            send_to_log("Problem z otwarciem potoku fifo klienta");
                            return -1;
                        }

                        if ((write(fd, output, sizeof(output))) == -1)
                        {

                            send_to_log("Problem z pisaniem informacji do potoku fifo klienta");
                            return -1;
                        }
                    }
                }
                else
                {
                    for (i = 0; i < user_count; i++)
                    {
                        if (!strcmp(recipient, users[i].username))
                        {
                            int fd;
                            if ((fd = open(users[i].fifo_path, O_WRONLY)) == -1)
                            {
                                send_to_log("Problem z otwarciem potoku fifo klienta");
                                return -1;
                            }

                            if ((write(fd, output, sizeof(output))) == -1)
                            {
                                 send_to_log("Problem z pisaniem informacji do potoku fifo klienta");
                                return -1;
                            }
                            break;
                        }
                    }
                    if (i == user_count)
                    {
                        break;
                    }
                }
                syslog(LOG_INFO, "Pomyslnie wyslano wiadomosc od %s do %s o tresci %s", sender, recipient, message);
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Opcje logowania: %s [--start | --login <nazwa_uzytkownika> | --download <sciezka_do_katalogu]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct sigaction akcja;
    akcja.sa_handler = signal_handler;
    sigemptyset(&akcja.sa_mask);
    akcja.sa_flags = 0;
    sigaction(SIGQUIT, &akcja, NULL);
    if (!strcmp(argv[1], "--start"))
        return server();

    else if (!strcmp(argv[1], "--login"))
    {
        User user;
        strncpy(user.username, argv[2], sizeof(argv[2]));
        if (argc > 3 && !strcmp(argv[3], "--download"))
            strncpy(user.download_path, argv[4], sizeof(argv[4]));
        else
            strncpy(user.download_path, "", sizeof(""));

        return client_parent(user);
    }
}