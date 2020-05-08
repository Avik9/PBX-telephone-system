# Private Branch Exchange (PBX) telephone system
#### Avi  k Kadakia and Professor Eugene Stark

### **Due Date: Friday 5/8/2020 @ 11:59pm**

## Introduction

The overall objective is to implement a server that simulates the behavior
of a Private Branch Exchange (PBX) telephone system.

For this project, there are three modules:
  * Server initialization (`main.c`)
  * Server module (`server.c`)
  * PBX module (`pbx.c`)

### The Base Code

Here is the structure of the base code:

```
.
├── .gitlab-ci.yml
└── hw5
    ├── demo
    │   └── pbx
    ├── hw5.sublime-project
    ├── include
    │   ├── csapp.h
    │   ├── debug.h
    │   ├── pbx.h
    │   └── server.h
    ├── lib
    │   └── pbx.a
    ├── Makefile
    ├── src
    │   ├── csapp.c
    │   ├── globals.c
    │   └── main.c
    │   └── pbx.c
    │   └── server.c
    ├── tests
    │   └── .git_keep
    └── util
        └── tester.c
        └── tester
```

Most of the detailed specifications for the various modules and functions
that are to implemented are provided in the comments in the header
files in the `include` directory.  In the interests of brevity and avoiding
redundancy, those specifications are not reproduced in this document.
Nevertheless, the information they contain is very important, and constitutes
the authoritative specification of what you are to implement.

## The PBX Server: Overview

"PBX" is a simple implementation of a server that simulates a PBX
telephone system.  A PBX is a private telephone exchange that is used
within a business or other organization to allow calls to be placed
between telephone units (TUs) attached to the system, without having to
route those calls over the public telephone network.  We will use the
familiar term "extension" to refer to one of the TUs attached to a PBX.

The PBX system provides the following basic capabilities:

  * *Register* a TU as an extension in the system.
  * *Unregister* a previously registered extension.

Once a TU has been registered, the following operations are available
to perform on it:

  * *Pick up* the handset of a registered TU.  If the TU was ringing,
then a connection is established with a calling TU.  If the TU was not
ringing, then the user hears a dial tone over the receiver.
  * *Hang up* the handset of a registered TU.  Any call in progress is
disconnected.
  * *Dial* an extension on a registered TU.  If the dialed extension is
currently "on hook" (*i.e.* the telephone handset is on the switchhook),
then the dialed extension starts to ring, indicating the presence of an
incoming call, and a "ring back" notification is played over the receiver
of the calling extension.  Otherwise, if the dialed extension is "off hook",
then a "busy signal" notification is played over the receiver of the
calling extension.
  * *Chat* over the connection made when one TU has dialed an extension
and the called extension has picked up.

The basic idea of these operations should be simple and familiar,
since I am sure that everyone has used a telephone system :wink:.
However, we will need a rather more detailed and complete specification
than just this simple overview.

## The PBX Server: Details

Our PBX system will be implemented as a multi-threaded network server.
When the server is started, a **master server** thread sets up a socket on
which to listen for connections from clients (*i.e.* the TUs).  When a network
connection is accepted, a **client service thread** is started to handle requests
sent by the client over that connection.  The client service thread registers
the client with the PBX system and is assigned an extension number.
The client service thread then executes a service loop in which it repeatedly
receives a **message** sent by the client, performs some operation determined
by the message, and sends one or more messages in response.
The server will also send messages to a client asynchronously as a result of actions
performed on behalf of other clients.
For example, if one client sends a "dial" message to dial another extension,
then if that extension is currently on-hook it will receive an asynchronous
"ring" message, indicating that the ringer is to be activated.

Messages from a client to a server represent commands to be performed.
Except for messages containing "chat" sent from a connected TU, every message
from the server to a client will consist of a notification of the current state
of that client, as it is currently understood by the server.
Usually these messages will inform the client of a state change that has occurred
as a result of a command the client sent, or of an asynchronous state change
that has occurred as a result of a command sent by some other client.
If a command sent by a client does not result in any state change, then the
response sent by the server will simply report the unchanged state.

> :nerd: One of the basic tenets of network programming is that a
> network connection can be broken at any time and the parties using
> such a connection must be able to handle this situation.  In the
> present context, the client's connection to the PBX server may
> be broken at any time, either as a result of explicit action by the
> client or for other reasons.  When disconnection of the client is
> noticed by the client service thread, the server acts as though the client
> had sent an explicit **hangup** command, the client is then unregistered from
> the PBX, and the client service thread terminates.

The PBX system maintains the set of registered clients in the form of a mapping
from assigned extension numbers to clients.
It also maintains, for each registered client, information about the current
state of the TU for that client.  The following are the possible states of a TU:

  * **On hook**: The TU handset is on the switchhook and the TU is idle.
  * **Ringing**: The TU handset is on the switchhook and the TU ringer is
active, indicating the presence of an incoming call.
  * **Dial tone**:  The TU handset is off the switchhook and a dial tone is
being played over the TU receiver.
  * **Ring back**:  The TU handset is off the switchhook and a "ring back"
signal is being played over the TU receiver.
  * **Busy signal**:  The TU handset is off the switchhook and a "busy"
signal is being played over the TU receiver.
  * **Connected**:  The TU handset is off the switchhook and a connection has been
established between this TU and the TU at another extension.  In this state
it is possible for users at the two TUs to "chat" with each other over the
connection.
  * **Error**:  The TU handset is off the switchhook and an "error" signal
is being played over the TU receiver.

Transitions between TU states occur in response to messages received from the
associated client, and sometimes also asynchronously in conjunction with
state transitions of other TUs.
The list below specifies all the possible transitions between states that a TU
can perform.  The arrival of any message other than those explicitly listed for
each particular state does not cause any transition between states to take place.

  * When in the **on hook** state:
    * A **pickup** message from the client will cause a transition to the **dial tone** state.
    * An asynchronous transition to the **ringing** state is also possible from this state.

  * When in the **ringing** state:
    * A **pickup** message from the client will cause a transition to the **connected** state.
      Simultaneously, the called TU will also make an asynchronous transition from the
      **ringing** state to the **connected** state.  The PBX will establish a connection
      between these two TUs, which we will refer to as *peers* as long as the connection
      remains established.
    * An asynchronous transition to the **on hook** state is also possible from this state.

  * When in the **dial tone** state:
    * A **hangup** message from the client will cause a transition to the **on hook** state.
    * A **dial** message from the client will cause a transition to either the **error**,
      **busy signal**, or **ring back** states, depending firstly on whether or not a TU
      is currently registered at the dialed extension, and secondly, whether the TU at
      the dialed extension is currently in the **on hook** state or in some other state.
      If there is no TU registered at the dialed extension, the transition will be to the
      **error** state.
      If there is a TU registered at the dialed extension, then if that extension is
      currently not in the **on hook** state, then the transition will be to the
      **busy signal** state, otherwise the transition will be to the **ring back** state.
      In the latter case, the TU at the dialed extension makes an asynchronous transition
      to the **ringing** state, simultaneously with the transition of the calling TU to the
      **ring back** state.

  * When in the **ring back** state:
    * A **hangup** message from the client will cause a transition to the **on hook** state.
      Simultaneously, the called TU will make an asynchronous transition from the **ringing**
	  state to the **on hook** state.
    * An asynchronous transition to the **connected** state (if the called TU picks up)
      or to the **dial tone** state (if the called TU unregisters) is also possible from this state.

  * When in the **busy signal** state:
    * A **hangup** message from the client will cause a transition to the **on hook** state.

  * When in the **connected** state:
    * A **hangup** message from the client will cause a transition to the **on hook** state.
      Simultaneously, the peer TU will make a transition from the **connected** state to
      the **dial tone** state.
    * An asynchronous transition to the **dial tone** state is possible from this state.

  * When in the **error** state:
    * A **hangup** message from the client will cause a transition to the **on hook** state.

Messages are sent between the client and server in a text-based format,
in which each message consists of a single line of text, the end of which is
indicated by the two-byte line termination sequence `"\r\n"`.
There is no *a priori* limitation on the length of the line of text that is sent
in a message.
The possible messages that can be sent are listed below.
The initial keywords in each message are case-sensitive.

  * Commands from Client to Server
    * **pickup**
    * **hangup**
    * **dial** #, where # is the number of the extension to be dialed.
    *  **chat** ...arbitrary text...

  * Responses from Server to Client
    * **ON HOOK** #, where # reports the extension number of the client.
    * **RINGING**
    * **DIAL TONE**
    * **RING BACK**
    * **CONNECTED** #, where # is the number of the extension to which the
      connection exists.
    * **ERROR**
    * **CHAT** ...arbitrary text...

## Task I: Server Initialization

The `main()` function does the following things:

- Obtains the port number to be used by the server from the command-line
  arguments.  The port number is to be supplied by the required option
  `-p <port>`.
  
- Installs a `SIGHUP` handler so that clean termination of the server can
  be achieved by sending it a `SIGHUP`.  

- Sets up the server socket and enter a loop to accept connections
  on this socket.  For each connection, a thread starts to
  run function `pbx_client_service()`.

These things should be relatively straightforward to accomplish, given the
information presented in class and in the textbook.  If you do them properly,
the server should function and accept connections on the specified port,
and you should be able to connect to the server using the test client.

## Task II: Server Module

The server module,
which provides the function `pbx_client_service()` that is invoked
when a client connects to the server.  This function is implemented
in the `src/server.c` file.

The `pbx_client_service` function is invoked as the **thread function**
for a thread that is created (using ``pthread_create()``) to service a
client connection.
The argument is a pointer to the integer file descriptor to be used
to communicate with the client.  Once this file descriptor has been
retrieved, the storage it occupied needs to be freed.
The thread must then become detached, so that it does not have to be
explicitly reaped, and it must register the client file descriptor with
the PBX module.
Finally, the thread should enter a service loop in which it repeatedly
receives a message sent by the client, parses the message, and carries
out the specified command.
The actual work involved in carrying out the command is performed by calling
the functions provided by the PBX module.
These functions will also send the required response back to the client,
so the server module need not be directly concerned with that.

## Task III: PBX Module

The PBX module is the central module in the implementation of the server.
It provides the functions listed below, for which more detailed specifications
are given in the header file `pbx.h`.  The folloeing functions are implemented 
in the `src/pbx.c` file.

  * `PBX *pbx_init()`:  Initialize a new PBX.
  * `pbx_shutdown(PBX *pbx)`:  Shut down a PBX.
  * `TU *pbx_register(PBX *pbx, int fd)`:  Register a TU client with a PBX.
  * `int pbx_unregister(PBX *pbx, TU *tu)`:  Unregister a TU from a PBX.
  * `int tu_fileno(TU *tu)`: Get the file descriptor for the network connection underlying a TU.
  * `int tu_extension(TU *tu)`: Get the extension number for a TU.
  * `int tu_pickup(TU *tu)`: Take a TU receiver off-hook (i.e. pick up the handset).
  * `int tu_hangup(TU *tu)`: Hang up a TU (i.e. replace the handset on the switchhook).
  * `int tu_dial(TU *tu, int ext)`: Dial an extension on a TU.
  * `int tu_chat(TU *tu, char *msg)`: "Chat" over a connection.

The PBX module will need to maintain a registry of connected clients and
manage the TU objects associated with these clients.
The PBX will need to be able to map each extension number to the associated TU object.
It will need to maintain, for each TU, the file descriptor of the underlying network
connection and the current state of the TU.
It will need to use the file descriptor to issue a response to the client,
as well as any required asynchronous notifications, whenever one of the `tu_xxx` functions
is called.
Finally, as the PBX and TU objects will be accessed concurrently by multiple threads,
the PBX module will need to provide appropriate synchronization (for example, using
mutexes and/or semaphores) to ensure correct and reliable operation.
Changes to the state of a TU will require exclusive access to the TU.
Some operations require simultaneous changes of state of two TUs; these will require
exclusive access to both TUs at the same time in order to ensure the simultaneity of
the state changes.
Sending responses to the client underlying a TU will require exclusive access to the
client file descriptor, in order to ensure that messages sent by separate threads
are serialized over the network connection, rather than intermingled.
Finally, the `pbx_shutdown()` function is required to shut down the network connections
to all registered clients (the `shutdown(2)` function can be used to shut down a socket
for reading, writing, or both, without closing the associated file descriptor)
and it is then required to wait for all the client service threads to unregister
the associated TUs before returning.  Consider using a semaphore, possibly in conjunction
with additional bookkeeping variables, for this purpose.

## Stress Test Exerciser

A test exerciser was provided that you can use to test
the implementation.  The code for it is in `util/tester.c`.
It can be build using `make util/tester` and then run it as `util/tester`.
It accepts the following command-line arguments:

  * `-h <hostname>`
    Specify the hostname to use when connecting to the server.  The default is `localhost`.
  * `-p <port #>`
    Specify the port number to use when connecting to the server.  The default is `3333`.
  * `-l <length of test>`
    Specify the length of the test, in terms of the number of commands to be sent to the server.
    The default is `0`, which means no limit.
  * `-x <min extension #>`
    Specify the minimum extension number that will be dialed during a test.  The default is `4`.
  * `-y <max extension #>`
    Specify the maximum extension number that will be dialed during a test.  The default is `5`.
  * `-d <microseconds>`
    Specify the basic delay time in microseconds.  The default is `100000` (0.1 seconds).
    Setting a smaller value will result in a more rapidly executing test.
    Setting a larger value will slow down the test.

The tester contains a table that determines the probabilities of the various actions to
be taken in each possible state, as well as a table used to check whether a particular
state transition is valid for the current state.