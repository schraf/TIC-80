// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <tic80.h>
#include <enet/enet.h>

#define TIC_SERVER_MAX_CONNECTIONS 64

void service(ENetHost* host)
{
    ENetEvent event;

    while (enet_host_service(host, &event, 1000) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            {
                printf("Connection from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                event.peer -> data = "Client information";
            } 
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                ENetPacket* packet = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags); 
                enet_host_broadcast(host, 0, packet);
                enet_packet_destroy (event.packet);            
            }
            break;
        
        case ENET_EVENT_TYPE_DISCONNECT:
            {
                printf ("Disconnection from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                event.peer -> data = NULL;
            }
            break;
        }
    }
}

int main(int argc, char **argv)
{
    printf("%s\n", tic80_version());

    if (argc != 2)
    {
        printf("USAGE: %s port\n", argv[0]);
        fprintf(stderr, "ERROR: missing arguments.\n");
        return EXIT_FAILURE;
    }

    if (enet_initialize() != 0)
    {
        fprintf(stderr, "ERROR: failed to initialize network.\n");
        return EXIT_FAILURE;
    }
    
    atexit(enet_deinitialize);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = atoi(argv[1]);

    ENetHost* server = enet_host_create(&address, TIC_SERVER_MAX_CONNECTIONS, 1, 0, 0);

    if (server == NULL)
    {
        fprintf (stderr, "ERROR: failed to create network host.\n");
        exit (EXIT_FAILURE);
    }

    printf("Listening on port %d.\n", address.port);

    while (true)
    {
        service(server);
    }

    printf("Shutting down server.");
    enet_host_destroy(server);

	return 0;
}
