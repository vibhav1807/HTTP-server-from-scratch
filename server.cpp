#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <string>

using namespace std;

class Socket {
public:
    Socket() = default;
    explicit Socket(SOCKET fd) : fd_(fd) {}
    ~Socket() { reset(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = INVALID_SOCKET;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET fd() const { return fd_; }
    bool valid() const { return fd_ != INVALID_SOCKET; }

    void reset() {
        if (fd_ != INVALID_SOCKET) {
            ::closesocket(fd_);
            fd_ = INVALID_SOCKET;
        }
    }

private:
    SOCKET fd_ = INVALID_SOCKET;
};

class HttpServer {
public:
    explicit HttpServer(uint16_t port, int backlog = 10);

    bool start();
    void run();

private:
    void handleClient(Socket client, const sockaddr_in& clientAddr);
    static bool sendAll(SOCKET fd, const string& data);
    static string buildResponse();

    static constexpr size_t BUF_SIZE = 4096;

    uint16_t port_;
    int backlog_;
    Socket listener_;
};

static void reportError(const char* what) {
    fprintf(stderr, "%s: Winsock error %d\n", what, WSAGetLastError());
}

HttpServer::HttpServer(uint16_t port, int backlog)
    : port_(port), backlog_(backlog) {}

bool HttpServer::start() {
    listener_ = Socket(::socket(AF_INET, SOCK_STREAM, 0));

    if (!listener_.valid()) {
        reportError("socket");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(
            listener_.fd(),
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        ) < 0) {

        reportError("bind");
        return false;
    }

    if (::listen(listener_.fd(), backlog_) < 0) {
        reportError("listen");
        return false;
    }

    return true;
}

void HttpServer::run() {
    cout << "Listening on http://localhost:" << port_ << "\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);

        Socket client(
            ::accept(
                listener_.fd(),
                reinterpret_cast<sockaddr*>(&clientAddr),
                &len
            )
        );

        if (!client.valid()) {
            if (WSAGetLastError() != WSAEINTR)
                reportError("accept");

            continue;
        }

        handleClient(move(client), clientAddr);
    }
}

void HttpServer::handleClient(Socket client, const sockaddr_in& clientAddr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(
        AF_INET,
        &clientAddr.sin_addr,
        ip,
        sizeof(ip)
    );
    cout << "--- connection from " << ip << ":" << ntohs(clientAddr.sin_port) << " ---\n";
    char buf[BUF_SIZE];
    int n = ::recv(
        client.fd(),
        buf,
        static_cast<int>(sizeof(buf)) - 1,
        0
    );
    if (n < 0) {
        reportError("recv");
        return;
    }
    buf[n] = '\0';
    cout << buf;
    if (!sendAll(
            client.fd(),
            buildResponse()
        )) {

        reportError("send");
    }
}

bool HttpServer::sendAll(SOCKET fd, const string& data) {
    size_t sent = 0;

    while (sent < data.size()) {
        int n = ::send(
            fd,
            data.data() + sent,
            static_cast<int>(data.size() - sent),
            0
        );
        if (n < 0) {
            if (WSAGetLastError() == WSAEINTR)
                continue;

            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

string HttpServer::buildResponse() {
    const string body = "Hello from my C++ HTTP server!\n";

    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/plain\r\n"
           "Content-Length: " + to_string(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}

static int runServer() {
    HttpServer server(8080);

    if (!server.start())
        return EXIT_FAILURE;

    server.run();
    return EXIT_SUCCESS;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    WSADATA wsaData;
    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0) {
        fprintf(
            stderr,
            "WSAStartup failed\n"
        );
        return EXIT_FAILURE;
    }

    int result = runServer();
    WSACleanup();
    return result;
}