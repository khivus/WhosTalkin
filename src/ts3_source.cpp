#include "ts3_source.h"
#include "teamspeak/public_errors.h"

namespace wt {
std::string Ts3Source::nickname(SpeakerKey key) {
    char* name = nullptr;
    if (f_.getClientVariableAsString && f_.freeMemory &&
        f_.getClientVariableAsString(key.server, key.client, CLIENT_NICKNAME, &name) == ERROR_ok && name) {
        const std::string result(name);
        f_.freeMemory(name);
        return result;
    }
    return "Client " + std::to_string(key.client);
}
std::string Ts3Source::serverName(std::uint64_t server) {
    char* name = nullptr;
    if (f_.getServerVariableAsString && f_.freeMemory &&
        f_.getServerVariableAsString(server, VIRTUALSERVER_NAME, &name) == ERROR_ok && name) {
        const std::string result(name);
        f_.freeMemory(name);
        return result;
    }
    return "Server " + std::to_string(server);
}
std::optional<int> Ts3Source::integer(SpeakerKey key, std::size_t flag) {
    int result = 0;
    if (f_.getClientVariableAsInt && f_.getClientVariableAsInt(key.server, key.client, flag, &result) == ERROR_ok)
        return result;
    return std::nullopt;
}
ClientInfo Ts3Source::client(SpeakerKey key) {
    ClientInfo result;
    result.nickname = nickname(key);
    result.serverName = serverName(key.server);
    anyID own = 0;
    result.own = f_.getClientID && f_.getClientID(key.server, &own) == ERROR_ok && own == key.client;
    if (const auto value = integer(key, CLIENT_FLAG_TALKING)) result.talking = *value == STATUS_TALKING;
    int whisper = 0;
    if (f_.isReceivingWhisper && f_.isReceivingWhisper(key.server, key.client, &whisper) == ERROR_ok)
        result.incomingWhisper = whisper != 0;
    if (result.own && f_.isWhispering && f_.isWhispering(key.server, key.client, &whisper) == ERROR_ok)
        result.outgoingWhisper = whisper != 0;
    for (const auto flag : {CLIENT_INPUT_MUTED, CLIENT_OUTPUT_MUTED}) {
        if (const auto value = integer(key, flag); value && *value != 0) result.suppressed = true;
    }
    if (const auto value = integer(key, CLIENT_IS_MUTED); !result.own && value && *value != 0)
        result.suppressed = true;
    if (result.own) {
        if (const auto value = integer(key, CLIENT_INPUT_HARDWARE); value && *value == 0)
            result.suppressed = true;
    }
    return result;
}
std::vector<Speaker> Ts3Source::snapshot(std::optional<std::uint64_t> requested) {
    std::vector<Speaker> result;
    if (!f_.getServerConnectionHandlerList || !f_.getClientList || !f_.freeMemory) return result;
    uint64* servers = nullptr;
    if (f_.getServerConnectionHandlerList(&servers) != ERROR_ok || !servers) return result;
    for (std::size_t i = 0; servers[i]; ++i) {
        if (requested && servers[i] != *requested) continue;
        anyID* clients = nullptr;
        if (f_.getClientList(servers[i], &clients) != ERROR_ok || !clients) continue;
        for (std::size_t j = 0; clients[j]; ++j) {
            const SpeakerKey key{servers[i], clients[j]};
            const auto talking = integer(key, CLIENT_FLAG_TALKING);
            if (!talking || *talking != STATUS_TALKING) continue;
            const auto info = client(key);
            if (!info.talking || !*info.talking || info.suppressed) continue;
            const auto kind = info.incomingWhisper ? SpeechKind::IncomingWhisper
                : (info.own && info.outgoingWhisper ? SpeechKind::OutgoingWhisper : SpeechKind::Normal);
            result.push_back({key, info.nickname, info.serverName, kind, info.own, 0, std::nullopt});
        }
        f_.freeMemory(clients);
    }
    f_.freeMemory(servers);
    return result;
}
std::uint64_t Ts3Source::activeServer() {
    return f_.getCurrentServerConnectionHandlerID ? f_.getCurrentServerConnectionHandlerID() : 0;
}
void Ts3Source::log(const QString& message) {
    if (f_.logMessage) f_.logMessage(message.toUtf8().constData(), LogLevel_WARNING, "WhosTalkin", 0);
}
}
