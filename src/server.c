#include "../include/define_source.h"

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "../include/boundedqueue.h"
#include "../include/configParser.h"
#include "../include/filesystem.h"
#include "../include/message_protocol.h"
#include "../include/utils.h"
#include "../include/worker.h"

#define DFL_SOCKET "./LSOfiletorage.sk"
#define DFL_THREADS 2
#define DFL_MAXFILES 10
#define DFL_MAXMEMORY 100
#define DFL_BACKLOG 5

#define GET_NUMERIC_SETTING_VAL(settings, key, val, default, op, cond)              \
    if (1)                                                                          \
    {                                                                               \
        errno = 0;                                                                  \
        if ((val = getNumericValue(settings, key)) == -1 || val op cond)            \
        {                                                                           \
            fprintf(stderr, "Errore valore " #key ", usando valore di default.\n"); \
            val = default;                                                          \
        }                                                                           \
    }

#define GET_SETTING_VAL(settings, key, val, default)                                \
    if (1)                                                                          \
    {                                                                               \
        if ((val = getValue(settings, key)) == NULL)                                \
        {                                                                           \
            fprintf(stderr, "Errore valore " key ", usando valore di default.\n"); \
            val = strdup(default);                                                  \
            if (!val)                                                               \
            {                                                                       \
                perror("strdup " default);                                          \
                if (settings)                                                       \
                    freeSettingList(&settings);                                     \
                                                                                    \
                exit(EXIT_FAILURE);                                                 \
            }                                                                       \
        }                                                                           \
    }

int hardQuit, softQuit;

int updatemax(fd_set set, int fdmax)
{
    for (int i = (fdmax - 1); i >= 0; --i)
        if (FD_ISSET(i, &set))
            return i;
    assert(1 == 0);
    return -1;
}

void cleanup()
{
    unlink(DFL_SOCKET);
}

void server_shutdown_handler(int signum)
{
    if (signum == SIGHUP)
        softQuit = 1;
    else
        hardQuit = 1;
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        printf("Uso: ./bin/server <path-file-di-configurazione>\n");
        exit(EXIT_SUCCESS);
    }

    atexit(cleanup);

    // Parso il file di configurazione
    Setting *settings = parseFile(argv[1]);

    if (!settings)
    {
        fprintf(stderr, "Errore parsando file di configurazione\n");
        exit(EXIT_FAILURE);
    }

    char *sockname;

    size_t maxFiles,
        maxMemory,
        nThreads;

    GET_NUMERIC_SETTING_VAL(settings, "THREADS", nThreads, DFL_THREADS, <=, 0);
    GET_NUMERIC_SETTING_VAL(settings, "MAXMEMORY", maxMemory, DFL_MAXMEMORY, <=, 0);
    GET_NUMERIC_SETTING_VAL(settings, "MAXFILES", maxFiles, DFL_MAXFILES, <=, 0);
    GET_SETTING_VAL(settings, "SOCKNAME", sockname, DFL_SOCKET);

    freeSettingList(&settings);

    // Creo la coda per comunicare con i thread worker
    BQueue_t *client_fd_queue = NULL;
    client_fd_queue = initBQueue(10);

    // Creo la pipe con cui i thread worker comunicherranno con il manager
    int workerManagerPipe[2];
    SYSCALL_EQ_ACTION(pipe, -1, exit(EXIT_FAILURE), workerManagerPipe);
    // Ignoro SIGPIPE
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGPIPE, &(const struct sigaction){.sa_handler = SIG_IGN}, NULL);
    SYSCALL_EQ_ACTION(fcntl, -1, exit(EXIT_FAILURE), workerManagerPipe[0], F_SETFL, O_NONBLOCK);

    // Inizializzo il filesystem
    Filesystem *fs = NULL;
    fs = initFileSystem(maxFiles, maxMemory);

    // Passo i riferimenti alla struttura per gli argomenti dei thread
    ThreadArgs *th_args = malloc(sizeof(*th_args));
    th_args->queue = client_fd_queue;
    th_args->write_end_pipe_fd = workerManagerPipe[1];
    th_args->fs = fs;

    // Alloco i threads e invoco la loro routine
    pthread_t *workers = calloc(nThreads, sizeof(pthread_t));

    if (!workers)
        exit(EXIT_FAILURE);

    for (int i = 0; i < DFL_THREADS; i++)
    {
        if (pthread_create(&workers[i], NULL, &processRequest, (void *)th_args) != 0)
            exit(EXIT_FAILURE);
    }

    // Imposto l'handler per la gestione dei segnali SIGINT, SIGQUIT e SIGHUP
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = server_shutdown_handler;

    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGINT, &act, NULL);
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGQUIT, &act, NULL);
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGHUP, &act, NULL);

    // Imposto l'indirizzo della socket del server
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    strncpy(server_addr.sun_path, sockname, UNIX_PATH_MAX);
    server_addr.sun_family = AF_UNIX;

    // Fd del server e fd massima per la select
    int listen_fd, fd_max = 0;

    fd_set master_set, working_set;

    // Numero di client attualmente connessi
    int connected_clients = 0;

    // Creo la socket del server
    SYSCALL_RET_EQ_ACTION(socket, -1, listen_fd, exit(EXIT_FAILURE), AF_UNIX, SOCK_STREAM, 0);

    // Inizializzo il master_set e imposto i fd per la select, calcolando l'fd più alto
    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);
    FD_SET(workerManagerPipe[0], &master_set);
    fd_max = MAX(listen_fd, workerManagerPipe[0]);

    // Eseguo la bind del socket con l'indirizzo del server e mi metto in ascolto di richieste di connessione
    SYSCALL_EQ_ACTION(bind, -1, exit(EXIT_FAILURE), listen_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    SYSCALL_EQ_ACTION(listen, -1, exit(EXIT_FAILURE), listen_fd, SOMAXCONN);

    // Esco quando ricevo i segnali di terminazione
    hardQuit = 0, softQuit = 0;
    while (!hardQuit)
    {

        // copio il master fd_set sul working fd_set
        memcpy(&working_set, &master_set, sizeof(master_set));

        if (select(fd_max + 1, &working_set, NULL, NULL, NULL) == -1)
        {
            // Se ricevo un interruzione esco dal ciclo subito se: non ci sono client connessi e ho ricevuto SIGHUP, ho ricevuto SIGINT o SIGQUIT
            if (errno == EINTR)
            {
                if (connected_clients == 0 && softQuit)
                    break;

                continue;
            }
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int fd = 0; fd <= fd_max; fd++)
        {

            if (FD_ISSET(fd, &working_set))
            {
                if (fd == listen_fd) // richiesta di connessione
                {
                    int fd_client;
                    SYSCALL_RET_EQ_ACTION(accept, -1, fd_client, continue, listen_fd, NULL, 0);

                    if (softQuit)
                    {
                        SYSCALL_EQ_ACTION(close, -1, exit(EXIT_FAILURE), fd_client);
                        continue;
                    }

                    FD_SET(fd_client, &master_set);
                    fd_max = MAX(fd_max, fd_client);
                    ++connected_clients;

                    continue;
                }

                if (fd == workerManagerPipe[0]) // messaggio da un thread worker
                {
                    int fd_sent_from_worker;

                    CHECK_AND_ACTION(readn, ==, -1, perror("readn"); exit(EXIT_FAILURE), workerManagerPipe[0], &fd_sent_from_worker, sizeof(int));

                    if (fd_sent_from_worker == 0)
                    {
                        if (--connected_clients == 0 && softQuit)
                            goto shutdown;

                        continue;
                    }

                    FD_SET(fd_sent_from_worker, &master_set);
                    fd_max = MAX(fd_max, fd_sent_from_worker);
                    continue;
                }

                // Un fd di un client già connesso è pronto per la lettura, quindi lo elimino dal master fd_set e lo spedisco ai thread worker
                FD_CLR(fd, &master_set);

                if (fd == fd_max)
                    fd_max = updatemax(master_set, fd_max);

                int *client_fd;
                CHECK_RET_AND_ACTION(malloc, ==, NULL, client_fd, perror("malloc"); exit(EXIT_FAILURE), sizeof(fd));
                memcpy(client_fd, &fd, sizeof(fd));

                CHECK_AND_ACTION(push, ==, -1, perror("push"); exit(EXIT_FAILURE), client_fd_queue, client_fd);
            }
        }
    }

shutdown:
    SYSCALL_EQ_ACTION(close, -1, exit(EXIT_FAILURE), listen_fd);
    printf("\nChiudendo il server\n");
    // Mando segnale di terminazione ai thread worker
    for (int i = 0; i < DFL_THREADS; i++)
        push(client_fd_queue, EOS);

    // E attendo la loro effettiva terminazione
    for (size_t i = 0; i < DFL_THREADS; i++)
    {
        int err = 0;
        CHECK_RET_AND_ACTION(pthread_join, !=, 0, err,
                             fprintf(stderr, "pthread_join: %s\n", strerror(err));
                             exit(EXIT_FAILURE), workers[i], NULL);
    }

    SYSCALL_EQ_ACTION(unlink, -1, exit(EXIT_FAILURE), sockname);
    free(sockname);

    SYSCALL_EQ_ACTION(close, -1, exit(EXIT_FAILURE), workerManagerPipe[0]);
    SYSCALL_EQ_ACTION(close, -1, exit(EXIT_FAILURE), workerManagerPipe[1]);

    free(workers);
    free(th_args);
    deleteBQueue(client_fd_queue, NULL);
    // stampo i contenuti del filesystem e lo elimino
    printFileSystem(fs);
    deleteFileSystem(fs);
    return 0;
}