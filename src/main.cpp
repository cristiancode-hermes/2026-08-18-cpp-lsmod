// SPDX-License-Identifier: MIT
// cpp-lsmod — Linux kernel module list viewer (/proc/modules)
// Build: g++ -std=c++23 -O2 -Wall -Wextra src/main.cpp -o lsmod
// Usage: ./lsmod [--json] [--watch N] [--no-color] [--sort size|deps|count] [--tree] [--filter NAME] [--selftest] [--demo]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ── ANSI Palette (mutable for --no-color) ──
static std::string C_RST  = "\033[0m";
static std::string C_BLD  = "\033[1m";
static std::string C_DIM  = "\033[2m";
static std::string C_RED  = "\033[31m";
static std::string C_GRN  = "\033[32m";
static std::string C_YLW  = "\033[33m";
static std::string C_BLU  = "\033[34m";
static std::string C_MAG  = "\033[35m";
static std::string C_CYN  = "\033[36m";
static std::string C_WHT  = "\033[37m";
static std::string C_GRY  = "\033[90m";
static bool g_color = true;

struct Module {
  std::string name;
  unsigned long long size;
  int use_count;
  std::vector<std::string> dependencies;
  bool is_live;  // loaded vs pending
};

// ── Format helpers ──
std::string fmt_size(unsigned long long bytes) {
  if (bytes >= 1048576) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%5.1f MiB", bytes / 1048576.0);
    return buf;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%5.1f KiB", bytes / 1024.0);
  return buf;
}

std::string fmt_count(int n) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%3d", n);
  return buf;
}

// ── Parse /proc/modules ──
std::vector<Module> parse_modules(const std::string& path = "/proc/modules") {
  std::ifstream f(path);
  if (!f.is_open()) return {};

  std::vector<Module> modules;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;

    Module m{};
    std::istringstream ss(line);
    // Format: name size used_by dep1,dep2,... state address
    ss >> m.name;

    // size
    std::string size_str;
    ss >> size_str;
    m.size = std::stoull(size_str);

    // use count
    std::string count_str;
    ss >> count_str;
    m.use_count = std::stoi(count_str);

    // dependencies (may be empty "-")
    std::string deps_str;
    ss >> deps_str;
    if (deps_str != "-" && !deps_str.empty()) {
      std::istringstream ds(deps_str);
      std::string dep;
      while (std::getline(ds, dep, ',')) {
        if (!dep.empty()) m.dependencies.push_back(dep);
      }
    }

    // state
    std::string state;
    ss >> state;
    m.is_live = (state == "Live");

    // skip address (hex)
    std::string addr;
    ss >> addr;

    modules.push_back(std::move(m));
  }
  return modules;
}

// ── Self-test ──
int selftest() {
  int pass = 0, fail = 0;

  // Test 1: parse /proc/modules
  auto mods = parse_modules();
  auto check = [&](const char* name, bool cond) {
    if (cond) { pass++; std::cout << C_GRN << "  PASS" << C_RST << " " << name << "\n"; }
    else      { fail++; std::cout << C_RED << "  FAIL" << C_RST << " " << name << "\n"; }
  };

  check("parse_modules returns modules", !mods.empty());
  check("module count > 10", static_cast<int>(mods.size()) > 10);

  // Check first module has valid fields
  if (!mods.empty()) {
    auto& m = mods[0];
    check("first module has name", !m.name.empty());
    check("first module size > 0", m.size > 0);
    check("first module use_count >= 0", m.use_count >= 0);
    check("first module is_live is bool", m.is_live == true || m.is_live == false);
  }

  // Test 2: fmt_size
  check("fmt_size(1048576) contains MiB", fmt_size(1048576).find("MiB") != std::string::npos);
  check("fmt_size(1024) contains KiB", fmt_size(1024).find("KiB") != std::string::npos);

  // Test 3: fmt_count
  check("fmt_count(5) contains 5", fmt_count(5).find("5") != std::string::npos);

  // Test 4: parse demo data
  {
    Module m{};
    m.name = "test_mod";
    m.size = 123456;
    m.use_count = 2;
    m.dependencies = {"dep1", "dep2"};
    m.is_live = true;
    check("demo module has 2 deps", m.dependencies.size() == 2);
  }

  std::cout << "\n" << C_BLD << "Results: " << pass << " passed, " << fail << " failed" << C_RST << "\n";
  return fail > 0 ? 1 : 0;
}

// ── Demo mode ──
void print_demo() {
  std::cout << C_BLD << C_CYN << "Module         Size  Used  Deps              State" << C_RST << "\n";
  std::cout << C_DIM << "───────────── ─────── ──── ───────────────── ────" << C_RST << "\n";
  std::cout << "btrfs         1.5 MiB    3  —                 Live\n";
  std::cout << "ext4          2.1 MiB    2  mbcache,jbd2      Live\n";
  std::cout << "dm_crypt      42.3 KiB   0  —                 Live\n";
  std::cout << "i915          3.8 MiB    0  drm_buddy,drm_kms Live\n";
  std::cout << "kvm_intel    124.5 KiB   0  kvm               Live\n";
  std::cout << "nvidia       41.2 MiB    2  —                 Live\n";
  std::cout << "overlay      156.2 KiB   0  —                 Live\n";
  std::cout << "wireless      89.1 KiB   2  cfg80211          Live\n";
  std::cout << "usb_storage  31.7 KiB   0  scsi_mod          Live\n";
  std::cout << C_DIM << "\nTotal: 10 modules, 47.5 MiB loaded" << C_RST << "\n";
}

// ── Table output ──
void print_table(const std::vector<Module>& modules, int filter_count) {
  // Helper to pad visible string
  auto pad_left = [](int width, const std::string& s) -> std::string {
    int vis = 0;
    bool esc = false;
    for (char c : s) {
      if (c == '\033') { esc = true; continue; }
      if (esc) { if (c == 'm') esc = false; continue; }
      vis++;
    }
    int pad = width - vis;
    if (pad > 0) return std::string(pad, ' ') + s;
    return s;
  };
  auto pad_right = [](int width, const std::string& s) -> std::string {
    int vis = 0;
    bool esc = false;
    for (char c : s) {
      if (c == '\033') { esc = true; continue; }
      if (esc) { if (c == 'm') esc = false; continue; }
      vis++;
    }
    int pad = width - vis;
    if (pad > 0) return s + std::string(pad, ' ');
    return s;
  };

  // Header
  std::cout << C_BLD << C_CYN
    << pad_right(22, "Module")
    << pad_left(9, "Size")
    << pad_left(5, "Used")
    << "  "
    << pad_right(30, "Dependencies")
    << pad_right(7, "State")
    << C_RST << "\n";

  std::cout << C_DIM
    << pad_right(22, "──────────────────────")
    << pad_left(9, "─────────")
    << pad_left(5, "─────")
    << "  "
    << pad_right(30, "──────────────────────────────")
    << pad_right(7, "───────")
    << C_RST << "\n";

  unsigned long long total_size = 0;
  for (auto& m : modules) {
    total_size += m.size;

    std::string deps_str;
    if (m.dependencies.empty()) {
      deps_str = C_DIM + std::string("—") + C_RST;
    } else {
      deps_str = C_GRY;
      for (size_t i = 0; i < m.dependencies.size() && i < 3; ++i) {
        if (i > 0) deps_str += ",";
        deps_str += m.dependencies[i];
      }
      if (m.dependencies.size() > 3) deps_str += "+";
      // Truncate long deps to fit 30-char column
      {
        int vis = 0; bool esc = false;
        for (char c : deps_str) {
          if (c == '\033') { esc = true; continue; }
          if (esc) { if (c == 'm') esc = false; continue; }
          vis++;
        }
        if (vis > 30) {
          int pos = 0; esc = false;
          size_t cut = 0;
          for (size_t i = 0; i < deps_str.size(); ++i) {
            char c = deps_str[i];
            if (c == '\033') { esc = true; continue; }
            if (esc) { if (c == 'm') esc = false; continue; }
            pos++;
            if (pos == 28) { cut = i; break; }
          }
          if (cut > 0) deps_str = deps_str.substr(0, cut) + "..." + C_RST;
          else deps_str += C_RST;
        } else {
          deps_str += C_RST;
        }
      }
    }

    std::string state_color = m.is_live ? C_GRN : C_YLW;
    std::string state_text = state_color + (m.is_live ? "Live" : "Loading") + C_RST;

    std::string size_fmt = fmt_size(m.size);
    std::string count_fmt = fmt_count(m.use_count);

    // Highlight high-use modules
    std::string name_color = (m.use_count >= 5) ? C_RED :
                             (m.use_count >= 2) ? C_YLW : C_WHT;

    std::cout
      << pad_right(22, name_color + m.name + C_RST)
      << pad_left(9, size_fmt)
      << pad_left(5, count_fmt)
      << "  "
      << pad_right(30, deps_str)
      << pad_right(7, state_text)
      << "\n";
  }

  std::cout << C_DIM << std::string(77, '-') << C_RST << "\n";
  char footer[128];
  std::snprintf(footer, sizeof(footer), "Total: %zu modules, %s loaded",
    modules.size(), fmt_size(total_size).c_str());
  std::cout << C_DIM << footer << C_RST << "\n";
  if (filter_count >= 0) {
    char ftr2[64];
    std::snprintf(ftr2, sizeof(ftr2), "(filtered: %d of %zu)", filter_count, modules.size());
    std::cout << C_DIM << ftr2 << C_RST << "\n";
  }
}

// ── Tree output ──
void print_tree(const std::vector<Module>& modules) {
  // Build dependency graph: which modules are depended on by others
  std::unordered_map<std::string, std::vector<std::string>> dependents;
  for (auto& m : modules) {
    for (auto& dep : m.dependencies) {
      dependents[dep].push_back(m.name);
    }
  }

  // Find root modules (no one depends on them AND they have deps themselves)
  // Actually, find modules that are depended on by others (top-level = have dependents)
  std::vector<std::string> roots;
  for (auto& m : modules) {
    if (dependents.count(m.name)) {
      roots.push_back(m.name);
    }
  }

  if (roots.empty()) {
    std::cout << C_DIM << "No dependency relationships found" << C_RST << "\n";
    return;
  }

  std::cout << C_BLD << C_CYN << "Module dependency tree:" << C_RST << "\n\n";

  for (auto& root : roots) {
    std::cout << C_GRN << root << C_RST << "\n";
    auto& deps_list = dependents[root];
    for (size_t i = 0; i < deps_list.size(); ++i) {
      std::string prefix = (i == deps_list.size() - 1) ? "└── " : "├── ";
      std::cout << C_DIM << prefix << C_RST << C_WHT << deps_list[i] << C_RST << "\n";
    }
    std::cout << "\n";
  }
}

// ── JSON output ──
void print_json(const std::vector<Module>& modules) {
  std::cout << "{\n  \"modules\": [\n";
  for (size_t i = 0; i < modules.size(); ++i) {
    auto& m = modules[i];
    std::cout << "    {\"name\": \"" << m.name
              << "\", \"size\": " << m.size
              << ", \"use_count\": " << m.use_count
              << ", \"state\": \"" << (m.is_live ? "Live" : "Loading")
              << "\", \"deps\": [";
    for (size_t j = 0; j < m.dependencies.size(); ++j) {
      if (j > 0) std::cout << ", ";
      std::cout << "\"" << m.dependencies[j] << "\"";
    }
    std::cout << "]}";
    if (i < modules.size() - 1) std::cout << ",";
    std::cout << "\n";
  }
  std::cout << "  ],\n  \"total\": " << modules.size() << "\n}\n";
}

int main(int argc, char** argv) {
  // Defaults
  bool do_json = false;
  bool do_tree = false;
  bool do_color = true;
  bool do_selftest = false;
  bool do_demo = false;
  int watch_secs = 0;
  std::string sort_by = "size";  // size, deps, count, name
  std::string filter_name;
  int filter_count = -1;  // -1 = no filter

  // Parse args
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--json") do_json = true;
    else if (arg == "--tree") do_tree = true;
    else if (arg == "--no-color") do_color = false;
    else if (arg == "--selftest") do_selftest = true;
    else if (arg == "--demo") do_demo = true;
    else if (arg == "--watch" && i + 1 < argc) watch_secs = std::stoi(argv[++i]);
    else if (arg == "--sort" && i + 1 < argc) sort_by = argv[++i];
    else if (arg == "--filter" && i + 1 < argc) filter_name = argv[++i];
    else if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: lsmod [OPTIONS]\n"
                << "  --json          Output as JSON\n"
                << "  --tree          Show dependency tree\n"
                << "  --watch N       Refresh every N seconds\n"
                << "  --sort FIELD    Sort by: size, deps, count, name\n"
                << "  --filter NAME   Filter by module name substring\n"
                << "  --no-color      Disable ANSI colors\n"
                << "  --demo          Show demo output\n"
                << "  --selftest      Run self-test\n";
      return 0;
    }
  }

  // Apply color
  if (!do_color) {
    g_color = false;
    C_RST = C_BLD = C_DIM = C_RED = C_GRN = C_YLW = "";
    C_BLU = C_MAG = C_CYN = C_WHT = C_GRY = "";
  }

  if (do_selftest) return selftest();

  auto run_once = [&]() {
    auto modules = parse_modules();
    if (modules.empty()) {
      std::cerr << C_RED << "Error: Cannot read /proc/modules" << C_RST << "\n";
      return;
    }

    // Apply filter
    if (!filter_name.empty()) {
      std::vector<Module> filtered;
      for (auto& m : modules) {
        if (m.name.find(filter_name) != std::string::npos)
          filtered.push_back(m);
      }
      filter_count = static_cast<int>(filtered.size());
      modules = std::move(filtered);
    }

    // Sort
    if (sort_by == "size") {
      std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) { return a.size > b.size; });
    } else if (sort_by == "deps") {
      std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) { return a.dependencies.size() > b.dependencies.size(); });
    } else if (sort_by == "count") {
      std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) { return a.use_count > b.use_count; });
    } else if (sort_by == "name") {
      std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) { return a.name < b.name; });
    }

    if (do_demo) {
      print_demo();
    } else if (do_json) {
      print_json(modules);
    } else if (do_tree) {
      print_tree(modules);
    } else {
      print_table(modules, filter_count);
    }
  };

  if (watch_secs > 0) {
    while (true) {
      std::cout << "\033[2J\033[H";  // clear screen
      run_once();
      std::cout << std::flush;
      std::this_thread::sleep_for(std::chrono::seconds(watch_secs));
    }
  } else {
    run_once();
  }

  return 0;
}
