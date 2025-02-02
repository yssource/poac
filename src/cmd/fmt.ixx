module;

// external
#include <glob/glob.h> // NOLINT(build/include_order)
#include <spdlog/spdlog.h> // NOLINT(build/include_order)
#include <structopt/app.hpp>
#include <toml.hpp>

// internal
#include "../util/result-macros.hpp"

export module poac.cmd.fmt;

import poac.config;
import poac.util.format;
import poac.util.log;
import poac.util.result;
import poac.util.rustify;
import poac.util.verbosity;
import poac.util.shell;
import poac.util.validator;

namespace poac::cmd::fmt {

export struct Options : structopt::sub_command {
  /// Perform only checks
  Option<bool> check = false;
};

using ClangFormatNotFound = Error<
    "`fmt` command requires `clang-format`; try installing it by:\n"
    "  apt/brew install clang-format">;

inline constexpr Arr<StringRef, 14> EXTENSIONS{"c",   "c++", "cc",  "cpp", "cu",
                                               "cuh", "cxx", "h",   "h++", "hh",
                                               "hpp", "hxx", "ixx", "cppm"};
inline constexpr Arr<StringRef, 2> PATTERNS{"{}/*.{}", "{}/**/*.{}"};

export inline constexpr Arr<StringRef, 3> EXCLUDES{
    config::POAC_OUT, "build", "cmake-build-debug" // CLion
};

void fmt_impl(const Path& base_dir, Vec<Path>& targets) {
  for (const auto& entry : fs::directory_iterator(base_dir)) {
    if (entry.is_directory()) {
      const String d = entry.path().filename().string();
      if (!d.starts_with('.') && !contains(EXCLUDES, d)) {
        for (const StringRef e : EXTENSIONS) {
          for (const StringRef p : PATTERNS) {
            const String search =
                format(::fmt::runtime((base_dir / p).string()), d, e);
            const Vec<Path> search_glob = glob::rglob(search);
            if (search_glob.empty()) {
              spdlog::trace("Glob `{}` not found; skipping ...", search);
              continue;
            }
            spdlog::trace("Glob `{}` found!", search);
            append(targets, search_glob);
          }
        }
      }
    }
  }
}

[[nodiscard]] auto fmt(const Path& base_dir, StringRef args) -> Result<void> {
  Vec<Path> targets;
  fmt_impl(base_dir, targets);
  if (targets.empty()) {
    spdlog::info("no targets found.");
    return Ok();
  }

  const String clang_format = format(
      "cd {} && clang-format {} {}", base_dir.string(), args,
      ::fmt::join(targets, " ")
  );
  spdlog::trace("Executing `{}`", clang_format);
  if (const i32 code = util::shell::Cmd(clang_format).exec_no_capture();
      code != 0) {
    spdlog::info("");
    return Err<SubprocessFailed>("clang-format", code);
  }
  return Ok();
}

export [[nodiscard]] auto exec(const Options& opts) -> Result<void> {
  spdlog::trace("Checking if `clang-format` command exists ...");
  if (!util::shell::has_command("clang-format")) {
    return Err<ClangFormatNotFound>();
  }

  spdlog::trace("Checking if required config exists ...");
  const Path manifest_path =
      Try(util::validator::required_config_exists().map_err(to_anyhow));

  spdlog::trace("Parsing the manifest file: {} ...", manifest_path);
  // TODO(ken-matsui): parse as a static type rather than toml::value
  const toml::value manifest =
      toml::parse(relative(manifest_path, config::cwd));
  String name = toml::find<String>(manifest, "package", "name");
  if (name.empty()) {
    log::warn("project name is empty; try setting anything you want.");
    name = "<empty>";
  }

  String args = "--style=file --fallback-style=LLVM -Werror ";
  if (util::verbosity::is_verbose()) {
    args += "--verbose ";
  }
  if (opts.check.value()) {
    args += "--dry-run";
  } else {
    args += "-i";
    log::status("Formatting", name);
  }
  return fmt(manifest_path.parent_path(), args);
}

} // namespace poac::cmd::fmt

STRUCTOPT(poac::cmd::fmt::Options, check);
