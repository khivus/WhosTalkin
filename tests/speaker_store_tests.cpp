#include "speaker_store.h"
#include <cstdlib>
#include <iostream>

using namespace wt;
int checks = 0;
void check(bool condition, const char* description) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
int main() {
    SpeakerStore s;
    const SpeakerKey a{1,1}, b{1,2}, c{2,1};
    auto start = [&](SpeakerKey key, SpeechKind kind = SpeechKind::Normal, bool own = false) {
        s.start(key, u8"Same · 日本語", "Server", kind, own);
    };
    check(s.size() == 0, "starts empty");
    start(a); check(s.size() == 1, "one normal start");
    s.stop(a, 0); check(s.size() == 0, "one normal stop is immediate");
    start(a); start(b); check(s.size() == 2, "two simultaneous speakers with identical Unicode nicknames");
    const auto seq = s.find(a)->sequence;
    start(a, SpeechKind::IncomingWhisper);
    check(s.size() == 2 && s.find(a)->kind == SpeechKind::IncomingWhisper, "normal switches to whisper without duplicate");
    check(s.find(a)->sequence == seq, "mode change preserves speaking order");
    s.stop(a, 0); check(s.size() == 1, "whisper removed regardless of stop whisper flag");
    start(a, SpeechKind::IncomingWhisper); s.remove(a);
    check(s.size() == 1, "disconnect while whispering removes immediately");
    s.clear();
    for (int i = 0; i < 10000; ++i) { start(a); s.stop(a, i); }
    check(s.size() == 0, "10000 PTT cycles leave no inactive rows");
    start(a); start(c);
    check(s.visible(1, false, true).size() == 1, "active server A");
    check(s.visible(2, false, true).size() == 1, "active server B retains available activity");
    check(s.visible(1, true, true).size() == 2, "all servers separate identical client IDs");
    s.removeServer(1); check(s.size() == 1 && s.find(c), "server disconnect clears only that server");
    s.clear(); start(a); start(b, SpeechKind::IncomingWhisper);
    s.removeNormalOnServer(1);
    check(!s.find(a) && s.find(b), "local channel change clears normal speech, preserves received whispers");
    s.rename(b, "Renamed"); check(s.find(b)->nickname == "Renamed", "live nickname update");
    s.rename(a, "inactive"); check(s.size() == 1, "nickname update never creates inactive user");
    s.stop(b, 100, 200);
    check(s.nextExpiry() == 300, "optional removal deadline");
    s.expire(299); check(s.find(b), "delay not expired early");
    start(b); s.expire(300); check(s.find(b) && !s.nextExpiry(), "new start cancels stale delayed stop");
    s.stop(b, 400, 200); s.stop(b, 500, 200);
    check(s.nextExpiry() == 600, "duplicate stop does not extend retention");
    s.expire(600); check(s.size() == 0, "delayed stop expires exactly");
    start(a); s.stop(a, 1000, 5000); s.expire(5999);
    check(s.find(a), "five-second retention preserves a brief speaker until the deadline");
    start(a); s.stop(a, 6000, 5000); s.expire(10999);
    check(s.find(a), "restarted speech receives a fresh five-second retention period");
    s.expire(11000); check(!s.find(a), "five-second retention expires exactly at the new deadline");
    start(a, SpeechKind::Normal, true);
    check(s.visible(1, false, false).empty(), "optional own microphone filter");
    s.stop(a, 0, 200); s.remove(a); check(!s.nextExpiry(), "disconnect bypasses delay");
    check(s.visible(0, false, true).empty(), "no active server has no active rows");
    std::cout << checks << " core checks passed\n";
}
