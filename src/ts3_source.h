#pragma once
#include "event_source.h"
#include "ts3_functions.h"

namespace wt {
class Ts3Source : public EventSource {
public:
    explicit Ts3Source(TS3Functions functions) : f_(functions) {}
    ClientInfo client(SpeakerKey key) override;
    std::vector<Speaker> snapshot(std::optional<std::uint64_t> server) override;
    std::uint64_t activeServer() override;
    void log(const QString& message) override;
private:
    std::optional<std::string> clientText(SpeakerKey key, std::size_t flag);
    std::string serverName(std::uint64_t server);
    std::optional<int> integer(SpeakerKey key, std::size_t flag);
    TS3Functions f_;
};
}
