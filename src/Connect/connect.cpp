#include "connect.hpp"
#include "httpc.hpp"
#include "httpc_data.hpp"
#include "os_porting.hpp"
#include "tls/tls.hpp"
#include "socket.hpp"
#include "crc32.h"

#include <cmsis_os.h>
#include <log.h>

#include <cassert>
#include <cstring>
#include <debug.h>
#include <cstring>
#include <optional>
#include <variant>

using http::ContentType;
using http::Status;
using std::decay_t;
using std::get;
using std::get_if;
using std::holds_alternative;
using std::is_same_v;
using std::min;
using std::monostate;
using std::nullopt;
using std::optional;
using std::string_view;
using std::variant;
using std::visit;

LOG_COMPONENT_DEF(connect, LOG_SEVERITY_DEBUG);

namespace con {

namespace {

    uint32_t cfg_crc(configuration_t &config) {
        uint32_t crc = 0;
        crc = crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(config.host), strlen(config.host));
        crc = crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(config.token), strlen(config.token));
        crc = crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(&config.port), sizeof config.port);
        crc = crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(&config.tls), sizeof config.tls);
        crc = crc32_calc_ex(crc, reinterpret_cast<const uint8_t *>(&config.enabled), sizeof config.enabled);
        return crc;
    }

    class BasicRequest final : public Request {
    private:
        core_interface &core;
        const printer_info_t &info;
        Action &action;
        HeaderOut hdrs[3];
        bool done = false;
        using RenderResult = variant<size_t, Error>;
        const char *url(const Sleep &) const {
            // Sleep already handled at upper level.
            assert(0);
            return "";
        }
        const char *url(const SendTelemetry &) const {
            return "/p/telemetry";
        }
        const char *url(const Event &) const {
            return "/p/events";
        }

        RenderResult write_body_chunk(SendTelemetry &telemetry, char *data, size_t size) {
            if (telemetry.empty) {
                assert(size > 2);
                memcpy(data, "{}", 2);
                return static_cast<size_t>(2);
            } else {
                device_params_t params = core.get_data();
                httpc_data renderer;
                return renderer.telemetry(params, data, size);
            }
        }

        RenderResult write_body_chunk(Event &event, char *data, size_t size) {
            // TODO: Incremental rendering support, if it doesn't fit into the buffer. Not yet supported.
            switch (event.type) {
            case EventType::Info: {
                httpc_data renderer;
                return renderer.info(info, data, size, event.command_id.value_or(0));
            }
            case EventType::JobInfo: {
                device_params_t params = core.get_data();
                if (event.job_id.has_value() && *event.job_id == params.job_id) {
                    httpc_data renderer;
                    return renderer.job_info(params, data, size, event.command_id.value_or(0));
                }
                // else -> fall through
                // Because we don't have this job ID at hand.
            }
            case EventType::Accepted:
            case EventType::Rejected: {
                // These events are always results of some commant we've received.
                // Checked when accepting the command.
                assert(event.command_id.has_value());
                size_t written = snprintf(data, size, "{\"event\":\"%s\",\"command_id\":%" PRIu32 "}", to_str(event.type), *event.command_id);

                // snprintf returns how much it would _want_ to write
                return min(size - 1 /* Taken up by the final \0 */, written);
            }
            default:
                assert(0);
                return static_cast<size_t>(0);
            }
        }

        Error write_body_chunk(Sleep &, char *, size_t) {
            // Handled at upper level.
            assert(0);
            return Error::INVALID_PARAMETER_ERROR;
        }

    public:
        BasicRequest(core_interface &core, const printer_info_t &info, const configuration_t &config, Action &action)
            : core(core)
            , info(info)
            , action(action)
            , hdrs {
                { "Fingerprint", info.fingerprint },
                { "Token", config.token },
                { nullptr, nullptr }
            } {}
        virtual const char *url() const override {
            return visit([&](auto &action) {
                return this->url(action);
            },
                action);
        }
        virtual ContentType content_type() const override {
            return ContentType::ApplicationJson;
        }
        virtual Method method() const override {
            return Method::Post;
        }
        virtual const HeaderOut *extra_headers() const override {
            return hdrs;
        }
        virtual RenderResult write_body_chunk(char *data, size_t size) override {
            if (done) {
                return 0U;
            } else {
                done = true;
                return visit([&](auto &action) -> RenderResult {
                    return this->write_body_chunk(action, data, size);
                },
                    action);
            }
        }
    };

    // TODO: We probably want to be able to both have a smaller buffer and
    // handle larger responses. We need some kind of parse-as-it-comes approach
    // for that.
    const constexpr size_t MAX_RESP_SIZE = 256;

    using Cache = variant<monostate, tls, socket_con, Error>;
}

class connect::CachedFactory final : public ConnectionFactory {
private:
    const char *hostname = nullptr;
    Cache cache;

public:
    virtual variant<Connection *, Error> connection() override {
        // Note: The monostate state should not be here at this moment, it's only after invalidate and similar.
        if (Connection *c = get_if<tls>(&cache); c != nullptr) {
            return c;
        } else if (Connection *c = get_if<socket_con>(&cache); c != nullptr) {
            return c;
        } else {
            Error error = get<Error>(cache);
            // Error is just one-off. Next time we'll try connecting again.
            cache = monostate();
            return error;
        }
    }
    virtual const char *host() override {
        return hostname;
    }
    virtual void invalidate() override {
        cache = monostate();
    }
    template <class C>
    void refresh(const char *hostname, C &&callback) {
        this->hostname = hostname;
        if (holds_alternative<monostate>(cache)) {
            callback(cache);
        }
        assert(!holds_alternative<monostate>(cache));
    }
    uint32_t cfg_fingerprint = 0;
};

connect::ServerResp connect::handle_server_resp(Response resp) {
    if (resp.content_length() > MAX_RESP_SIZE) {
        return Error::ResponseTooLong;
    }

    // Note: missing command ID is already checked at upper level.
    CommandId command_id = resp.command_id.value();
    // XXX Use allocated string? Figure out a way to consume it in parts?
    uint8_t recv_buffer[MAX_RESP_SIZE];
    size_t pos = 0;

    while (resp.content_length() > 0) {
        const auto result = resp.read_body(recv_buffer + pos, resp.content_length());
        if (holds_alternative<size_t>(result)) {
            pos += get<size_t>(result);
        } else {
            return get<Error>(result);
        }
    }

    const string_view body(reinterpret_cast<const char *>(recv_buffer), pos);

    // Note: Anything of these can result in an "Error"-style command (Unknown,
    // Broken...). Nevertheless, we return a Command, which'll consider the
    // whole request-response pair a successful one. That's OK, because on the
    // lower-level it is - we consumed all the data and are allowed to reuse
    // the connection and all that.
    switch (resp.content_type) {
    case ContentType::TextGcode:
        return Command::gcode_command(command_id, body);
    case ContentType::ApplicationJson:
        return Command::parse_json_command(command_id, body);
    default:;
        // If it's unknown content type, then it's unknown command because we
        // have no idea what to do about it / how to even parse it.
        return Command {
            command_id,
            UnknownCommand {},
        };
    }
}

optional<Error> connect::communicate(CachedFactory &conn_factory) {
    configuration_t config = core.get_connect_config();

    if (!config.enabled) {
        planner.reset();
        osDelay(10000);
        return nullopt;
    }

    auto action = planner.next_action();

    // Handle sleeping first. That one doesn't need the connection.
    if (auto *s = get_if<Sleep>(&action)) {
        osDelay(s->milliseconds);
        return nullopt;
    }

    // Make sure to reconnect if the configuration changes (we ignore the
    // 1:2^32 possibility of collision).
    const uint32_t cfg_fingerprint = cfg_crc(config);
    if (cfg_fingerprint != conn_factory.cfg_fingerprint) {
        conn_factory.cfg_fingerprint = cfg_fingerprint;
        conn_factory.invalidate();
    }

    // Let it reconnect if it needs it.
    conn_factory.refresh(config.host, [&](Cache &cache) {
        Connection *connection;
        if (config.tls) {
            cache.emplace<tls>();
            connection = &std::get<tls>(cache);
        } else {
            cache.emplace<socket_con>();
            connection = &std::get<socket_con>(cache);
        }

        if (const auto result = connection->connection(config.host, config.port); result.has_value()) {
            cache = *result;
        }
    });

    HttpClient http(conn_factory);

    BasicRequest request(core, printer_info, config, action);
    const auto result = http.send(request);

    if (holds_alternative<Error>(result)) {
        planner.action_done(ActionResult::Failed);
        conn_factory.invalidate();
        return get<Error>(result);
    }

    Response resp = get<Response>(result);
    if (!resp.can_keep_alive) {
        conn_factory.invalidate();
    }
    switch (resp.status) {
    // The server has nothing to tell us
    case Status::NoContent:
        planner.action_done(ActionResult::Ok);
        return nullopt;
    case Status::Ok: {
        if (resp.command_id.has_value()) {
            const auto sub_resp = handle_server_resp(resp);
            return visit([&](auto &&arg) -> optional<Error> {
                // Trick out of std::visit documentation. Switch by the type of arg.
                using T = decay_t<decltype(arg)>;

                if constexpr (is_same_v<T, monostate>) {
                    planner.action_done(ActionResult::Ok);
                    return nullopt;
                } else if constexpr (is_same_v<T, Command>) {
                    planner.action_done(ActionResult::Ok);
                    planner.command(arg);
                    return nullopt;
                } else if constexpr (is_same_v<T, Error>) {
                    planner.action_done(ActionResult::Failed);
                    planner.command(Command {
                        resp.command_id.value(),
                        BrokenCommand {},
                    });
                    conn_factory.invalidate();
                    return arg;
                }
            },
                sub_resp);
        } else {
            // We have received a command without command ID
            // There's no better action for us than just throw it away.
            planner.action_done(ActionResult::Refused);
            conn_factory.invalidate();
            return Error::UnexpectedResponse;
        }
    }
    default:
        conn_factory.invalidate();
        // TODO: Figure out if the server somehow refused that instead
        // of failed to process.
        planner.action_done(ActionResult::Failed);
        return Error::UnexpectedResponse;
    }
}

void connect::run() {
    CONNECT_DEBUG("%s", "Connect client starts\n");
    // waits for file-system and network interface to be ready
    //FIXME! some mechanisms to know that file-system and network are ready.
    osDelay(10000);

    CachedFactory conn_factory;

    while (true) {
        // TODO: Deal with the error somehow
        communicate(conn_factory);
    }
}

connect::connect()
    : printer_info(core.get_printer_info()) {}

}
