#ifndef PIMID_SOCKET_COMM_H
#define PIMID_SOCKET_COMM_H

#include "common/types.h"
#include <string>
#include <vector>
#include <memory>

namespace pimid {

/**
 * Message types for host-device communication
 */
enum class MessageType {
    OFFLOAD_REQUEST,      // Host -> Device: offload computation
    OFFLOAD_COMPLETE,     // Device -> Host: computation done
    MEMORY_REQUEST,       // Cross-domain memory access
    MEMORY_RESPONSE,      // Memory access response
    SYNC_REQUEST,         // Timing synchronization
    SYNC_ACK,             // Synchronization acknowledgment
    TERMINATE             // Shutdown signal
};

/**
 * Communication message structure
 */
struct CommMessage {
    MessageType type;
    Cycle timestamp;
    SimulationDomain src_domain;
    SimulationDomain dst_domain;

    // Payload data
    std::vector<uint8_t> data;

    // Request-specific fields
    Address addr;
    uint64_t size;
    uint32_t request_id;

    CommMessage() : type(MessageType::SYNC_REQUEST), timestamp(0),
                    src_domain(SimulationDomain::HOST),
                    dst_domain(SimulationDomain::DEVICE),
                    addr(0), size(0), request_id(0) {}
};

/**
 * Socket-based communication layer for host-device co-simulation
 * Enables independent simulation engines to communicate and synchronize
 */
class SocketComm {
public:
    SocketComm(SimulationDomain domain, const std::string& host, int port);
    ~SocketComm();

    // Connection management
    bool initialize();
    void shutdown();
    bool isConnected() const { return connected_; }

    // Message passing
    bool sendMessage(const CommMessage& msg);
    bool receiveMessage(CommMessage& msg);
    bool hasMessage() const;

    // Synchronization primitives
    void synchronize(Cycle cycle);
    Cycle getRemoteCycle() const { return remote_cycle_; }

    // Buffer management
    void flushSendBuffer();
    size_t getPendingMessageCount() const;

private:
    SimulationDomain domain_;
    std::string host_;
    int port_;
    int socket_fd_;
    bool connected_;
    Cycle remote_cycle_;

    // Socket operations
    bool createSocket();
    bool connectSocket();  // For client (device)
    bool bindSocket();     // For server (host)
    bool acceptConnection(); // For server (host)

    // Serialization helpers
    std::vector<uint8_t> serializeMessage(const CommMessage& msg);
    CommMessage deserializeMessage(const std::vector<uint8_t>& data);

    // Buffer for partial messages
    std::vector<uint8_t> recv_buffer_;
};

/**
 * Factory for creating communication channels
 */
class CommFactory {
public:
    static std::unique_ptr<SocketComm> createHostComm(int port);
    static std::unique_ptr<SocketComm> createDeviceComm(
        const std::string& host, int port);
};

} // namespace pimid

#endif // PIMID_SOCKET_COMM_H
