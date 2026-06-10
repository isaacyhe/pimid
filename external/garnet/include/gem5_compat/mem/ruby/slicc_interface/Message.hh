/**
 * @file Message.hh
 * @brief Abstract message class for network communication
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_SLICC_INTERFACE_MESSAGE_HH__
#define __GARNET_COMPAT_MEM_RUBY_SLICC_INTERFACE_MESSAGE_HH__

#include <iostream>
#include <memory>

#include "base/types.hh"
#include "../common/MachineID.hh"
#include "../common/NetDest.hh"
#include "../common/TypeDefines.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
class Message;
struct Packet;
class WriteMask;

// Message pointer type
using MsgPtr = std::shared_ptr<Message>;

// Packet stub for functional access (not used in standalone mode)
struct Packet {
    uint64_t addr;
    uint8_t* data;
    size_t size;
};

// WriteMask stub
class WriteMask {
public:
    bool empty() const { return true; }
    void clear() {}
};

/**
 * Message - abstract base class for all network messages
 */
class Message {
public:
    Message(Tick curTime = 0)
        : time_(curTime), lastEnqueueTime_(curTime), delayedTicks_(0) {}

    virtual ~Message() = default;

    // Clone the message
    virtual MsgPtr clone() const = 0;

    // Print for debugging
    virtual void print(std::ostream& out) const = 0;

    // Get message type name
    virtual const char* getTypeName() const { return "Message"; }

    // Message timing
    Tick getTime() const { return time_; }
    void setTime(Tick t) { time_ = t; }

    Tick getLastEnqueueTime() const { return lastEnqueueTime_; }
    void setLastEnqueueTime(Tick t) { lastEnqueueTime_ = t; }

    Tick getDelayedTicks() const { return delayedTicks_; }
    void setDelayedTicks(Tick t) { delayedTicks_ = t; }

    // Destination handling
    virtual const NetDest& getDestination() const {
        static NetDest empty;
        return empty;
    }

    virtual NetDest& getDestination() {
        static NetDest empty;
        return empty;
    }

    // Message size type (for bandwidth calculation)
    virtual MessageSizeType getMessageSize() const {
        return MessageSizeType::Control; // Default control message
    }

    // Functional access (for debugging)
    virtual bool functionalRead(Packet* pkt) { return false; }
    virtual bool functionalRead(Packet* pkt, WriteMask& mask) { return false; }
    virtual bool functionalWrite(Packet* pkt) { return false; }
    virtual bool functionalWrite(Packet* pkt, WriteMask& mask) { return false; }

protected:
    Tick time_;
    Tick lastEnqueueTime_;
    Tick delayedTicks_;
};

inline std::ostream& operator<<(std::ostream& out, const Message& msg) {
    msg.print(out);
    return out;
}

/**
 * SimpleMessage - basic message implementation for testing
 */
class SimpleMessage : public Message {
public:
    SimpleMessage(NodeID src = 0, NodeID dst = 0,
                  MessageSizeType size = MessageSizeType::Data,
                  Tick curTime = 0)
        : Message(curTime), src_(src), sizeType_(size) {
        dest_.add(dst);
    }

    MsgPtr clone() const override {
        return std::make_shared<SimpleMessage>(*this);
    }

    void print(std::ostream& out) const override {
        out << "SimpleMessage [src=" << src_ << ", dest=" << dest_
            << ", sizeType=" << static_cast<int>(sizeType_) << "]";
    }

    const char* getTypeName() const override { return "SimpleMessage"; }

    const NetDest& getDestination() const override { return dest_; }
    NetDest& getDestination() override { return dest_; }

    MessageSizeType getMessageSize() const override { return sizeType_; }

    NodeID getSource() const { return src_; }

    void setTag(uint64_t t) { tag_ = t; }
    uint64_t getTag() const { return tag_; }

private:
    NodeID src_;
    NetDest dest_;
    MessageSizeType sizeType_;
    uint64_t tag_ = 0;  // opaque tag for request matching
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_SLICC_INTERFACE_MESSAGE_HH__
