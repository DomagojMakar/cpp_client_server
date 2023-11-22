# cpp_client_server

## Introduction
Simple client/server application written in C++. The client side is implemented more in C style, while the server side is designed as an object-oriented program to enhance maintainability and extensibility.

## Features
### Client
- Accepts commands via CLI to interact with the server.
  - `CONNECT port client_name`: Connects the client to the specified port.
  - `DISCONNECT`: Disconnects the client from the port.
  - `PUBLISH topic_name data`: Publishes a message to the specified topic.
  - `SUBSCRIBE topic_name`: Subscribes the client to the specified topic.
  - `UNSUBSCRIBE topic_name`: Unsubscribes the client from the specified topic.
  - `HELP`: Displays available commands.
  - `EXIT`: Closes the client program.

### Server
- Maintains a list of topics and subscribers.
- Allows clients to send and receive messages based on topic subscriptions.
- Provides short diagnostic messages about received messages.

## Prerequisite
A C++17 or newer compiler (e.g., GCC, Clang, MSVC).

## Running
Compile the client and server files with a preferred compiler.

- Client: `g++ client.cpp -o client`
- Server: `g++ server.cpp -o server`

Run the client: `./client`

Run the server: `./server <port>` (replace `<port>` with an actual port number).

## License
[GNU License](LICENSE)
