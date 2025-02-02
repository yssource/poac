module;

// std
#include <algorithm> // std::remove

// external
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <spdlog/spdlog.h> // NOLINT(build/include_order)
#include <structopt/app.hpp>

// internal
#include "../util/result-macros.hpp"

export module poac.cmd.search;

import poac.util.format;
import poac.util.log;
import poac.util.net;
import poac.util.pretty;
import poac.util.result;
import poac.util.rustify;
import poac.util.verbosity;

namespace poac::cmd::search {

export struct Options : structopt::sub_command {
  /// Package name to search
  String package_name;
};

using PackageNotFound = Error<"No packages found for `{}`", String>;

[[nodiscard]] auto search(const Options& opts) -> Result<void> {
  const boost::property_tree::ptree pt =
      Try(util::net::api::search(opts.package_name, 20).map_err(to_anyhow));
  if (util::verbosity::is_verbose()) {
    boost::property_tree::write_json(std::cout, pt);
  }

  const auto children = pt.get_child("data.results");
  if (children.empty()) {
    return Err<PackageNotFound>(opts.package_name);
  }

  constexpr i32 MAX_COLUMNS = 10; // limit the number of columns
  i32 i = 1;
  for (const boost::property_tree::ptree::value_type& child : children) {
    if (i > MAX_COLUMNS) {
      break;
    }

    const boost::property_tree::ptree& hits = child.second;
    const String package = format(
        "{} = \"{}\"", hits.get<String>("name"), hits.get<String>("version")
    );

    auto description = hits.get<String>("description");
    description = util::pretty::clip_string(description, 100);
    // If util::pretty::clip_string clips last \n, \n should be removed
    description.erase(
        std::remove(description.begin(), description.end(), '\n'),
        description.end()
    );

    spdlog::info("{:<40}# {}", package, description);
    ++i;
  }
  return Ok();
}

export [[nodiscard]] inline auto exec(const Options& opts) -> Result<void> {
  return search(opts);
}

} // namespace poac::cmd::search

STRUCTOPT(poac::cmd::search::Options, package_name);
