#include "communication/socket_comm.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <poll.h>

namespace pimid {

// SocketComm implementation
SocketComm::SocketComm(SimulationDomain domain, const std::string& host, int port)
    : domain_(domain), host_(host), port_(port), socket_fd_(-1),
      connected_(false), remote_cycle_(0) {}

SocketComm::~SocketComm() {
    shutdown();
}

bool SocketComm::initialize() {
    if (!createSocket()) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    if (domain_ == SimulationDomain::HOST) {
        // Host acts as server
        if (!bindSocket()) {
            std::cerr << "Failed to bind socket" << std::endl;
            return false;
        }
        if (!acceptConnection()) {
            std::cerr << "Failed to accept connection" << std::endl;
            return false;
        }
    } else {
        // Device acts as client
        if (!connectSocket()) {
            std::cerr << "Failed to connect to host" << std::endl;
            return false;
        }
    }

    connected_ = true;
    std::cout << (domain_ == SimulationDomain::HOST ? "Host" : "Device")
              << " communication initialized on port " << port_ << std::endl;
    return true;
}

void SocketComm::shutdown() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool SocketComm::sendMessage(const CommMessage& msg) {
    if (!connected_) {
        std::cerr << "Not connected, cannot send message" << std::endl;
        return false;
    }

    // Serialize the message
    std::vector<uint8_t> data = serializeMessage(msg);

    // Send message size first (4 bytes)
    uint32_t msg_size = static_cast<uint32_t>(data.size());
    uint32_t msg_size_network = htonl(msg_size);

    ssize_t sent = send(socket_fd_, &msg_size_network, sizeof(msg_size_network), 0);
    if (sent != sizeof(msg_size_network)) {
        std::cerr << "Failed to send message size" << std::endl;
        return false;
    }

    // Send the actual message data
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        sent = send(socket_fd_, data.data() + total_sent,
                   data.size() - total_sent, 0);
        if (sent <= 0) {
            std::cerr << "Failed to send message data" << std::endl;
            return false;
        }
        total_sent += sent;
    }

    return true;
}

bool SocketComm::receiveMessage(CommMessage& msg) {
    if (!connected_) {
        std::cerr << "Not connected, cannot receive message" << std::endl;
        return false;
    }

    // Receive message size first
    uint32_t msg_size_network;
    ssize_t received = recv(socket_fd_, &msg_size_network,
                           sizeof(msg_size_network), MSG_WAITALL);
    if (received != sizeof(msg_size_network)) {
        if (received == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            connected_ = false;
        } else {
            std::cerr << "Failed to receive message size" << std::endl;
        }
        return false;
    }

    uint32_t msg_size = ntohl(msg_size_network);

    // Sanity check on message size
    if (msg_size > 100 * 1024 * 1024) { // 100 MB max
        std::cerr << "Message size too large: " << msg_size << std::endl;
        return false;
    }

    // Receive the actual message data
    std::vector<uint8_t> data(msg_size);
    size_t total_received = 0;
    while (total_received < msg_size) {
        received = recv(socket_fd_, data.data() + total_received,
                       msg_size - total_received, 0);
        if (received <= 0) {
            std::cerr << "Failed to receive message data" << std::endl;
            return false;
        }
        total_received += received;
    }

    // Deserialize the message
    msg = deserializeMessage(data);

    // Update remote cycle if this is a sync message
    if (msg.type == MessageType::SYNC_REQUEST ||
        msg.type == MessageType::SYNC_ACK) {
        remote_cycle_ = msg.timestamp;
    }

    return true;
}

bool SocketComm::hasMessage() const {
    if (!connected_ || socket_fd_ < 0) {
        return false;
    }

    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 0); // 0 timeout = non-blocking
    return ret > 0 && (pfd.revents & POLLIN);
}

void SocketComm::synchronize(Cycle cycle) {
    CommMessage sync_msg;
    sync_msg.type = MessageType::SYNC_REQUEST;
    sync_msg.timestamp = cycle;
    sync_msg.src_domain = domain_;
    sync_msg.dst_domain = (domain_ == SimulationDomain::HOST) ?
                          SimulationDomain::DEVICE : SimulationDomain::HOST;

    if (!sendMessage(sync_msg)) {
        std::cerr << "Failed to send sync request" << std::endl;
        return;
    }

    // Wait for sync acknowledgment
    CommMessage ack_msg;
    if (!receiveMessage(ack_msg)) {
        std::cerr << "Failed to receive sync ack" << std::endl;
        return;
    }

    if (ack_msg.type != MessageType::SYNC_ACK) {
        std::cerr << "Expected SYNC_ACK, got " << static_cast<int>(ack_msg.type) << std::endl;
    }
}

void SocketComm::flushSendBuffer() {
    // TCP automatically handles buffering, so this is a no-op
    // Could add explicit flush logic if using application-level buffering
}

size_t SocketComm::getPendingMessageCount() const {
    // For simple implementation, check if there's any data available
    return hasMessage() ? 1 : 0;
}

bool SocketComm::createSocket() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof(optval)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    return true;
}

bool SocketComm::connectSocket() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host_ << std::endl;
        return false;
    }

    std::cout << "Device connecting to host at " << host_ << ":" << port_ << "..." << std::endl;

    // Retry connection a few times (host might not be ready yet)
    int max_retries = 10;
    for (int i = 0; i < max_retries; i++) {
        if (connect(socket_fd_, (struct sockaddr*)&server_addr,
                   sizeof(server_addr)) == 0) {
            std::cout << "Device connected to host successfully" << std::endl;
            return true;
        }

        std::cout << "Connection attempt " << (i + 1) << "/" << max_retries
                  << " failed, retrying..." << std::endl;
        sleep(1);
    }

    std::cerr << "Failed to connect after " << max_retries << " attempts: "
              << strerror(errno) << std::endl;
    return false;
}

bool SocketComm::bindSocket() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(socket_fd_, (struct sockaddr*)&server_addr,
            sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        return false;
    }

    if (listen(socket_fd_, 1) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "Host listening on port " << port_ << "..." << std::endl;
    return true;
}

bool SocketComm::acceptConnection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    std::cout << "Waiting for device connection..." << std::endl;

    int client_fd = accept(socket_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
        return false;
    }

    // Close the listening socket and use the client socket
    close(socket_fd_);
    socket_fd_ = client_fd;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "Host accepted connection from " << client_ip << std::endl;

    return true;
}

std::vector<uint8_t> SocketComm::serializeMessage(const CommMessage& msg) {
    std::vector<uint8_t> data;

    // Reserve space for fixed fields
    data.reserve(sizeof(MessageType) + sizeof(Cycle) +
                 sizeof(SimulationDomain) * 2 + sizeof(Address) +
                 sizeof(uint64_t) + sizeof(uint32_t) +
                 sizeof(uint32_t) + msg.data.size());

    // Helper lambda to append data
    auto append = [&data](const void* ptr, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
        data.insert(data.end(), bytes, bytes + size);
    };

    // Serialize fixed fields
    append(&msg.type, sizeof(msg.type));
    append(&msg.timestamp, sizeof(msg.timestamp));
    append(&msg.src_domain, sizeof(msg.src_domain));
    append(&msg.dst_domain, sizeof(msg.dst_domain));
    append(&msg.addr, sizeof(msg.addr));
    append(&msg.size, sizeof(msg.size));
    append(&msg.request_id, sizeof(msg.request_id));

    // Serialize data payload
    uint32_t data_size = static_cast<uint32_t>(msg.data.size());
    append(&data_size, sizeof(data_size));
    if (data_size > 0) {
        data.insert(data.end(), msg.data.begin(), msg.data.end());
    }

    return data;
}

CommMessage SocketComm::deserializeMessage(const std::vector<uint8_t>& data) {
    CommMessage msg;
    size_t offset = 0;

    // Helper lambda to extract data
    auto extract = [&data, &offset](void* ptr, size_t size) {
        if (offset + size > data.size()) {
            throw std::runtime_error("Buffer underflow during deserialization");
        }
        memcpy(ptr, data.data() + offset, size);
        offset += size;
    };

    // Deserialize fixed fields
    extract(&msg.type, sizeof(msg.type));
    extract(&msg.timestamp, sizeof(msg.timestamp));
    extract(&msg.src_domain, sizeof(msg.src_domain));
    extract(&msg.dst_domain, sizeof(msg.dst_domain));
    extract(&msg.addr, sizeof(msg.addr));
    extract(&msg.size, sizeof(msg.size));
    extract(&msg.request_id, sizeof(msg.request_id));

    // Deserialize data payload
    uint32_t data_size;
    extract(&data_size, sizeof(data_size));
    if (data_size > 0) {
        if (offset + data_size > data.size()) {
            throw std::runtime_error("Invalid data size in message");
        }
        msg.data.assign(data.begin() + offset, data.begin() + offset + data_size);
        offset += data_size;
    }

    return msg;
}

// CommFactory implementation
std::unique_ptr<SocketComm> CommFactory::createHostComm(int port) {
    auto comm = std::make_unique<SocketComm>(
        SimulationDomain::HOST, "0.0.0.0", port);
    return comm;
}

std::unique_ptr<SocketComm> CommFactory::createDeviceComm(
    const std::string& host, int port) {
    auto comm = std::make_unique<SocketComm>(
        SimulationDomain::DEVICE, host, port);
    return comm;
}

} // namespace pimid
