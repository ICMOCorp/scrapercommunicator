# Monotunnel Bi-valve

One wire. One direction.

## Description

We want to share messages across two clients. That's what the 
monotunnel bi-valve does. We only have 1 wire and they need
to share this wire but if they both try to send signals and 
listen then that's chaos.

Mostly this implementation is a protocol, and the code exists
to ensure this protocol is secured.

## Protocol

Say A and B are clients. Clients have 2 features:

1. Keep checking what's in the tunnel. When found something in
tunnel, take it.
2. Put something in the tunnel

If A wants to deliver a message to B, A needs to get rid of feature 
1 and B needs to get rid of feature 2. Once the transaction is done,
we just await for the next transaction.