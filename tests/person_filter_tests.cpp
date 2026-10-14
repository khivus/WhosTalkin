#include "person_filter.h"
#include <cstdlib>
#include <iostream>

using namespace wt;
int checks = 0;
void check(bool good, const char* description) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
int main() {
    PersonFilter filter;
    Speaker a{{1,1}, "Same", "A", SpeechKind::Normal, false, 1, {}, "identity-A"};
    Speaker b{{1,2}, "Same", "A", SpeechKind::IncomingWhisper, false, 2, {}, "identity-B"};
    Speaker again{{2,30}, "New name", "B", SpeechKind::Normal, false, 3, {}, "identity-A"};
    check(filter.allows(a) && filter.allows(b), "default displays all current speakers");
    filter.set(a.key, a.uniqueId, a.nickname, FilterAction::Exclude);
    check(!filter.allows(a) && filter.allows(b), "equal nicknames remain separate identities");
    check(!filter.allows(again), "identity exclusion follows reconnect and another server");
    filter.configure(true, filter.persistent());
    check(!filter.allows(a) && !filter.allows(b), "include-only mode hides unlisted people");
    filter.set(b.key, b.uniqueId, b.nickname, FilterAction::Include);
    check(filter.allows(b) && !filter.allows(a), "include-only displays included identities");
    filter.set(a.key, a.uniqueId, a.nickname, FilterAction::Include);
    check(filter.allows(a) && filter.allows(again), "include replaces exclude for one identity");
    check(filter.rename(a.uniqueId, "Renamed") && filter.persistent().at(a.uniqueId).nickname == "Renamed", "saved label follows nickname changes");
    check(!filter.rename(a.uniqueId, "Renamed"), "unchanged label needs no write");
    filter.set(a.key, a.uniqueId, a.nickname, FilterAction::Clear);
    check(!filter.allows(a), "clearing a rule restores include-only default");
    filter.configure(false, filter.persistent());
    check(filter.allows(a) && filter.allows(b), "default mode allows included and unlisted users");
    Speaker missing{{1,5}, "Unknown identity", "A", SpeechKind::Normal, false, 4, {}, ""};
    filter.set(missing.key, "", missing.nickname, FilterAction::Exclude);
    check(!filter.allows(missing), "missing UID can be excluded for current connection");
    check(!filter.persistent().count(""), "session rules never become persistent UID rules");
    filter.removeClient(missing.key);
    check(filter.allows(missing), "client ID reuse cannot inherit a session exclusion");
    filter.configure(true, filter.persistent());
    filter.set(missing.key, "", missing.nickname, FilterAction::Include);
    check(filter.allows(missing), "missing UID can be included for current connection");
    filter.removeServer(1);
    check(!filter.allows(missing) && filter.allows(b), "server disconnect clears session rules and retains persistent ones");
    filter.set(a.key, a.uniqueId, a.nickname, FilterAction::Exclude);
    filter.removeClient(a.key);
    check(!filter.allows(a), "persistent exclusion survives disconnect");
    Speaker reused = a; reused.uniqueId = "different-person";
    filter.configure(false, filter.persistent());
    check(filter.allows(reused), "new identity with old client ID is a different person");
    const auto saved = filter.persistent();
    PersonFilter loaded; loaded.configure(true, saved);
    check(loaded.allows(b) && !loaded.allows(a), "rules and mode can be restored");
    loaded.clear(); check(loaded.entries().empty() && !loaded.allows(b), "clear all restores current mode's default");
    std::cout << checks << " people filter checks passed\n";
}
