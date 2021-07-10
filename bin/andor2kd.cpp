#include "andor2k.hpp"
#include "cpp_socket.hpp"

using andor2k::ServerSocket;
using andor2k::Socket;

constexpr int SOCKET_PORT = 8080;

int main() {
    int sock_status;
    try {
        ServerSocket server_sock(SOCKET_PORT);

        Socket child_socket = server_sock.accept(sock_status);
        if (sock_status < 0) {
            fprintf(stderr, "[FATAL] Failed to create child socket ... exiting\n");
            return 1;
        }
    } catch (std::exception& e) {
        fprintf(stderr, "[ERROR] Failed creating deamon\n");
        fprintf(stderr, "[ERROR] what(): %s". e.what());
        fprintf(stderr, "[FATAL] ... exiting\n");
        return 1;
    }
}