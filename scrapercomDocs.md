# Program Flow

## Overview

1. The program has FIFOs for connecting to the webserver to get user requests from there. 

2. The program has a socket for whichever computer is the scraper. This
24 hour "socketman" should be able to open this program and connect
their scraper to the socketman.
The socketman's scraper needs to be able to fulfill the queries:
- search <Topic>
- get_links <Topic> <smart Topic>
- blog_data <URL>

---

## AI Code Overview

**Initialization**:

 The program starts by creating and opening two FIFOs: one for reading (IFIFOPATH_STR) and one for writing (OFIFOPATH_STR). It then creates a TCP socket and binds it to a port. The server begins listening for incoming client connections.

**Waiting for Client Connections**:

 The server waits for a client to connect using the wait_for_socket() function. Once the client connects, the server can begin interacting with it.

**Processing Queries**:

 The server listens for incoming queries from the FIFO. The queries are read asynchronously using ping(). The program identifies the type of query (e.g., "search", "get_links", etc.) and processes it accordingly. This is where the main logic for handling client requests will go.

**Communication**:

 When a query is received, the server processes the request and sends a response back to the client. This happens over the established socket connection.

**Handling Disconnections**: 

 The client_is_connected() function is used to monitor the client's connection status. If the client disconnects, the server will stop interacting with the client and can attempt to reconnect or wait for a new client.

**Cleanup**:

 When the program terminates, it ensures that all resources are properly cleaned up by closing the FIFOs and the socket using closeAll() and close_socket().