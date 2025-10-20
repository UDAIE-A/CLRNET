#include "runtime/ScriptRuntime.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

namespace clrnet {
namespace {

bool is_space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

}  // namespace

bool ScriptRuntime::load_from_file(const std::filesystem::path& path, std::string& error_message) {
    error_message.clear();
    commands_.clear();
    metadata_.clear();

    script_path_ = std::filesystem::weakly_canonical(path);

    std::ifstream stream(path);
    if (!stream) {
        error_message = "Unable to open script file: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    metadata_["script.path"] = script_path_.string();
    metadata_["script.directory"] = script_path_.parent_path().string();
    metadata_["script.name"] = script_path_.stem().string();

    return parse_contents(buffer.str(), error_message);
}

ScriptRuntime::ExecutionReport ScriptRuntime::execute(ExecutionOptions options) const {
    ExecutionReport report;
    std::unordered_map<std::string, std::string> state = metadata_;

    std::ostream* output_stream = options.output ? options.output : &std::cout;

    for (const auto& command : commands_) {
        ++report.commands_executed;
        switch (command.type) {
            case ScriptCommand::Type::Print: {
                const std::string message = substitute_variables(command.argument, state);
                report.log.push_back("print -> " + message);
                if (!options.quiet && output_stream) {
                    (*output_stream) << message << '\n';
                }
                break;
            }
            case ScriptCommand::Type::Sleep: {
                const auto milliseconds = std::chrono::milliseconds(command.numeric_value < 0 ? 0 : command.numeric_value);
                if (options.dry_run) {
                    report.log.push_back("sleep " + std::to_string(milliseconds.count()) + "ms (skipped)");
                } else {
                    report.log.push_back("sleep " + std::to_string(milliseconds.count()) + "ms");
                    std::this_thread::sleep_for(milliseconds);
                }
                break;
            }
            case ScriptCommand::Type::Set: {
                const std::string value = substitute_variables(command.value, state);
                state[command.argument] = value;
                report.log.push_back("set " + command.argument + " = " + value);
                break;
            }
            case ScriptCommand::Type::Append: {
                const std::string value = substitute_variables(command.value, state);
                auto& existing = state[command.argument];
                if (!existing.empty()) {
                    existing.append("\n");
                }
                existing.append(value);
                report.log.push_back("append " + command.argument);
                break;
            }
            case ScriptCommand::Type::Fail: {
                const std::string message = substitute_variables(command.argument, state);
                report.log.push_back("fail -> " + message);
                report.error_message = message;
                report.success = false;
                report.final_state = std::move(state);
                return report;
            }
        }
    }

    report.success = true;
    report.final_state = std::move(state);
    return report;
}

std::string ScriptRuntime::describe_command(const ScriptCommand& command) const {
    std::ostringstream oss;
    oss << "[line " << command.line << "] ";
    switch (command.type) {
        case ScriptCommand::Type::Print:
            oss << "print " << command.argument;
            break;
        case ScriptCommand::Type::Sleep:
            oss << "sleep " << command.numeric_value << "ms";
            break;
        case ScriptCommand::Type::Set:
            oss << "set " << command.argument << " = " << command.value;
            break;
        case ScriptCommand::Type::Append:
            oss << "append " << command.argument << " += " << command.value;
            break;
        case ScriptCommand::Type::Fail:
            oss << "fail " << command.argument;
            break;
    }
    return oss.str();
}

std::string ScriptRuntime::trim(std::string_view text) {
    auto begin = std::find_if_not(text.begin(), text.end(), is_space);
    auto end = std::find_if_not(text.rbegin(), text.rend(), is_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::pair<std::string, std::string> ScriptRuntime::split_first_token(std::string_view text) {
    auto begin = std::find_if_not(text.begin(), text.end(), is_space);
    if (begin == text.end()) {
        return {std::string{}, std::string{}};
    }

    auto token_end = std::find_if(begin, text.end(), is_space);
    std::string token(begin, token_end);

    auto remainder_begin = std::find_if_not(token_end, text.end(), is_space);
    std::string remainder;
    if (remainder_begin != text.end()) {
        remainder.assign(remainder_begin, text.end());
    }

    return {token, trim(remainder)};
}

std::string ScriptRuntime::to_lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

bool ScriptRuntime::parse_contents(const std::string& contents, std::string& error_message) {
    std::istringstream stream(contents);
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (trimmed[0] == '@') {
            if (!parse_metadata_line(trimmed.substr(1), line_number, error_message)) {
                return false;
            }
            continue;
        }

        auto command = parse_command_line(trimmed, line_number, error_message);
        if (!command.has_value()) {
            return false;
        }
        commands_.push_back(std::move(*command));
    }

    if (commands_.empty()) {
        error_message = "The script does not contain any commands.";
        return false;
    }

    return true;
}

std::optional<ScriptCommand> ScriptRuntime::parse_command_line(const std::string& line, std::size_t line_number,
                                                               std::string& error_message) {
    error_message.clear();
    const auto [command_token, remainder] = split_first_token(line);
    if (command_token.empty()) {
        error_message = "Missing command at line " + std::to_string(line_number);
        return std::nullopt;
    }

    const std::string command_name = to_lower(command_token);
    ScriptCommand command;
    command.line = line_number;

    if (command_name == "print" || command_name == "say") {
        if (remainder.empty()) {
            error_message = "print command requires a message at line " + std::to_string(line_number);
            return std::nullopt;
        }
        command.type = ScriptCommand::Type::Print;
        command.argument = remainder;
        return command;
    }

    if (command_name == "sleep" || command_name == "wait") {
        if (remainder.empty()) {
            error_message = "sleep command requires a duration in milliseconds at line " + std::to_string(line_number);
            return std::nullopt;
        }
        try {
            command.numeric_value = std::stoll(remainder);
        } catch (const std::exception&) {
            error_message = "Invalid number supplied to sleep at line " + std::to_string(line_number);
            return std::nullopt;
        }
        command.type = ScriptCommand::Type::Sleep;
        command.argument = remainder;
        return command;
    }

    if (command_name == "set" || command_name == "let") {
        const auto [name, value] = split_first_token(remainder);
        if (name.empty() || value.empty()) {
            error_message = "set command requires a name and a value at line " + std::to_string(line_number);
            return std::nullopt;
        }
        command.type = ScriptCommand::Type::Set;
        command.argument = name;
        command.value = value;
        return command;
    }

    if (command_name == "append") {
        const auto [name, value] = split_first_token(remainder);
        if (name.empty() || value.empty()) {
            error_message = "append command requires a name and a value at line " + std::to_string(line_number);
            return std::nullopt;
        }
        command.type = ScriptCommand::Type::Append;
        command.argument = name;
        command.value = value;
        return command;
    }

    if (command_name == "fail") {
        if (remainder.empty()) {
            error_message = "fail command requires a message at line " + std::to_string(line_number);
            return std::nullopt;
        }
        command.type = ScriptCommand::Type::Fail;
        command.argument = remainder;
        return command;
    }

    error_message = "Unknown command '" + command_token + "' at line " + std::to_string(line_number);
    return std::nullopt;
}

bool ScriptRuntime::parse_metadata_line(const std::string& line, std::size_t line_number, std::string& error_message) {
    const auto [key, value] = split_first_token(line);
    if (key.empty()) {
        error_message = "Metadata key is missing at line " + std::to_string(line_number);
        return false;
    }
    metadata_[key] = value;
    return true;
}

std::string ScriptRuntime::substitute_variables(const std::string& value,
                                                const std::unordered_map<std::string, std::string>& state) const {
    std::string result;
    result.reserve(value.size());

    for (std::size_t index = 0; index < value.size();) {
        if (value[index] == '$' && index + 1 < value.size() && value[index + 1] == '{') {
            const auto closing = value.find('}', index + 2);
            if (closing != std::string::npos) {
                const auto key = value.substr(index + 2, closing - (index + 2));
                const auto it = state.find(key);
                if (it != state.end()) {
                    result.append(it->second);
                } else {
                    result.append("${");
                    result.append(key);
                    result.push_back('}');
                }
                index = closing + 1;
                continue;
            }
        }

        result.push_back(value[index]);
        ++index;
    }

    return result;
}

}  // namespace clrnet
