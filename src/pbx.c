#include "server.h"
#include "debug.h"
#include "pbx.h"
#include "csapp.h"

struct tu
{
    TU_STATE current_state;
    int number;
    int outgoing;
    int incoming;
    sem_t tu_sem;
};

struct pbx
{
    int num_registered_tu;
    sem_t pbx_sem;
    TU *registered_tu[PBX_MAX_EXTENSIONS];
};

int printStatus(TU *tu, char *msg);

/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init()
{
    // debug("Entered pbx_init");

    PBX *temp = (PBX *)Malloc(sizeof(PBX));
    // debug("Size of pbs: %ld", sizeof(PBX));

    if (temp == NULL)
        return NULL;

    Sem_init(&temp->pbx_sem, 0, 1);
    P(&temp->pbx_sem);

    temp->num_registered_tu = 0;
    // debug("Exiting pbx_init");
    V(&temp->pbx_sem);
    return temp;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed.  The PBX object itself is freed, and should not be used again.
 *
 * @param pbx  The PBX to be shut down.
 */
void pbx_shutdown(PBX *pbx)
{
    // debug("Entered pbx_shutdown");
    P(&pbx->pbx_sem);

    Free(pbx);
    pbx = NULL;

    // debug("Exiting pbx_shutdown");
    V(&pbx->pbx_sem);
}

/*
 * Register a TU client with a PBX.
 * This amounts to "plugging a telephone unit into the PBX".
 * The TU is assigned an extension number and it is initialized to the TU_ON_HOOK state.
 * A notification of the assigned extension number is sent to the underlying network
 * client.
 *
 * @param pbx  The PBX.
 * @param fd  File descriptor providing access to the underlying network client.
 * @return A TU object representing the client TU, if registration succeeds, otherwise NULL.
 * The caller is responsible for eventually calling pbx_unregister to free the TU object
 * that was returned.
 */
TU *pbx_register(PBX *pbx, int fd)
{
    debug("Entered pbx_register | fd: %d", fd);

    if (fd >= PBX_MAX_EXTENSIONS)
    {
        fprintf(stderr, "ERROR: Reached PBX_MAX_EXTENSIONS. FD: %d", fd);
        return NULL;
    }

    TU *temp_tu = (TU *)Malloc(sizeof(TU));
    if (temp_tu == NULL)
        return NULL;

    Sem_init(&temp_tu->tu_sem, 0, 1);
    P(&temp_tu->tu_sem);

    temp_tu->number = fd;
    temp_tu->incoming = -1;
    temp_tu->outgoing = -1;
    temp_tu->current_state = TU_ON_HOOK;
    printStatus(temp_tu, "");

    P(&pbx->pbx_sem);
    pbx->registered_tu[temp_tu->number] = temp_tu;
    pbx->num_registered_tu++;

    // debug("File descriptor   %d", temp_tu->number);
    // debug("num_registered_tu %d", pbx->num_registered_tu);

    V(&temp_tu->tu_sem);
    V(&pbx->pbx_sem);
    // debug("Exiting pbx_register");

    return temp_tu;
}

/*
 * Unregister a TU from a PBX.
 * This amounts to "unplugging a telephone unit from the PBX".
 *
 * @param pbx  The PBX.
 * @param tu  The TU to be unregistered.
 * This object is freed as a result of the call and must not be used again.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu)
{
    // debug("Entered pbx_unregister");

    P(&pbx->pbx_sem);
    if (pbx->registered_tu[tu->number] == NULL || pbx->num_registered_tu < 1)
        return -1;

    // debug("File descriptor   %d", tu->number);
    pbx->registered_tu[tu->number] = NULL;
    pbx->num_registered_tu--;
    Free(tu);
    // debug("num_registered_tu %d", pbx->num_registered_tu);
    // debug("Exiting pbx_unregister");
    V(&pbx->pbx_sem);

    return 0;
}

/*
 * Get the file descriptor for the network connection underlying a TU.
 * This file descriptor should only be used by a server to read input from
 * the connection.  Output to the connection must only be performed within
 * the PBX functions.
 *
 * @param tu
 * @return the underlying file descriptor, if any, otherwise -1.
 */
int tu_fileno(TU *tu)
{
    // debug("Returning from tu_fileno");
    return tu->number;
}

/*
 * Get the extension number for a TU.
 * This extension number is assigned by the PBX when a TU is registered
 * and it is used to identify a particular TU in calls to tu_dial().
 * The value returned might be the same as the value returned by tu_number(),
 * but is not necessarily so.
 *
 * @param tu
 * @return the extension number, if any, otherwise -1.
 */
int tu_extension(TU *tu)
{
    // debug("Returning from tu_extension");
    return tu->number;
}

/*
 * Take a TU receiver off-hook (i.e. pick up the handset).
 *
 *   If the TU was in the TU_ON_HOOK state, it goes to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RINGING state, it goes to the TU_CONNECTED state,
 *     reflecting an answered call.  In this case, the calling TU simultaneously
 *     also transitions to the TU_CONNECTED state.
 *   If the TU was in any other state, then it remains in that state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU. In addition, if the new state is TU_CONNECTED, then the
 * calling TU is also notified of its new state.
 *
 * @param tu  The TU that is to be taken off-hook.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_pickup(TU *tu)
{
    // debug("Entering tu_pickup");
    P(&pbx->pbx_sem);
    // P(&tu->tu_sem);

    switch (tu->current_state)
    {

    case TU_ON_HOOK:
        // debug("Entering TU_ON_HOOK");

        tu->current_state = TU_DIAL_TONE;
        printStatus(tu, "");
        break;

    case TU_RINGING:
        // debug("Entering TU_RINGING");
        tu->current_state = TU_CONNECTED;
        printStatus(tu, "");
        pbx->registered_tu[tu->incoming]->current_state = TU_CONNECTED;
        pbx->registered_tu[tu->incoming]->incoming = tu->number;
        printStatus(pbx->registered_tu[tu->incoming], "");
        break;

    default:
        // debug("Entering default");
        printStatus(tu, "");
        break;
    }

    // V(&tu->tu_sem);
    V(&pbx->pbx_sem);
    // debug("Returning from tu_pickup");
    return 0;
}

/*
 * Hang up a TU (i.e. replace the handset on the switchhook).
 *
 *   If the TU was in the TU_CONNECTED state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the peer TU (the one to which the call is currently
 *     connected) simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RING_BACK state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the calling TU (which is in the TU_RINGING state)
 *     simultaneously transitions to the TU_ON_HOOK state.
 *   If the TU was in the TU_RINGING state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the called TU (which is in the TU_RING_BACK state)
 *     simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_DIAL_TONE, TU_BUSY_SIGNAL, or TU_ERROR state,
 *     then it goes to the TU_ON_HOOK state.
 *   If the TU was in any other state, then there is no change of state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.  In addition, if the previous state was TU_CONNECTED,
 * TU_RING_BACK, or TU_RINGING, then the peer, called, or calling TU is also
 * notified of its new state.
 *
 * @param tu  The tu that is to be hung up.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_hangup(TU *tu)
{
    // debug("Entering tu_hangup");
    P(&pbx->pbx_sem);
    // P(&tu->tu_sem);

    switch (tu->current_state)
    {

    case TU_CONNECTED:
        // debug("TU_CONNECTED");
        tu->current_state = TU_ON_HOOK;
        printStatus(tu, "");

        if (tu->incoming != -1)
        {
            // debug("TU_CONNECTED: tu->incoming != -1");
            pbx->registered_tu[tu->incoming]->current_state = TU_DIAL_TONE;
            // debug("II TU_CONNECTED: tu->incoming != -1");
            pbx->registered_tu[tu->incoming]->outgoing = -1;
            // debug("III TU_CONNECTED: tu->incoming != -1");
            printStatus(pbx->registered_tu[tu->incoming], "");
            // debug("IV TU_CONNECTED: tu->incoming != -1");
            tu->incoming = -1;
            // debug("V TU_CONNECTED: tu->incoming != -1");
        }
        else if (tu->outgoing != -1)
        {
            // debug("I TU_CONNECTED: tu->outgoing != -1");
            pbx->registered_tu[tu->outgoing]->current_state = TU_DIAL_TONE;
            // debug("II TU_CONNECTED: tu->outgoing != -1");
            pbx->registered_tu[tu->outgoing]->incoming = -1;
            // debug("III TU_CONNECTED: tu->outgoing != -1");
            printStatus(pbx->registered_tu[tu->incoming], "");
            // debug("IV TU_CONNECTED: tu->outgoing != -1");
            tu->outgoing = -1;
            // debug("V TU_CONNECTED: tu->outgoing != -1");
        }
        break;

    case TU_RING_BACK:
        // debug("TU_RING_BACK");
        tu->current_state = TU_ON_HOOK;
        printStatus(tu, "");

        if (tu->incoming != -1)
        {
            // debug("I TU_RING_BACK: tu->incoming != -1");
            pbx->registered_tu[tu->incoming]->current_state = TU_ON_HOOK;
            // debug("II TU_RING_BACK: tu->incoming != -1");
            printStatus(pbx->registered_tu[tu->incoming], "");
            // debug("III TU_RING_BACK: tu->incoming != -1");
            tu->incoming = -1;
            // debug("IV TU_RING_BACK: tu->incoming != -1");
        }
        else if (tu->outgoing != -1)
        {
            // debug("I TU_RING_BACK: tu->outgoing != -1");
            pbx->registered_tu[tu->outgoing]->current_state = TU_ON_HOOK;
            // debug("II TU_RING_BACK: tu->outgoing != -1");
            printStatus(pbx->registered_tu[tu->outgoing], "");
            // debug("III TU_RING_BACK: tu->outgoing != -1");
            tu->outgoing = -1;
            // debug("IV TU_RING_BACK: tu->outgoing != -1");
        }
        break;

    case TU_RINGING:
        // debug("TU_RINGING");
        tu->current_state = TU_ON_HOOK;
        printStatus(tu, "");

        pbx->registered_tu[tu->incoming]->current_state = TU_DIAL_TONE;
        printStatus(pbx->registered_tu[tu->incoming], "");
        break;

    case TU_DIAL_TONE:
    case TU_BUSY_SIGNAL:
    case TU_ERROR:
        // debug("TU_ERROR");
        tu->current_state = TU_ON_HOOK;
        printStatus(tu, "");
        break;

    default:
        // debug("default");
        printStatus(tu, "");
        break;
    }

    // V(&tu->tu_sem);
    V(&pbx->pbx_sem);
    // debug("Returning from tu_hangup");
    return 0;
}

/*
 * Dial an extension on a TU.
 *
 *   If the specified extension number does not refer to any currently registered
 *     extension, then the TU transitions to the TU_ERROR state.
 *   Otherwise, if the TU was in the TU_DIAL_TONE state, then what happens depends
 *     on the current state of the dialed extension:
 *       If the dialed extension was in the TU_ON_HOOK state, then the calling TU
 *         transitions to the TU_RING_BACK state and the dialed TU simultaneously
 *         transitions to the TU_RINGING state.
 *       If the dialed extension was not in the TU_ON_HOOK state, then the calling
 *         TU transitions to the TU_BUSY_SIGNAL state and there is no change to the
 *         state of the dialed extension.
 *   If the TU was in any state other than TU_DIAL_TONE, then there is no state change.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.  In addition, if the new state is TU_RING_BACK, then the
 * called extension is also notified of its new state (i.e. TU_RINGING).
 *
 * @param tu  The tu on which the dialing operation is to be performed.
 * @param ext  The extension to be dialed.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_dial(TU *tu, int ext)
{
    P(&pbx->pbx_sem);
    // P(&tu->tu_sem);

    if (tu == NULL || ext < 4)
    {
        V(&pbx->pbx_sem);
        // V(&tu->tu_sem);
        return -1;
    }
    if (pbx->registered_tu[ext] == NULL && tu->current_state == TU_DIAL_TONE)
    {
        tu->current_state = TU_ERROR;
        printStatus(tu, "");
    }
    else if (tu->current_state == TU_DIAL_TONE)
    {
        if(tu->number == ext)
        {
            tu->current_state = TU_BUSY_SIGNAL;
            printStatus(tu, "");
        }
        else if (pbx->registered_tu[ext]->current_state == TU_ON_HOOK)
        {
            tu->current_state = TU_RING_BACK;
            tu->outgoing = ext;
            printStatus(tu, "");
            pbx->registered_tu[ext]->current_state = TU_RINGING;
            pbx->registered_tu[ext]->incoming = tu->number;
            printStatus(pbx->registered_tu[ext], "");
        }
        else
        {
            tu->current_state = TU_BUSY_SIGNAL;
            printStatus(tu, "");
        }
    }
    else
        printStatus(tu, "");

    // V(&tu->tu_sem);
    V(&pbx->pbx_sem);

    return 0;
}

/*
 * "Chat" over a connection.
 *
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged and a notification containing
 * the current state is sent to the TU sending the chat.
 *
 * @param tu  The tu sending the chat.
 * @param msg  The message to be sent.
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg)
{
    // debug("Entering tu_chat");

    P(&pbx->pbx_sem);
    // P(&tu->tu_sem);

    if (tu->current_state != TU_CONNECTED)
    {
        // debug("Returning from tu_chat with error");
        printStatus(tu, "");
        // V(&tu->tu_sem);
        V(&pbx->pbx_sem);
        return -1;
    }
    else
    {
        if (tu->incoming != -1)
        {
            printStatus(pbx->registered_tu[tu->incoming], msg);
            printStatus(tu, "CHAT");
        }

        else if (tu->outgoing != -1)
        {
            printStatus(pbx->registered_tu[tu->outgoing], msg);
            printStatus(tu, "CHAT");
        }
    }

    // debug("Returning from tu_chat");

    // V(&tu->tu_sem);
    V(&pbx->pbx_sem);

    return 0;
}

/*
 *
 * Prints the status of the tu passed in.
 * 
 * @param tu  The tu printing the status.
 * @param msg  The message to print if it is a chat message.
 * @param status -1 if there was an error printing. 0 on Success.
 */
int printStatus(TU *tu, char *msg)
{
    int status = 0;
    if (strcmp(msg, "") == 0)
    {
        // debug("In ''");

        switch (tu->current_state)
        {

        case TU_ON_HOOK:
            status = dprintf(tu->number, "%s %d%s", tu_state_names[tu->current_state], tu->number, EOL);
            break;

        case TU_CONNECTED:
            if (tu->incoming != -1)
                status = dprintf(tu->number, "%s %d%s", tu_state_names[tu->current_state], tu->incoming, EOL);

            else if (tu->outgoing != -1)
                status = dprintf(tu->number, "%s %d%s", tu_state_names[tu->current_state], tu->outgoing, EOL);

            break;

        case TU_RINGING:
        case TU_DIAL_TONE:
        case TU_RING_BACK:
        case TU_BUSY_SIGNAL:
        case TU_ERROR:
            status = dprintf(tu->number, "%s%s", tu_state_names[tu->current_state], EOL);
            break;
        }
    }
    else if (strcmp(msg, "CHAT") == 0)
    {
        // debug("In print CHAT");
        if (tu->incoming != -1)
            status = dprintf(tu->number, "%s %d%s", tu_state_names[tu->current_state], pbx->registered_tu[tu->incoming]->number, EOL);

        else if (tu->outgoing != -1)
            status = dprintf(tu->number, "%s %d%s", tu_state_names[tu->current_state], pbx->registered_tu[tu->outgoing]->number, EOL);
    }

    else
        status = dprintf(tu->number, "CHAT %s%s", msg, EOL);

    return status;
}
