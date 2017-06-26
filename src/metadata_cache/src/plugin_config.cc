/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/metadata_cache.h"
#include "plugin_config.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <cerrno>
#include <exception>
#include <limits.h>
#include <map>
#include <vector>

#include "mysql/harness/logging/logging.h"

using mysqlrouter::string_format;
using mysqlrouter::to_string;
using std::invalid_argument;

std::string MetadataCachePluginConfig::get_default(const std::string &option) {

  static const std::map<std::string, std::string> defaults{
      {"address",  metadata_cache::kDefaultMetadataAddress},
      {"ttl", to_string(metadata_cache::kDefaultMetadataTTL)},
  };
  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return std::string();
  }
  return it->second;
}

bool MetadataCachePluginConfig::is_required(const std::string &option) {
  const std::vector<std::string> required{
      "user",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

std::vector<mysqlrouter::TCPAddress>
MetadataCachePluginConfig::get_bootstrap_servers(
  const mysql_harness::ConfigSection *section, const std::string &option,
  uint16_t default_port) {

  std::string value = get_option_string(section, option);
  std::stringstream ss(value);

  std::pair<std::string, uint16_t> bind_info;

  std::string address;
  std::vector<mysqlrouter::TCPAddress> address_vector;

  // Fetch the string that contains the list of bootstrap servers separated
  // by a delimiter (,).
  while(getline(ss, address, ','))
  {
    try {
      mysqlrouter::URI u(address);
      bind_info.first = u.host;
      bind_info.second = u.port;
      if (bind_info.second == 0) {
        bind_info.second = default_port;
      }
      address_vector.push_back(mysqlrouter::TCPAddress(bind_info.first,
                                                     bind_info.second));
    } catch (const std::runtime_error &exc) {
      throw invalid_argument(get_log_prefix(option) + " is incorrect (" +
                             exc.what() + ")");
    }
  }
  return address_vector;
}
