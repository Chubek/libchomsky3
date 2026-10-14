#include "ZethaMEM.hpp"
#include "CliKlyspecBridge.hpp"
#include "cpp-linenoise/linenoise.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
using DB = zethamem::memv2::MemDB;
using Record = zethamem::memv2::Record;
using Value = zethamem::Value;

enum class ExitCode : int {
  Ok = 0,
  Usage = 2,
  Parse = 10,
  Schema = 11,
  Storage = 12,
  Ipc = 13,
  ExitRequested = 99,
};

struct ParseLineResult {
  std::vector<std::string> args{};
  bool needs_more{false};
  std::string error{};
};

ParseLineResult split_ws(const std::string& line) {
  enum class QuoteMode : std::uint8_t { None, Single, Double };
  QuoteMode mode = QuoteMode::None;
  std::vector<std::string> out;
  std::string tok;
  bool escape = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    char ch = line[i];
    if (escape) {
      tok.push_back(ch);
      escape = false;
      continue;
    }
    if (ch == '\\') {
      escape = true;
      continue;
    }
    if (ch == '"' && mode != QuoteMode::Single) {
      mode = (mode == QuoteMode::Double) ? QuoteMode::None : QuoteMode::Double;
      continue;
    }
    if (ch == '\'' && mode != QuoteMode::Double) {
      mode = (mode == QuoteMode::Single) ? QuoteMode::None : QuoteMode::Single;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) && mode == QuoteMode::None) {
      if (!tok.empty()) {
        out.push_back(tok);
        tok.clear();
      }
      continue;
    }
    tok.push_back(ch);
  }
  if (escape) return ParseLineResult{{}, false, "dangling escape"};
  if (mode != QuoteMode::None) return ParseLineResult{{}, true, ""};
  if (!tok.empty()) out.push_back(tok);
  return ParseLineResult{std::move(out), false, ""};
}

std::optional<Value> parse_value(std::string_view s) {
  if (s == "true") return Value{true};
  if (s == "false") return Value{false};
  bool digits = !s.empty();
  bool has_dot = false;
  for (char c : s) {
    if (c == '.') {
      if (has_dot) {
        digits = false;
        break;
      }
      has_dot = true;
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-') {
      digits = false;
      break;
    }
  }
  if (digits) {
    try {
      if (has_dot) return Value{std::stod(std::string(s))};
      return Value{std::int64_t{std::stoll(std::string(s))}};
    } catch (...) {}
  }
  return Value{std::string(s)};
}

Record parse_record(const std::vector<std::string>& kvs, std::size_t start) {
  Record r;
  for (std::size_t i = start; i < kvs.size(); ++i) {
    auto pos = kvs[i].find('=');
    if (pos == std::string::npos || pos == 0) continue;
    auto key = kvs[i].substr(0, pos);
    auto val = parse_value(kvs[i].substr(pos + 1));
    if (val.has_value()) r.fields[key] = *val;
  }
  return r;
}

std::string value_to_string(const Value& v) {
  if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
  if (std::holds_alternative<std::int64_t>(v)) return std::to_string(std::get<std::int64_t>(v));
  if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
  return std::get<std::string>(v);
}

void print_record(const Record& r) {
  bool first = true;
  for (const auto& kv : r.fields) {
    if (!first) std::cout << " ";
    first = false;
    std::cout << kv.first << "=" << value_to_string(kv.second);
  }
  std::cout << "\n";
}

int fail_usage(const std::string& msg) {
  std::cerr << "error[usage]: " << msg << "\n";
  std::cerr << "{\"kind\":\"usage\",\"message\":\"" << msg << "\"}\n";
  return static_cast<int>(ExitCode::Usage);
}

int fail_parse(const std::string& context, const std::string& detail = "") {
  std::cerr << "error[parse]: " << context;
  if (!detail.empty()) std::cerr << " detail=" << detail;
  std::cerr << "\n";
  std::cerr << "{\"kind\":\"parse\",\"context\":\"" << context << "\",\"detail\":\"" << detail << "\"}\n";
  return static_cast<int>(ExitCode::Parse);
}

int fail_storage(const std::string& context, const std::string& detail = "") {
  std::cerr << "error[storage]: " << context;
  if (!detail.empty()) std::cerr << " detail=" << detail;
  std::cerr << "\n";
  std::cerr << "{\"kind\":\"storage\",\"context\":\"" << context << "\",\"detail\":\"" << detail << "\"}\n";
  return static_cast<int>(ExitCode::Storage);
}

int fail_ipc(const std::string& context, const std::string& detail = "") {
  std::cerr << "error[ipc]: " << context;
  if (!detail.empty()) std::cerr << " detail=" << detail;
  std::cerr << "\n";
  std::cerr << "{\"kind\":\"ipc\",\"context\":\"" << context << "\",\"detail\":\"" << detail << "\"}\n";
  return static_cast<int>(ExitCode::Ipc);
}

struct App {
  DB db{};
  bool opened = true;

  int dispatch(const std::vector<std::string>& argv) {
    if (argv.empty()) return 0;
    const std::string cmd = argv[0];
    if (!zetha_cli_is_known("mem", cmd)) return fail_usage("unknown command: " + cmd);
    if (cmd == "help") {
      std::cout << "commands: help create open put get erase query dump load close exit\n";
      return 0;
    }
    if (cmd == "create") {
      db = DB(zethamem::memv2::Mode::PureRAM);
      opened = true;
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "open") {
      if (argv.size() < 2) return fail_usage("open requires <path>");
      db = DB(zethamem::memv2::Mode::MMapBacked);
      auto r = db.open_mmap(argv[1]);
      if (r.is_err()) return fail_storage("open");
      opened = true;
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "put") {
      if (!opened || argv.size() < 3) return fail_usage("put requires <path> <k=v>...");
      auto rec = parse_record(argv, 2);
      auto r = db.put(argv[1], std::move(rec));
      if (r.is_err()) return fail_storage("put");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "get") {
      if (!opened || argv.size() < 2) return fail_usage("get requires <path>");
      auto r = db.get(argv[1]);
      if (r.is_err()) return fail_storage("get");
      if (!r.unwrap().has_value()) {
        std::cout << "not-found\n";
      } else {
        print_record(*r.unwrap());
      }
      return 0;
    }
    if (cmd == "erase") {
      if (!opened || argv.size() < 2) return fail_usage("erase requires <path>");
      auto r = db.erase(argv[1]);
      if (r.is_err()) return fail_storage("erase");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "query") {
      if (!opened || argv.size() < 2) return fail_usage("query requires <query-text>");
      std::string q = argv[1];
      auto r = db.query(q);
      if (r.is_err()) return fail_parse("query");
      std::cout << "rows=" << r.unwrap().size() << "\n";
      for (const auto& row : r.unwrap()) {
        std::cout << row.first << " ";
        print_record(row.second);
      }
      return 0;
    }
    if (cmd == "dump") {
      if (!opened || argv.size() < 2) return fail_usage("dump requires <path>");
      auto r = db.dump_to_file(argv[1]);
      if (r.is_err()) return fail_ipc("dump");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "load") {
      if (!opened || argv.size() < 2) return fail_usage("load requires <path>");
      auto r = db.load_from_file(argv[1]);
      if (r.is_err()) return fail_ipc("load");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "close") {
      db = DB(zethamem::memv2::Mode::PureRAM);
      opened = false;
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "exit" || cmd == "quit") return static_cast<int>(ExitCode::ExitRequested);
    return fail_usage("unknown command: " + cmd);
  }
};

} // namespace

int main(int argc, char** argv) {
  App app;
  if (argc == 3 && std::string(argv[1]) == "--script") {
    std::ifstream in(argv[2]);
    if (!in) return fail_storage("script", "open");
    std::string line_s;
    std::string pending;
    bool continuing = false;
    while (std::getline(in, line_s)) {
      if (line_s.empty() || line_s[0] == '#') continue;
      const std::string candidate = continuing ? (pending + "\n" + line_s) : line_s;
      auto parsed = split_ws(candidate);
      if (!parsed.error.empty()) return fail_usage(parsed.error);
      if (parsed.needs_more) {
        pending = candidate;
        continuing = true;
        continue;
      }
      pending.clear();
      continuing = false;
      int rc = app.dispatch(parsed.args);
      if (rc == static_cast<int>(ExitCode::ExitRequested)) return 0;
      if (rc != 0) return rc;
    }
    if (continuing) return fail_usage("unterminated quote");
    return 0;
  }
  if (argc > 1) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    int rc = app.dispatch(args);
    return rc == static_cast<int>(ExitCode::ExitRequested) ? 0 : rc;
  }

  linenoise::linenoiseState line("zetha-mem> ");
  line.LoadHistory(".zetha-mem.history");
  while (true) {
    std::string in;
    if (line.Readline(in)) break;
    if (in.empty()) continue;
    std::string full = in;
    while (true) {
      auto parsed = split_ws(full);
      if (!parsed.error.empty()) {
        const int rc = fail_usage(parsed.error);
        if (rc != 0) break;
      }
      if (!parsed.needs_more) {
        line.AddHistory(full.c_str());
        const int rc = app.dispatch(parsed.args);
        if (rc == static_cast<int>(ExitCode::ExitRequested)) goto done;
        break;
      }
      std::string cont;
      linenoise::linenoiseState cont_line("... ");
      if (cont_line.Readline(cont)) {
        (void) fail_usage("unterminated quote");
        break;
      }
      full += "\n";
      full += cont;
    }
  }
done:
  line.SaveHistory(".zetha-mem.history");
  return 0;
}
