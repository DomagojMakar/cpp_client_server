#include <mutex>
#include <thread>
#include <vector>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <arpa/inet.h>
#include <unordered_map>

class TCPServer 
{
public:
    TCPServer(int port);
    ~TCPServer();

    void start();

    int port{ 8080 };
    const int max_clients{ 16 };

private:
    int server_socket { };
    std::vector<std::thread> clientThreads;
    std::unordered_map<std::string, std::vector<int>> topic_subscribers;
    std::mutex topicMutex;

    void initialize_server();
    void handle_connection(int client_socket);
    void handle_client_communication(int client_socket);
    void server_message_received(const std::string& message_received, int client_socket);
    void subscribe_to_topic(int client_socket, const std::string& topic);
    void publish_to_topic(int client_socket, const std::string& topic, const std::string& message);
    void unsubscribe_from_topic(int client_socket, const std::string& topic);
};

TCPServer::TCPServer(int port = 8080) : server_socket{ -1 }, port{ port }  
{
    initialize_server();
}

TCPServer::~TCPServer() 
{
    for (auto& thread : clientThreads) 
    {
        if (thread.joinable()) 
        {
            thread.join();
        }
    }

    if (server_socket != -1) 
    {
        close(server_socket);
    }
}

void TCPServer::initialize_server() 
{
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) 
    {
        std::cerr << "Failed to create server socket\n";
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) 
    {
        std::cerr << "Failed to bind socket\n";
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) 
    {
        std::cerr << "Error listening for connections\n";
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << port << '\n';

    // Initialize topics
    topic_subscribers["speed_topic"];
    topic_subscribers["battery_topic"];
}

void TCPServer::start() 
{
    while (true) 
    {
        // New connection
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) 
        {
            std::cerr << "Accepting connection failed!\n";
            exit(EXIT_FAILURE);
        }

        std::cout << "New client connected\n";
        // New thread for new connection
        clientThreads.emplace_back(&TCPServer::handle_connection, this, client_socket);
    }
}

void TCPServer::handle_connection(int client_socket) 
{
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) 
    {
        // Process the received data to identify subscription and publication requests
        buffer[bytes_read] = '\0';
        std::string client_message(buffer);

        server_message_received(client_message, client_socket);
    }

    // Client disconnected
    std::cout << "Client disconnected\n";

    // Cell removing of the subscriptions here!

    // Close the client socket
    close(client_socket);
}

// client handle is missing
void TCPServer::server_message_received(const std::string &message_received, int client_socket)
{
    if(message_received.rfind("PUBLISH") == 0)
    {
        std::size_t topic_name_position{message_received.find(' ')};
        topic_name_position++;
        
        std::size_t topic_name_end{message_received.find(' ', topic_name_position)};
        std::string topic_name{ message_received.substr(topic_name_position, (topic_name_end - topic_name_position))};
        std::size_t message_received_size{ message_received.size() };
        
        // Check if the message ends in cr/nl, if not, the receiving terminal will stay on the same line 
        std::string message_to_be_published{ message_received.substr(topic_name_end + 1, (message_received_size - (topic_name_end + 1)))};
        if(message_received.find("\r\n") == message_received.npos)
        {
            message_to_be_published.append("\r\n\0");
        }

        publish_to_topic(client_socket, topic_name, message_to_be_published);
    }
    else if(message_received.rfind("SUBSCRIBE") == 0)
    {
        std::cout << "Received subscribe message\n";

        // find where space is, topic name is after it
        std::size_t topic_name_position{message_received.find(' ')};
        topic_name_position++;

        std::size_t message_received_size = message_received.size();

        // shave unnecessary end from the message if it was received!!
        for(int i = message_received.size() - 1; i > 0 && !(std::isalnum(message_received[i])); --i)
        {
            message_received_size--;
        }


        std::string received_topic{ message_received.substr(topic_name_position, (message_received_size - topic_name_position))};

        subscribe_to_topic(client_socket, received_topic);
        // Subscribe the client to the topic
    }
    else if(message_received.rfind("UNSUBSCRIBE") == 0)
    {
        std::cout << "Unsubscribe command called\n";
        std::size_t topic_name_position{message_received.find(' ')};
        topic_name_position++;
        std::string received_topic{ message_received.substr(topic_name_position, (message_received.size() - topic_name_position))};

        unsubscribe_from_topic(client_socket, received_topic);
    }
    else
    {
        std::cout << "Unknown command received! Command:\"" << message_received << "\"\n";
    }
}

void TCPServer::subscribe_to_topic(int client_socket, const std::string& topic) 
{

    if(topic_subscribers.find(topic) == topic_subscribers.end())
    {
        std::cout << "Client tried to subscribe to non-existing topic! TOPIC: " << topic << '\n';
    }
  
    else
    {
        std::lock_guard<std::mutex> lock(topicMutex);

        if(std::find(topic_subscribers[topic].begin(), topic_subscribers[topic].end(), client_socket) 
            != topic_subscribers[topic].end())
            {
                std::cout << "Client already subscribed to the topic!\n";
            }
        else
        {
            topic_subscribers[topic].push_back(client_socket);
            std::cout << "Client subscribed to topic: " << topic << '\n';
        }
    }
}

void TCPServer::unsubscribe_from_topic(int client_socket, const std::string& topic)
{
    std::lock_guard<std::mutex> lock(topicMutex);

    auto topic_data = topic_subscribers.find(topic);
    if(topic_data == topic_subscribers.end())
    {
        std::cout << "Client tried unsubscribing from topic that doesn't exist!\n";
        return;
    }
    else
    {
        auto list_of_subs = topic_data->second;
        auto element_to_be_removed = std::find(list_of_subs.begin(), list_of_subs.end(), client_socket);

        if(element_to_be_removed == list_of_subs.end())
        {
            std::cout << "Client was not subscribed to the topic!\n";
        }
        else
        {
            topic_subscribers[topic].erase(element_to_be_removed);
        }
    }
}

void TCPServer::publish_to_topic(int client_socket, const std::string& topic, const std::string& message) 
{
    std::string full_message{ "[Message] Topic: " + topic + " Data: " + message };
    std::lock_guard<std::mutex> lock(topicMutex);
    // TDL: WHen client disconnects -message remove the subscriptions?
    auto subscriber_iterator = topic_subscribers.find(topic);
    if (subscriber_iterator != topic_subscribers.end()) 
    {
        for (int subscriber : subscriber_iterator->second) 
        {
            if (subscriber != client_socket) 
            {
                // Avoid sending the message back to the client who published it
                send(subscriber, full_message.c_str(), full_message.size(), 0);
            }
        }
        std::cout << "Message published\n";
    }
    else
    {
        std::cout << "Client tried to publish to a non-existent topic!\n";
    }
}

int main(int argc, char** argv) 
{
    int port{ -1 }; 

    if(argc > 1)
    {
        try
        {
            port = std::stoi(argv[1]);
        }
        catch(...)
        {
            std::cerr << "Argument passed to the program must be an integer!\n";
            return 1;
        }
    }
    else
    {
        port = 8080;
    }

    TCPServer server{ port };
    server.start();

    return 0;
}
