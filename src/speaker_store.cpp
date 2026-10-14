#include "speaker_store.h"
#include <algorithm>
#include <tuple>
#include <utility>

namespace wt {
bool SpeakerKey::operator<(const SpeakerKey& other) const {
    return std::tie(server, client) < std::tie(other.server, other.client);
}
bool SpeakerKey::operator==(const SpeakerKey& other) const {
    return server == other.server && client == other.client;
}
void SpeakerStore::start(SpeakerKey key, std::string name, std::string serverName,
                         SpeechKind kind, bool own) {
    auto it = speakers_.find(key);
    if (it == speakers_.end()) {
        speakers_.emplace(key, Speaker{key, std::move(name), std::move(serverName), kind,
                                      own, ++sequence_, std::nullopt});
    } else {
        it->second.nickname = std::move(name);
        it->second.serverName = std::move(serverName);
        it->second.kind = kind;
        it->second.own = own;
        it->second.removeAt.reset(); // A new start cancels an old delayed stop.
    }
}
void SpeakerStore::stop(SpeakerKey key, std::int64_t now, int delayMs) {
    auto it = speakers_.find(key);
    if (it == speakers_.end()) return;
    if (delayMs <= 0) speakers_.erase(it);
    else if (!it->second.removeAt) it->second.removeAt = now + delayMs;
}
void SpeakerStore::remove(SpeakerKey key) { speakers_.erase(key); }
void SpeakerStore::removeServer(std::uint64_t server) {
    for (auto it = speakers_.begin(); it != speakers_.end();) {
        if (it->first.server == server) it = speakers_.erase(it); else ++it;
    }
}
void SpeakerStore::removeNormalOnServer(std::uint64_t server) {
    for (auto it = speakers_.begin(); it != speakers_.end();) {
        if (it->first.server == server && (it->second.kind == SpeechKind::Normal || it->second.own))
            it = speakers_.erase(it);
        else ++it;
    }
}
void SpeakerStore::rename(SpeakerKey key, std::string name) {
    auto it = speakers_.find(key);
    if (it != speakers_.end()) it->second.nickname = std::move(name);
}
void SpeakerStore::expire(std::int64_t now) {
    for (auto it = speakers_.begin(); it != speakers_.end();) {
        if (it->second.removeAt && *it->second.removeAt <= now) it = speakers_.erase(it);
        else ++it;
    }
}
void SpeakerStore::clear() { speakers_.clear(); }
std::optional<std::int64_t> SpeakerStore::nextExpiry() const {
    std::optional<std::int64_t> result;
    for (const auto& pair : speakers_) {
        const auto deadline = pair.second.removeAt;
        if (deadline && (!result || *deadline < *result)) result = deadline;
    }
    return result;
}
std::vector<Speaker> SpeakerStore::visible(std::uint64_t activeServer, bool allServers, bool showOwn) const {
    std::vector<Speaker> result;
    for (const auto& pair : speakers_) {
        if ((allServers || pair.first.server == activeServer) && (showOwn || !pair.second.own))
            result.push_back(pair.second);
    }
    return result;
}
const Speaker* SpeakerStore::find(SpeakerKey key) const {
    const auto it = speakers_.find(key);
    return it == speakers_.end() ? nullptr : &it->second;
}
}
