#include <command.hpp>

#include <catch2/catch.hpp>

using namespace con;
using std::get;
using std::holds_alternative;
using std::string_view;

namespace {

template <class D>
D command_test(const string_view cmd) {
    const auto command = Command::parse_json_command(13, cmd);
    REQUIRE(command.id == 13);
    REQUIRE(holds_alternative<D>(command.command_data));
    return get<D>(command.command_data);
}

}

TEST_CASE("Invalid JSON command") {
    command_test<BrokenCommand>("This is not a JSON");
}

TEST_CASE("Unknown command") {
    command_test<UnknownCommand>("{\"command\": \"SOME_CRAP\"}");
}

TEST_CASE("Array command") {
    command_test<BrokenCommand>("[\"hello\"]");
}

TEST_CASE("Send info command") {
    command_test<SendInfo>("{\"command\": \"SEND_INFO\"}");
}

TEST_CASE("Send info with params") {
    command_test<SendInfo>("{\"command\": \"SEND_INFO\", \"args\": [], \"kwargs\": {}}");
}

TEST_CASE("Send job info") {
    REQUIRE(command_test<SendJobInfo>("{\"command\": \"SEND_JOB_INFO\", \"args\": [42], \"kwargs\": {\"job_id\": 42}}").job_id == 42);
}

TEST_CASE("Send job info - missing args") {
    command_test<BrokenCommand>("{\"command\": \"SEND_JOB_INFO\", \"args\": [], \"kwargs\": {}}");
}
