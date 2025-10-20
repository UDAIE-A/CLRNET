#include "runtime/ScriptRuntime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

void print_banner() {
    std::cout << "CLRNet Script Host" << '\n'
              << "==================" << '\n';
}

void print_usage() {
    std::cout << "Usage:\n"
              << "  clrnet run <script> [--dry-run] [--quiet] [--no-banner] [--set key=value]\n"
              << "  clrnet explain <script>\n"
              << "  clrnet init <path>\n"
              << '\n'
              << "Commands:\n"
              << "  run       Execute a script file.\n"
              << "  explain   Print a human-readable summary of a script.\n"
              << "  init      Generate a starter script at the given path.\n";
}

std::vector<std::string> collect_arguments(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }
    return args;
}

std::string script_display_name(const clrnet::ScriptRuntime& runtime) {
    const auto& metadata = runtime.metadata();
    if (const auto it = metadata.find("name"); it != metadata.end()) {
        return it->second;
    }
    return runtime.script_path().filename().string();
}

int handle_run(const std::vector<std::string>& args) {
    if (args.empty()) {
        print_usage();
        return 1;
    }

    std::string script_path;
    bool dry_run = false;
    bool quiet = false;
    bool show_banner = true;
    std::unordered_map<std::string, std::string> overrides;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& argument = args[index];
        if (!argument.empty() && argument[0] == '-') {
            if (argument == "--dry-run") {
                dry_run = true;
            } else if (argument == "--quiet") {
                quiet = true;
            } else if (argument == "--no-banner") {
                show_banner = false;
            } else if (argument == "--set") {
                if (index + 1 >= args.size()) {
                    std::cerr << "--set requires a key=value pair" << '\n';
                    return 1;
                }
                const auto& pair = args[++index];
                const auto equals = pair.find('=');
                if (equals == std::string::npos) {
                    std::cerr << "--set expects a key=value pair" << '\n';
                    return 1;
                }
                const std::string key = pair.substr(0, equals);
                const std::string value = pair.substr(equals + 1);
                overrides[key] = value;
            } else if (argument.rfind("--set=", 0) == 0) {
                const std::string pair = argument.substr(6);
                const auto equals = pair.find('=');
                if (equals == std::string::npos) {
                    std::cerr << "--set expects a key=value pair" << '\n';
                    return 1;
                }
                const std::string key = pair.substr(0, equals);
                const std::string value = pair.substr(equals + 1);
                overrides[key] = value;
            } else if (argument == "--help" || argument == "-h") {
                print_usage();
                return 0;
            } else {
                std::cerr << "Unknown option: " << argument << '\n';
                return 1;
            }
        } else if (script_path.empty()) {
            script_path = argument;
        } else {
            std::cerr << "Unexpected argument: " << argument << '\n';
            return 1;
        }
    }

    if (script_path.empty()) {
        std::cerr << "No script specified." << '\n';
        return 1;
    }

    const fs::path path(script_path);
    if (!fs::exists(path)) {
        std::cerr << "Script not found: " << path << '\n';
        return 1;
    }

    clrnet::ScriptRuntime runtime;
    std::string error;
    if (!runtime.load_from_file(path, error)) {
        std::cerr << error << '\n';
        return 2;
    }

    if (show_banner && !quiet) {
        print_banner();
        std::cout << "Running script: " << script_display_name(runtime);
        if (dry_run) {
            std::cout << " (dry run)";
        }
        std::cout << '\n' << '\n';
    }

    if (!quiet && !overrides.empty()) {
        std::cout << "Overrides:" << '\n';
        for (const auto& [key, value] : overrides) {
            std::cout << "  " << key << " = " << value << '\n';
        }
        std::cout << '\n';
    }

    clrnet::ScriptRuntime::ExecutionOptions options;
    options.dry_run = dry_run;
    options.quiet = quiet;
    options.output = quiet ? nullptr : &std::cout;
    options.initial_state = std::move(overrides);

    const auto report = runtime.execute(options);
    if (!report.success) {
        std::cerr << "Script failed: " << report.error_message << '\n';
        return 3;
    }

    if (!quiet) {
        std::cout << '\n' << "Completed " << report.commands_executed << " command";
        if (report.commands_executed != 1) {
            std::cout << 's';
        }
        std::cout << "." << '\n';
    }

    return 0;
}

int handle_explain(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cerr << "Usage: clrnet explain <script>" << '\n';
        return 1;
    }

    const fs::path path(args.front());
    if (!fs::exists(path)) {
        std::cerr << "Script not found: " << path << '\n';
        return 1;
    }

    clrnet::ScriptRuntime runtime;
    std::string error;
    if (!runtime.load_from_file(path, error)) {
        std::cerr << error << '\n';
        return 2;
    }

    std::cout << "Script: " << script_display_name(runtime) << '\n';
    for (const auto& [key, value] : runtime.metadata()) {
        std::cout << "  @" << key << " = " << value << '\n';
    }

    std::cout << '\n' << "Commands:" << '\n';
    for (const auto& command : runtime.commands()) {
        std::cout << "  - " << runtime.describe_command(command) << '\n';
    }

    return 0;
}

std::string sample_script_contents() {
    return R"(# Sample CLRNet script
@name Hello CLRNet
@greeting Hello from CLRNet!
print ${greeting}
append greeting Running simple automation steps.
print ${greeting}
sleep 250
print Done!
)";
}

int handle_init(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cerr << "Usage: clrnet init <path>" << '\n';
        return 1;
    }

    fs::path target(args.front());
    fs::path output_path;

    if (!target.has_extension()) {
        if (!fs::exists(target)) {
            fs::create_directories(target);
        }
        output_path = target / "hello.clr";
    } else {
        if (auto parent = target.parent_path(); !parent.empty()) {
            fs::create_directories(parent);
        }
        output_path = target;
    }

    if (fs::exists(output_path)) {
        std::cerr << "File already exists: " << output_path << '\n';
        return 1;
    }

    std::ofstream stream(output_path);
    if (!stream) {
        std::cerr << "Unable to write file: " << output_path << '\n';
        return 1;
    }
    stream << sample_script_contents();

    std::cout << "Created sample script at " << output_path << '\n';
    std::cout << "Run it with: clrnet run " << output_path << '\n';
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    const auto args = collect_arguments(argc, argv);
    if (args.empty()) {
        print_usage();
        return 1;
    }

    const std::string& command = args.front();
    const std::vector<std::string> command_args(args.begin() + 1, args.end());

    if (command == "run") {
        return handle_run(command_args);
    }

    if (command == "explain") {
        return handle_explain(command_args);
    }

    if (command == "init") {
        return handle_init(command_args);
    }

    if (command == "--help" || command == "-h" || command == "help") {
        print_usage();
        return 0;
    }

    std::cerr << "Unknown command: " << command << '\n';
    print_usage();
    return 1;
}
