#include "server.h"
#include "debug.h"
#include "pbx.h"
#include "csapp.h"

void *pbx_client_service(void *arg)
{
    debug("Inside server.c");

    int connfd = *((int *)arg);
    Pthread_detach(pthread_self());
    Free(arg);

    TU *tu_client = pbx_register(pbx, connfd);
    FILE *file = Fdopen(connfd, "r");
    char command[MAXBUF];
    int stat = -1;

    while (1)
    {
        strcpy(command, "");
        fscanf(file, " %[^\r\n]s", command);
        debug("String read: %s\n", command);
        if (feof(file))
            break;

        if (strcmp(command, tu_command_names[TU_PICKUP_CMD]) == 0)
        {
            stat = tu_pickup(tu_client);
            debug("tu_pickup status: %d", stat);
        }
        else if (strcmp(command, tu_command_names[TU_HANGUP_CMD]) == 0)
        {
            stat = tu_hangup(tu_client);
            debug("tu_hangup status: %d", stat);
        }
        else if (strncmp(command, tu_command_names[TU_DIAL_CMD], 4) == 0 && strlen(command) >= 6)
        {
            stat = tu_dial(tu_client, atoi(command + 4));
            debug("tu_dial status: %d", stat);
        }
        else if (strncmp(command, tu_command_names[TU_CHAT_CMD], 4) == 0)
        {
            stat = tu_chat(tu_client, command + 5);
            debug("tu_chat status: %d", stat);
        }
    }

    stat++;

    debug("Exited the loop");
    pbx_unregister(pbx, tu_client);
    Fclose(file);

    return NULL;
}