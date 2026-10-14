#include "ZethaRDB.hpp"
#include "CliKlyspecBridge.hpp"
#include "cpp-linenoise/linenoise.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
using DB = zetha::rdb::ZethaRDB;
using Row = zetha::rdb::Row;
using Value = zetha::rdb::Value;
using Errc = zetha::rdb::RdbErrc;
using Error = zetha::rdb::Error;

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

Value parse_value(std::string_view s) {
  if (s == "true") return Value::Boolean(true);
  if (s == "false") return Value::Boolean(false);
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
      if (has_dot) return Value::Real(std::stod(std::string(s)));
      return Value::Integer(std::stoll(std::string(s)));
    } catch (...) {}
  }
  return Value::Text(std::string(s));
}

Row parse_row(const std::vector<std::string>& kvs, std::size_t start) {
  Row r;
  for (std::size_t i = start; i < kvs.size(); ++i) {
    auto pos = kvs[i].find('=');
    if (pos == std::string::npos || pos == 0) continue;
    r[kvs[i].substr(0, pos)] = parse_value(kvs[i].substr(pos + 1));
  }
  return r;
}

void print_row(const Row& row) {
  bool first = true;
  for (const auto& kv : row) {
    if (!first) std::cout << " ";
    first = false;
    std::cout << kv.first << "=" << kv.second.data;
  }
  std::cout << "\n";
}

int fail_usage(const std::string& msg) {
  std::cerr << "error[usage]: " << msg << "\n";
  std::cerr << "{\"kind\":\"usage\",\"message\":\"" << msg << "\"}\n";
  return static_cast<int>(ExitCode::Usage);
}

int fail_classified(const Error& e, const std::string& context) {
  auto code = ExitCode::Storage;
  const char* kind = "storage";
  switch (e.code) {
    case Errc::QueryParseError:
      code = ExitCode::Parse;
      kind = "parse";
      break;
    case Errc::SchemaError:
      code = ExitCode::Schema;
      kind = "schema";
      break;
    case Errc::IpcError:
      code = ExitCode::Ipc;
      kind = "ipc";
      break;
    default:
      code = ExitCode::Storage;
      kind = "storage";
      break;
  }
  std::cerr << "error[" << kind << "]: " << context;
  if (!e.message.empty()) std::cerr << " detail=" << e.message;
  std::cerr << "\n";
  std::cerr << "{\"kind\":\"" << kind << "\",\"context\":\"" << context << "\",\"detail\":\"" << e.message << "\"}\n";
  return static_cast<int>(code);
}

struct App {
  std::optional<DB> db;
  std::string path;

  int dispatch(const std::vector<std::string>& argv) {
    if (argv.empty()) return 0;
    const std::string cmd = argv[0];
    if (!zetha_cli_is_known("rdb", cmd)) return fail_usage("unknown command: " + cmd);
    if (cmd == "help") {
      std::cout << "commands: create open schema-load put get erase query maintain close exit\n";
      return 0;
    }
    if (cmd == "create") {
      if (argv.size() < 2) return fail_usage("create requires <path> [schema]");
      zetha::rdb::CreateOptions opt{};
      if (argv.size() >= 3) {
        opt.schema_text = argv[2];
        for (std::size_t i = 3; i < argv.size(); ++i) {
          opt.schema_text += " ";
          opt.schema_text += argv[i];
        }
      }
      auto r = DB::create(argv[1], opt);
      if (!r) return fail_classified(*r.error, "create");
      db = std::move(*r.value);
      path = argv[1];
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "open") {
      if (argv.size() < 2) return fail_usage("open requires <path>");
      auto r = DB::open(argv[1], {});
      if (!r) return fail_classified(*r.error, "open");
      db = std::move(*r.value);
      path = argv[1];
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "schema-load") {
      if (!db.has_value() || argv.size() < 2) return fail_usage("schema-load requires <schema-text> or --file <path>");
      std::string schema_text;
      if (argv[1] == "--file") {
        if (argv.size() < 3) return fail_usage("schema-load --file requires <path>");
        std::ifstream in(argv[2]);
        if (!in) return fail_usage("schema file open failed: " + argv[2]);
        schema_text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      } else {
        schema_text = argv[1];
        for (std::size_t i = 2; i < argv.size(); ++i) {
          schema_text += " ";
          schema_text += argv[i];
        }
      }
      auto r = db->init_schema(schema_text);
      if (!r) return fail_classified(*r.error, "schema-load");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "put") {
      if (!db.has_value() || argv.size() < 4) return fail_usage("put requires <table> <id> <k=v>...");
      auto row = parse_row(argv, 3);
      row["id"] = Value::Text(argv[2]);
      auto r = db->insert(argv[1], std::move(row));
      if (!r) return fail_classified(*r.error, "put");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "get") {
      if (!db.has_value() || argv.size() < 3) return fail_usage("get requires <table> <id>");
      auto r = db->get(argv[1], argv[2]);
      if (!r) return fail_classified(*r.error, "get");
      if (!r.value->has_value()) std::cout << "not-found\n";
      else print_row(**r.value);
      return 0;
    }
    if (cmd == "erase") {
      if (!db.has_value() || argv.size() < 3) return fail_usage("erase requires <table> <id>");
      auto r = db->erase(argv[1], argv[2]);
      if (!r) return fail_classified(*r.error, "erase");
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "query") {
      if (!db.has_value() || argv.size() < 2) return fail_usage("query requires <query-text>");
      auto r = db->query(argv[1]);
      if (!r) return fail_classified(*r.error, "query");
      std::cout << "rows=" << r.value->rows.size() << "\n";
      for (const auto& row : r.value->rows) print_row(row);
      return 0;
    }
    if (cmd == "maintain") {
      if (path.empty()) return fail_usage("maintain requires opened database");
      auto opened = zetha::storage::ZethaStorage::open(path, {});
      if (std::holds_alternative<zetha::storage::Error>(opened)) {
        std::cerr << "error[storage]: maintain detail=open failed\n";
        return static_cast<int>(ExitCode::Storage);
      }
      auto st = std::move(std::get<zetha::storage::ZethaStorage>(opened));
      auto rc = st.defragment();
      if (rc.has_value()) {
        std::cerr << "error[storage]: maintain detail=" << rc.value().message << "\n";
        return static_cast<int>(ExitCode::Storage);
      }
      std::cout << "ok\n";
      return 0;
    }
    if (cmd == "close") {
      db.reset();
      path.clear();
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
    if (!in) return fail_usage("script open failed");
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

  linenoise::linenoiseState line("zetha-rdb> ");
  line.LoadHistory(".zetha-rdb.history");
  while (true) {
    std::string in;
    if (line.Readline(in)) break;
    if (in.empty()) continue;
    std::string full = in;
    while (true) {
      auto parsed = split_ws(full);
      if (!parsed.error.empty()) {
        (void) fail_usage(parsed.error);
        break;
      }
      if (!parsed.needs_more) {
        line.AddHistory(full.c_str());
        int rc = app.dispatch(parsed.args);
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
  line.SaveHistory(".zetha-rdb.history");
  return 0;
}
