// C++ class to generate .ninja files.
// This file is based on ninja_syntax.py from:
// https://github.com/ninja-build/ninja/blob/master/misc/ninja_syntax.py

module;

// std
#include <cassert>
#include <sstream>
#include <string>
#include <utility> // std::move

// external
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/regex.hpp>

export module poac.core.builder.syntax;

import poac.util.format;
import poac.util.log;
import poac.util.result;
import poac.util.rustify;
import poac.util.pretty;

namespace poac::core::builder::syntax {

export struct RuleSet {
  Option<String> description = None;
  Option<String> depfile = None;
  bool generator = false;
  Option<String> pool = None;
  bool restat = false;
  Option<String> rspfile = None;
  Option<String> rspfile_content = None;
  Option<String> deps = None;
};

export using Variables = HashMap<String, String>;
export struct BuildSet {
  Option<Vec<String>> inputs = None;
  Option<Vec<Path>> implicit = None;
  Option<Path> order_only = None;
  Option<Variables> variables = None;
  Option<Path> implicit_outputs = None;
  Option<String> pool = None;
  Option<String> dyndep = None;
};

inline auto escape_path(const Path& p) -> Path {
  String s = p.string();
  boost::replace_all(s, "$ ", "$$ ");
  boost::replace_all(s, " ", "$ ");
  boost::replace_all(s, ":", "$:");
  return s;
}

/// Escape a string such that it can be embedded into a Ninja file without
/// further interpretation.
inline void escape(String& s) {
  assert(s.find('\n') == None); // Ninja syntax does not allow newlines
  // We only have one special metacharacter: '$'.
  boost::replace_all(s, "$", "$$");
}

/// Expand a string containing $vars as Ninja would.
///
/// Note: doesn't handle the full Ninja variable syntax, but it's enough
/// to make configure.py's use of it work.
auto expand(
    const String& text, const Variables& vars, const Variables& local_vars = {}
) -> String {
  using namespace std::literals::string_literals;

  const auto exp = [&](const boost::smatch& m) {
    const String var = m[1].str();
    if (var == "$") {
      return "$"s;
    }
    return local_vars.contains(var) ? local_vars.at(var)
           : vars.contains(var)     ? vars.at(var)
                                    : ""s;
  };
  return boost::regex_replace(text, boost::regex(R"(\$(\$|\w*))"), exp);
}

/// ref: https://stackoverflow.com/a/46379136
auto operator*(const String& s, usize n) -> String {
  String result;
  result.reserve(s.size() * n);
  for (usize i = 0; i < n; ++i) {
    result += s;
  }
  return result;
}

export class Writer {
  std::ostringstream output;
  usize width;

  /// Returns the number of '$' characters right in front of s[i].
  static auto count_dollars_before_index(StringRef s, usize i) -> usize {
    usize dollar_count = 0;
    usize dollar_index = i - 1;
    while (dollar_index > 0 && s[dollar_index] == '$') {
      dollar_count += 1;
      dollar_index -= 1;
    }
    return dollar_count;
  }

  // Export this function for testing
#if __has_include(<boost/ut.hpp>)
public:
#endif
  /// Write 'text' word-wrapped at self.width characters.
  void line(String text, usize indent = 0) {
    String leading_space = String("  ") * indent;

    while (leading_space.length() + text.length() > width) {
      // The text is too wide; wrap if possible.

      // Find the rightmost space that would obey our width constraint and
      // that's not an escaped space.
      const std::int32_t available_space =
          width - leading_space.length() - 2; // " $".length() == 2
      std::int32_t space = available_space;
      do {
        space = text.rfind(' ', space);
        // NOLINTNEXTLINE(readability-simplify-boolean-expr)
      } while (!(space < 0 || count_dollars_before_index(text, space) % 2 == 0)
      );

      if (space < 0) {
        // No such space; just use the first unescaped space we can find.
        space = available_space - 1;
        do {
          space = text.find(' ', space + 1);
          // NOLINTNEXTLINE(readability-simplify-boolean-expr)
        } while (
            !(space < 0 || count_dollars_before_index(text, space) % 2 == 0)
        );
      }
      if (space < 0) {
        // Give up on breaking.
        break;
      }

      output << leading_space + text.substr(0, space) + " $\n";
      text = text.substr(space + 1);

      // Subsequent lines are continuations, so indent them.
      leading_space = String("  ") * (indent + 2);
    }
    output << leading_space + text + '\n';
  }

#if !__has_include(<boost/ut.hpp>)
public:
#endif
  explicit Writer(std::ostringstream&& o, usize w = 78)
      : output(std::move(o)), width(w) {}

  inline auto get_value() const -> String { return output.str(); }

  inline void newline() { output << '\n'; }

  void comment(const String& text) {
    for (const String& line : util::pretty::textwrap(text, width - 2)) {
      output << "# " + line + '\n';
    }
  }

  void variable(StringRef key, StringRef value, usize indent = 0) {
    if (value.empty()) {
      return;
    }
    line(format("{} = {}", key, value), indent);
  }

  inline void
  variable(StringRef key, const Vec<String>& values, usize indent = 0) {
    const String value =
        boost::algorithm::join_if(values, " ", [](const auto& s) {
          return !s.empty();
        });
    line(format("{} = {}", key, value), indent);
  }

  inline void pool(StringRef name, StringRef depth) {
    line(format("pool {}", name));
    variable("depth", depth, 1);
  }

  void rule(StringRef name, StringRef command, const RuleSet& rule_set = {}) {
    line(format("rule {}", name));
    variable("command", command, 1);
    if (rule_set.description.has_value()) {
      variable("description", rule_set.description.value(), 1);
    }
    if (rule_set.depfile.has_value()) {
      variable("depfile", rule_set.depfile.value(), 1);
    }
    if (rule_set.generator) {
      variable("generator", "1", 1);
    }
    if (rule_set.pool.has_value()) {
      variable("pool", rule_set.pool.value(), 1);
    }
    if (rule_set.restat) {
      variable("restat", "1", 1);
    }
    if (rule_set.rspfile.has_value()) {
      variable("rspfile", rule_set.rspfile.value(), 1);
    }
    if (rule_set.rspfile_content.has_value()) {
      variable("rspfile_content", rule_set.rspfile_content.value(), 1);
    }
    if (rule_set.deps.has_value()) {
      variable("deps", rule_set.deps.value(), 1);
    }
  }

  auto build(
      const Vec<String>& outputs, StringRef rule, const BuildSet& build_set = {}
  ) -> Vec<String> {
    Vec<String> out_outputs;
    for (const String& o : outputs) {
      out_outputs.emplace_back(escape_path(o).string());
    }

    Vec<String> all_inputs;
    if (build_set.inputs.has_value()) {
      for (const String& i : build_set.inputs.value()) {
        all_inputs.emplace_back(escape_path(i).string());
      }
    }

    if (build_set.implicit.has_value()) {
      Vec<String> implicit;
      for (const Path& i : build_set.implicit.value()) {
        implicit.emplace_back(escape_path(i).string());
      }
      all_inputs.emplace_back("|");
      boost::push_back(all_inputs, implicit);
    }
    if (build_set.order_only.has_value()) {
      Vec<String> order_only;
      for (const Path& o : build_set.order_only.value()) {
        order_only.emplace_back(escape_path(o).string());
      }
      all_inputs.emplace_back("||");
      boost::push_back(all_inputs, order_only);
    }
    if (build_set.implicit_outputs.has_value()) {
      Vec<String> implicit_outputs;
      for (const Path& i : build_set.implicit_outputs.value()) {
        implicit_outputs.emplace_back(escape_path(i).string());
      }
      out_outputs.emplace_back("|");
      boost::push_back(out_outputs, implicit_outputs);
    }

    line(format(
        "build {}: {} {}", boost::algorithm::join(out_outputs, " "), rule,
        boost::algorithm::join(all_inputs, " ")
    ));

    if (build_set.pool.has_value()) {
      line(format("  pool = {}", build_set.pool.value()));
    }
    if (build_set.dyndep.has_value()) {
      line(format("  dyndep = {}", build_set.dyndep.value()));
    }

    if (build_set.variables.has_value()) {
      for (const auto& [key, val] : build_set.variables.value()) {
        variable(key, val, 1);
      }
    }

    return outputs;
  }

  inline void include(const Path& path) { line(format("include {}", path)); }

  inline void subninja(const Path& path) { line(format("subninja {}", path)); }

  inline void defalt(const Vec<String>& paths) {
    line(format("default {}", boost::algorithm::join(paths, " ")));
  }

  inline friend auto operator<<(std::ostream& os, const Writer& w)
      -> std::ostream& {
    return os << w.get_value();
  }
};

} // namespace poac::core::builder::syntax
