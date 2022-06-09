#include "../include/define_source.h"

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include "../include/utils.h"
#include "../include/configParser.h"
#include "../include/boundedqueue.h"
#include "../include/worker.h"
#include "../include/filesystem.h"
#include "../include/message_protocol.h"

#define DFL_SOCKET "./mysock"
#define DFL_THREADS 2
#define DFL_BACKLOG 50

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

    char *test = NULL;
    printf("Test: %s\n", (test = getValue(settings, "TEST")));
    free(test);

    freeSettingList(&settings);

    // Creo la coda per comunicare con i thread worker 
    BQueue_t *client_fd_queue = NULL;
    client_fd_queue = initBQueue(10);

    // Creo la pipe con cui i thread worker comunicherranno con il manager
    int workerManagerPipe[2];
    SYSCALL_EQ_ACTION(pipe, -1, exit(EXIT_FAILURE), workerManagerPipe);
    // Ignoro SIGPIPE e imposto l'fd della read end della pipe come non bloccante
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGPIPE, &(const struct sigaction) {.sa_handler=SIG_IGN}, NULL);
    //SYSCALL_EQ_ACTION(fcntl, -1, exit(EXIT_FAILURE), workerManagerPipe[0], F_SETFL, O_NONBLOCK);

    // Inizializzo il filesystem
    Filesystem *fs = NULL;
    fs = initFileSystem(100, 1000000000);

    // Passo i riferimenti alla struttura per gli argomenti dei thread
    ThreadArgs *th_args = malloc(sizeof(*th_args));
    th_args->queue = client_fd_queue;
    th_args->write_end_pipe_fd = workerManagerPipe[1];
    th_args->fs = fs;

    // Alloco i thread specificati e invoco la loro routine
    pthread_t *workers = malloc(sizeof(*workers) * DFL_THREADS);

    for (int i = 0; i < DFL_THREADS; i++)
    {
        int err = 0;
        CHECK_RET_AND_ACTION(pthread_create, !=, 0, err,
                             fprintf(stderr, "pthread_create: %s\n", strerror(err));
                             exit(EXIT_FAILURE), &workers[i], NULL, &processRequest, (void *)th_args);
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
    strncpy(server_addr.sun_path, DFL_SOCKET, UNIX_PATH_MAX);
    server_addr.sun_family = AF_UNIX;

    // Fd del server e fd più alta per la select
    int listen_fd, fd_max = 0;

    fd_set set, rdset;

    // Numero di client attualmente connessi
    int connected_clients = 0;

    // Creo la socket del server
    SYSCALL_RET_EQ_ACTION(socket, -1, listen_fd, exit(EXIT_FAILURE), AF_UNIX, SOCK_STREAM, 0);

    // Imposto i set per la select e calcolo il fd più alto
    FD_ZERO(&set);
    FD_ZERO(&rdset);
    FD_SET(listen_fd, &set);
    FD_SET(workerManagerPipe[0], &set);
    fd_max = MAX( listen_fd, workerManagerPipe[0]);

    // Eseguo la bind del socket con l'indirizzo del server e mi metto in ascolto di richieste di connessione
    SYSCALL_EQ_ACTION(bind, -1, exit(EXIT_FAILURE), listen_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    SYSCALL_EQ_ACTION(listen, -1, exit(EXIT_FAILURE), listen_fd, SOMAXCONN);

    // Esco quando ricevo i segnali di terminazione
    hardQuit = 0, softQuit = 0;
    while (!hardQuit)
    {

        // ripristino il set dei fd in lettura
        rdset = set;

        if (select(fd_max + 1, &rdset, NULL, NULL, NULL) == -1)
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
            
            if (FD_ISSET(fd, &rdset))
            {
                if (fd == listen_fd) // richiesta di connessione
                {
                    int fd_c = 0; 
                    SYSCALL_RET_EQ_ACTION(accept, -1, fd_c, continue, listen_fd, NULL, 0);

                    if (softQuit) {
                        SYSCALL_EQ_ACTION(close, -1, continue, fd_c);
                        continue;
                    }

                    FD_SET(fd_c, &set);
                    fd_max = MAX(fd_max, fd_c);
                    connected_clients++;
                } 
                else if (fd == workerManagerPipe[0]) // messaggio da un thread worker
                {
                    char buf[PIPE_BUF_LEN + 1] = "";

                    CHECK_AND_ACTION(read, ==, -1, perror("readn"); exit(EXIT_FAILURE), workerManagerPipe[0], buf, PIPE_BUF_LEN);

                    long fd_sent_from_worker = atol(buf);

                    if(fd_sent_from_worker == 0) {
                        if (--connected_clients == 0 && softQuit)
                            goto shutdown;

                        continue;
                    }

                    FD_SET(fd_sent_from_worker, &set);
                    fd_max = MAX(fd_max, fd_sent_from_worker);
                }
                else // fd client già connesso
                {
                    FD_CLR(fd, &set);

                    if (fd == fd_max)
                        fd_max = updatemax(set, fd_max);

                    int *client_fd = malloc(sizeof(int));
                    *client_fd = fd;
                    CHECK_AND_ACTION(push, ==, -1, perror("push"); exit(EXIT_FAILURE), client_fd_queue, client_fd);
                }
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

    free(workers);
    free(th_args);
    deleteBQueue(client_fd_queue, NULL);
    //stampo i contenuti del filesystem e lo elimino
    printFileSystem(fs);
    deleteFileSystem(fs);
    return 0;
}