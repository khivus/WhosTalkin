#pragma once
#include "speaker_store.h"
#include <map>

namespace wt {
enum class FilterAction { Include, Exclude, Clear };
struct PersonRule {
    std::string nickname;
    FilterAction action = FilterAction::Exclude;
};
using PersonRules = std::map<std::string, PersonRule>;
struct FilterEntry {
    std::string uniqueId; // Empty means a connection-only rule, never written to disk.
    SpeakerKey key;
    PersonRule rule;
};
class PersonFilter {
public:
    void configure(bool includeOnly, PersonRules rules);
    void set(SpeakerKey key, const std::string& uniqueId, std::string name, FilterAction action);
    bool allows(const Speaker& speaker) const;
    std::optional<FilterAction> rule(SpeakerKey key, const std::string& uniqueId) const;
    bool rename(const std::string& uniqueId, const std::string& name);
    void removeClient(SpeakerKey key);
    void removeServer(std::uint64_t server);
    void clear();
    const PersonRules& persistent() const { return persistent_; }
    std::vector<FilterEntry> entries() const;
private:
    bool includeOnly_ = false;
    PersonRules persistent_;
    std::map<SpeakerKey, PersonRule> session_;
};
}
