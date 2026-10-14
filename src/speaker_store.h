#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wt {
struct SpeakerKey {
    std::uint64_t server = 0;
    std::uint16_t client = 0;
    bool operator<(const SpeakerKey& other) const;
    bool operator==(const SpeakerKey& other) const;
};
enum class SpeechKind { Normal, IncomingWhisper, OutgoingWhisper };
struct Speaker {
    SpeakerKey key;
    std::string nickname;
    std::string serverName;
    SpeechKind kind = SpeechKind::Normal;
    bool own = false;
    std::uint64_t sequence = 0;
    std::optional<std::int64_t> removeAt;
};
// Owned exclusively by the GUI thread. No Qt or TS3 dependency.
class SpeakerStore {
public:
    void start(SpeakerKey key, std::string name, std::string serverName,
               SpeechKind kind, bool own);
    void stop(SpeakerKey key, std::int64_t now, int delayMs = 0);
    void remove(SpeakerKey key);
    void removeServer(std::uint64_t server);
    void removeNormalOnServer(std::uint64_t server);
    void rename(SpeakerKey key, std::string name);
    void expire(std::int64_t now);
    void clear();
    std::optional<std::int64_t> nextExpiry() const;
    std::vector<Speaker> visible(std::uint64_t activeServer, bool allServers, bool showOwn) const;
    const Speaker* find(SpeakerKey key) const;
    std::size_t size() const { return speakers_.size(); }
private:
    std::map<SpeakerKey, Speaker> speakers_;
    std::uint64_t sequence_ = 0;
};
}
