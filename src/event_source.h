#pragma once
#include "speaker_store.h"
#include <QString>
#include <optional>

namespace wt {
enum class EventType { Talk, RemoveClient, ClearServer, ClientMoved, RefreshClient, Rename, ActiveServer, Connected };
struct Event {
    EventType type = EventType::Talk;
    SpeakerKey key;
    int talkStatus = 0;
    bool incomingWhisper = false;
    std::string nickname;
    std::uint64_t oldChannel = 0;
    std::uint64_t newChannel = 0;
};
struct ClientInfo {
    std::string nickname;
    std::string serverName;
    bool own = false;
    bool outgoingWhisper = false;
    // Only used by explicit snapshot/metadata refresh, never to replace ordered talk callbacks.
    std::optional<bool> talking;
    bool incomingWhisper = false;
    bool suppressed = false;
    std::string uniqueId;
    bool known = true;
};
class EventSource {
public:
    virtual ~EventSource() = default;
    virtual ClientInfo client(SpeakerKey key) = 0;
    virtual std::vector<Speaker> snapshot(std::optional<std::uint64_t> server = std::nullopt) = 0;
    virtual std::uint64_t activeServer() = 0;
    virtual void log(const QString& message) = 0;
};
}
