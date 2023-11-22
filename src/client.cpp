/**
* @file client.cpp
*
* @brief client source file
*/

//--------------------------------- INCLUDES ----------------------------------
#include <limits>
#include <thread>
#include <cstring>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <condition_variable>

//-------------------------------- DATA TYPES ---------------------------------
enum class Message_Status
{
    MESSAGE_VALID,
    MESSAGE_INVALID
};

//------------------------------- GLOBAL DATA ---------------------------------
const char* server_ip = "127.0.0.1";
const int default_server_port = 8080;

int connected_socket = -1;
bool connected = false;

std::condition_variable cv;
std::mutex cv_mutex;

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
/**
 * @brief Prints help - available messages
 * 
 */
static void client_print_help(void);

/**
 * @brief Closes the client program
 * 
 * @param current_socket socket used to disconnect from the server
 */
static void client_exit(int &current_socket);

/**
 * @brief Disconnects the client from the server
 * 
 * @param current_socket socket used to disconnect from the server
 */
static void client_disconnect(int& current_socket);

/**
 * @brief Check if the connect message is valid, prints help if not
 * 
 * @param connect_message connect message
 * @return Message_Status::MESSAGE_VALID if validvalidvalid
 */
static Message_Status client_check_connect_msg(const std::string &connect_message);

/**
 * @brief Check if the subscribe message is valid, prints help if not
 * 
 * @param subscribe_message subscribe message to be sent to the server
 * @return Message_Status::MESSAGE_VALID if valid, 
 */
static Message_Status client_check_subscribe_msg(const std::string &subscribe_message);

/**
 * @brief Check if the unsubscribe message is valid, prints help if not
 * 
 * @param unsubscribe_message unsubscribe message to be sent to the server
 * @return Message_Status 
 */
static Message_Status client_check_unsubscribe_msg(const std::string &unsubscribe_message);

/**
 * @brief Checks if the publish message is valid, prints help if not
 * 
 * @param publish_message publish messge to be sent to the server
 * @return Message_Status::MESSAGE_VALID if the message is valid
 */
static Message_Status client_check_publish_msg(const std::string &publish_message);

/**
 * @brief Connects to the server
 * 
 * @param current_socket variable which holds the information on the current 
 * connection status of the client, if unconnected, variable is -1
 * @param connect_message string message consisting of CONNECT port subscriber_name
 */
static void client_connect(int& current_socket, const std::string& connect_message);

/**
 * @brief Disconnects the client from the server
 * 
 * @param client_socket socket by which the client is connected to the server
 */
static void client_disconnect(int& client_socket);

/**
 * @brief Returns the number of words, not including the command name
 * 
 * @param string string command from which the number of arguments is counted
 * @return number of arguments
 */
static int client_check_number_of_arguments(const std::string &string);

//---------------------------- PRIVATE FUNCTIONS ------------------------------
static int client_check_number_of_arguments(const std::string &string)
{
    std::istringstream string_stream{ string };
    std::string word{ };
    int number_of_arguments{ -1 };

    while(string_stream >> word)
    {
        number_of_arguments++;
    }

    return number_of_arguments;
}

static Message_Status client_check_publish_msg(const std::string &publish_message)
{
    if(client_check_number_of_arguments(publish_message) > 1)
    {
        return Message_Status::MESSAGE_VALID;
    }
    else
    {
        std::cout << "PUBLISH: Wrong number of arguments\n";
        std::cout << "command: PUBLISH <TOPIC_NAME> <TOPIC_DATA>\n";
        std::cout << "example: PUBLISH sample_topic sample topic data\n";

        return Message_Status::MESSAGE_INVALID;
    }
}

static Message_Status client_check_unsubscribe_msg(const std::string &unsubscribe_message)
{
    if(client_check_number_of_arguments(unsubscribe_message) == 1)
    {
        return Message_Status::MESSAGE_VALID;
    }
    else
    {
        std::cout << "UNSUBSCRIBE: Wrong number of arguments\n";
        std::cout << "command: UNSUBSCRIBE <TOPIC_NAME>\n";
        std::cout << "usage: UNSUBSCRIBE sample_topic\n";

        return Message_Status::MESSAGE_INVALID;
    }
}

static Message_Status client_check_subscribe_msg(const std::string &subscribe_message)
{
    if(client_check_number_of_arguments(subscribe_message) == 1)
    {
        return Message_Status::MESSAGE_VALID;
    }
    else
    {
        std::cout << "SUBSCRIBE: Wrong number of arguments\n";
        std::cout << "command: SUBSCRIBE <TOPIC_NAME>\n";
        std::cout << "usage: SUBSCRIBE sample_topic\n";

        return Message_Status::MESSAGE_INVALID;
    }
}

static void client_disconnect(int& current_socket)
{
    if (current_socket == -1) {
        std::cerr << "Client is not connected to the server\n";
        return;
    }

    close(current_socket);
    current_socket = -1;
    std::cout << "Disconnected from the server\n";
}

static Message_Status client_check_connect_msg(const std::string &connect_message)
{
    if(client_check_number_of_arguments(connect_message) == 2)
    {
        return Message_Status::MESSAGE_VALID;
    }
    else
    {
        std::cout << "CONNECT: Wrong number of arguments\n";
        std::cout << "command: CONNECT <PORT> <CLIENT_NAME>\n";
        std::cout << "usage: CONNECT 8080 sample_client_name\n";

        return Message_Status::MESSAGE_INVALID;
    }
}

static void client_print_help(void)
{
    std::cout << "\nList of available commands:\n";
    std::cout << "CONNECT <PORT> <CLIENT_NAME>\n";
    std::cout << "DISCONNECT\n";
    std::cout << "PUBLISH <TOPIC_NAME> <DATA>\n";
    std::cout << "SUBSCRIBE <TOPIC_NAME>\n";
    std::cout << "UNSUBSCRIBE <TOPIC_NAME>\n";
    std::cout << "HELP\n";
    std::cout << "EXIT\n";
}

static void client_exit(int &current_socket)
{
    // Disconnect from the server first
    client_disconnect(current_socket);

    exit(0);
}

static void receive_messages() 
{
    char buffer[1024];
    while (1) {

        // Wait for the connection to be ready
        {
            std::unique_lock<std::mutex> lock(cv_mutex);
            cv.wait(lock, [] { return connected; });
        }

        int bytes_read = recv(connected_socket, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) 
        {
            buffer[bytes_read] = '\0';
            std::cout << buffer << '\n';
        } 
        else if (bytes_read == 0) 
        {
            client_disconnect(connected_socket);
            connected = false;
        } 
        else 
        {
            std::cerr << "Failed to receive data from server\n";
            client_disconnect(connected_socket);
            connected = false;
        }
    }
}

static void client_connect(int& client_socket, const std::string& connect_message) 
{
    if (client_socket != -1) 
    {
        std::cerr << "Client is already connected to the server.\n";
        return;
    }

    int server_port{ };

    std::istringstream message{ connect_message };
    std::string command{ };

    message >> command;

    if(message >> server_port)
    {
        
    }
    else
    {
        std::cerr << "Port number invalid input, must be integer!\n";
        return;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) 
    {
        std::cerr << "Client failed to create socket\n";
        return;
    }

    // Set up server address struct
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) 
    {
        std::cerr << "Server IP address invalid\n";
        close(client_socket);
        client_socket = -1;
        return;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) 
    {
        std::cerr << "Connecting to the server failed!\n";
        close(client_socket);
        client_socket = -1;
        return;
    }

    // Signal receiving thread it can start receiving messages
    {
        std::lock_guard<std::mutex> lock(cv_mutex);
        connected = true;
        cv.notify_one();
    }

    std::cout << "Connected to server on port " << server_port << '\n';
}

static void client_send_message(int client_socket, const std::string& message)
{
    if (client_socket == -1) 
    {
        std::cerr << "Client is not connected, call CONNECT first.\n";
        return;
    }

    send(client_socket, message.c_str(), message.size(), 0);

}

//------------------------------ PUBLIC FUNCTIONS -----------------------------
int main()
{
    std::thread reciever_thread(receive_messages);

    std::cout << "Client application, enter commands\n";
    std::string user_input{ };

    while(1)
    {
        std::getline(std::cin >> std::ws, user_input);

        if(user_input == "HELP")
        {
            client_print_help();
        }
        else if(user_input == "EXIT")
        {
            client_exit(connected_socket);
        }
        else if(user_input == "DISCONNECT")
        {
            client_disconnect(connected_socket);
        }
        else if(user_input.rfind("CONNECT") == 0)
        {
            if(client_check_connect_msg(user_input) == Message_Status::MESSAGE_VALID)
            {
                client_connect(connected_socket, user_input);
            }
        }
        else if(user_input.rfind("PUBLISH") == 0)
        {
            if(client_check_publish_msg(user_input) == Message_Status::MESSAGE_VALID)
            {
                client_send_message(connected_socket, user_input);
            }
        }
        else if(user_input.rfind("SUBSCRIBE") == 0)
        {
            if(client_check_subscribe_msg(user_input) == Message_Status::MESSAGE_VALID)
            {
                client_send_message(connected_socket, user_input);
            }
        }
        else if(user_input.rfind("UNSUBSCRIBE") == 0)
        {
            if(client_check_unsubscribe_msg(user_input) == Message_Status::MESSAGE_VALID)
            {
                client_send_message(connected_socket, user_input);
            }
        }
        else
        {
            std::cout << "Unknown command called!\n";
            client_print_help();

            std::cin.clear();
        }
    }

    reciever_thread.join();

    return 0;
}