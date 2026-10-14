#include "person_filter.h"
#include <utility>

namespace wt {
void PersonFilter::configure(bool includeOnly, PersonRules rules) {
    includeOnly_ = includeOnly;
    persistent_ = std::move(rules);
}
void PersonFilter::set(SpeakerKey key, const std::string& uid, std::string name, FilterAction action) {
    if (!uid.empty()) {
        session_.erase(key);
        if (action == FilterAction::Clear) persistent_.erase(uid);
        else persistent_[uid] = {std::move(name), action};
    } else {
        if (action == FilterAction::Clear) session_.erase(key);
        else session_[key] = {std::move(name), action};
    }
}
std::optional<FilterAction> PersonFilter::rule(SpeakerKey key, const std::string& uid) const {
    if (!uid.empty()) {
        const auto it = persistent_.find(uid);
        if (it != persistent_.end()) return it->second.action;
    }
    const auto it = session_.find(key);
    if (it != session_.end()) return it->second.action;
    return std::nullopt;
}
bool PersonFilter::allows(const Speaker& speaker) const {
    const auto action = rule(speaker.key, speaker.uniqueId);
    return action ? *action == FilterAction::Include : !includeOnly_;
}
bool PersonFilter::rename(const std::string& uid, const std::string& name) {
    const auto it = persistent_.find(uid);
    if (it == persistent_.end() || it->second.nickname == name) return false;
    it->second.nickname = name;
    return true;
}
void PersonFilter::removeClient(SpeakerKey key) { session_.erase(key); }
void PersonFilter::removeServer(std::uint64_t server) {
    for (auto it = session_.begin(); it != session_.end();) {
        if (it->first.server == server) it = session_.erase(it); else ++it;
    }
}
void PersonFilter::clear() { persistent_.clear(); session_.clear(); }
std::vector<FilterEntry> PersonFilter::entries() const {
    std::vector<FilterEntry> result;
    for (const auto& pair : persistent_) result.push_back({pair.first, {}, pair.second});
    for (const auto& pair : session_) result.push_back({{}, pair.first, pair.second});
    return result;
}
}
