#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

static void terminate(int status);

void handle_sighup(int signal)
{
    // debug("Exiting with sighup");
    terminate(EXIT_SUCCESS);
}

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char *argv[])
{
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    char *port = Malloc(sizeof(char));
    if (argc == 3 && (strcmp(argv[1], "-p") == 0))
        strcpy(port, argv[2]);
    else
    {
        fprintf(stderr, "Usage: bin/pbx -p <port>\n");
        _exit(EXIT_FAILURE);
    }

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    // Perform required initialization of the PBX module.
    // debug("Initializing PBX...");
    pbx = pbx_init();

    Signal(SIGHUP, handle_sighup);

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    listenfd = Open_listenfd(port);

    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, pbx_client_service, connfdp);
    }

    fprintf(stderr, "There was an error with the infinite "
                    "loop of accepting connections.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status)
{
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    _exit(status);
}
