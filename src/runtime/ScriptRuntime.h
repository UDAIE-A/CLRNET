#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clrnet {

struct ScriptCommand {
    enum class Type {
        Print,
        Sleep,
        Set,
        Append,
        Fail
    };

    Type type{Type::Print};
    std::size_t line{0};
    std::string argument;   // command-specific primary argument (e.g., variable name)
    std::string value;      // secondary argument (e.g., value to set)
    std::int64_t numeric_value{0}; // pre-parsed numeric payloads (milliseconds for sleep)
};

class ScriptRuntime {
public:
    struct ExecutionOptions {
        bool dry_run{false};
        bool quiet{false};
        std::ostream* output{nullptr};
    };

    struct ExecutionReport {
        bool success{true};
        std::size_t commands_executed{0};
        std::vector<std::string> log;
        std::string error_message;
        std::unordered_map<std::string, std::string> final_state;
    };

    bool load_from_file(const std::filesystem::path& path, std::string& error_message);

    [[nodiscard]] const std::filesystem::path& script_path() const noexcept { return script_path_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& metadata() const noexcept { return metadata_; }
    [[nodiscard]] const std::vector<ScriptCommand>& commands() const noexcept { return commands_; }

    ExecutionReport execute(ExecutionOptions options) const;
    ExecutionReport execute() const { return execute(ExecutionOptions{}); }

    [[nodiscard]] std::string describe_command(const ScriptCommand& command) const;

private:
    static std::string trim(std::string_view text);
    static std::pair<std::string, std::string> split_first_token(std::string_view text);
    static std::string to_lower(std::string text);

    bool parse_contents(const std::string& contents, std::string& error_message);
    std::optional<ScriptCommand> parse_command_line(const std::string& line, std::size_t line_number,
                                                    std::string& error_message);
    bool parse_metadata_line(const std::string& line, std::size_t line_number, std::string& error_message);

    std::string substitute_variables(const std::string& value,
                                     const std::unordered_map<std::string, std::string>& state) const;

    std::filesystem::path script_path_{};
    std::unordered_map<std::string, std::string> metadata_{};
    std::vector<ScriptCommand> commands_{};
};

}  // namespace clrnet
