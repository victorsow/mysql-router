/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "router_app.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>

#include <mysql/harness/config_parser.h>
#include <mysql/harness/filesystem.h>
#include <mysql/harness/loader.h>


using std::string;
using mysqlrouter::string_format;
using mysqlrouter::substitute_envvar;
using mysqlrouter::wrap_string;


MySQLRouter::MySQLRouter(const vector<string> arguments) : version_(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH),
                                                           arg_handler_(), loader_(), can_start_(false),
                                                           showing_info_(false) {
  init(arguments);
}

MySQLRouter::MySQLRouter(const int argc, char **argv) : MySQLRouter(vector<string>({argv + 1, argv + argc})) { }

void MySQLRouter::init(vector<string> arguments) {
  set_default_config_files(CONFIG_FILES);
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }

  if (showing_info_) {
    return;
  }

  check_config_files();
  can_start_ = true;
}

void MySQLRouter::start() {
  if (!can_start_) {
    return;
  }
  string err_msg = "Configuration error: %s.";

  std::map<std::string, std::string> params = {
      {"program", "mysqlrouter"}
  };

  try {
    loader_ = std::unique_ptr<Loader>(new Loader("router", params));
    auto config_file_containers = {
        &default_config_files_,
        &config_files_,
        &extra_config_files_
    };
    for (vector<string> *vec: config_file_containers) {
      for (auto config_file: *vec) {
        loader_->read(Path(config_file));
      }
    }
  } catch (const parser_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  }

  if (!startup_messages_.empty()) {
    for (auto msg: startup_messages_) {
      std::cout << string_format(msg.c_str()) << std::endl;
    }
    startup_messages_.clear();
    vector<string>().swap(startup_messages_);
  }

  loader_->start();
}

void MySQLRouter::set_default_config_files(const char *locations) NOEXCEPT {
  std::stringstream ss_line{locations};

  // We remove all previous entries
  default_config_files_.clear();
  std::vector<string>().swap(default_config_files_);

  for (string file; std::getline(ss_line, file, ';');) {
    try {
      substitute_envvar(file);
    } catch (const mysqlrouter::envvar_no_placeholder &err) {
      // No placeholder in file path; this is OK
    } catch (const mysqlrouter::envvar_error &err) {
      // Any other problem with placeholders we ignore and don't use file
      continue;
    }
    default_config_files_.push_back(std::move(file));
  }
}

string MySQLRouter::get_version() NOEXCEPT {
  return string(VERSION);
}

string MySQLRouter::get_version_line() NOEXCEPT {
  std::ostringstream os;
  string edition{VERSION_EDITION};

  os << PACKAGE_NAME << " v" << get_version();

  os << " on " << PACKAGE_PLATFORM << " (" << (PACKAGE_ARCH_64BIT ? "64-bit" : "32-bit") << ")";

  if (!edition.empty()) {
    os << " (" << edition << ")";
  }

  return os.str();
}

vector<string> MySQLRouter::check_config_files() {
  vector<string> result;

  size_t nr_of_none_extra = 0;

  auto config_file_containers = {
      &default_config_files_,
      &config_files_,
      &extra_config_files_
  };

  for (vector<string> *vec: config_file_containers) {
    for (auto &file: *vec) {
      auto pos = std::find(result.begin(), result.end(), file);
      if (pos != result.end()) {
        throw std::runtime_error(string_format("Duplicate configuration file: %s.", file.c_str()));
      }
      if (std::fopen(file.c_str(), "r") != nullptr) {
        result.push_back(file);
        if (vec != &extra_config_files_) {
          nr_of_none_extra++;
        }
      }
    }
  }

  // Can not have extra configuration files when we do not have other configuration files
  if (!extra_config_files_.empty() && nr_of_none_extra == 0) {
    throw std::runtime_error("Extra configuration files only work when other configuration files are available.");
  }

  if (result.empty()) {
    throw std::runtime_error("No valid configuration file available.");
  }

  return result;
}

void MySQLRouter::prepare_command_options() NOEXCEPT {
  arg_handler_.clear_options();
  arg_handler_.add_option(OptionNames({"-v", "--version"}), "Display version information and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
        std::cout << this->get_version_line() << std::endl;
        this->showing_info_ = true;
      });

  arg_handler_.add_option(OptionNames({"-h", "--help"}), "Display this help and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
        this->show_help();
        this->showing_info_ = true;
      });

  arg_handler_.add_option(OptionNames({"-c", "--config"}),
                          "Only read configuration from given file.",
                          CmdOptionValueReq::required, "path", [this](const string &value) throw(std::runtime_error) {
        // When --config is used, no defaults shall be read
        default_config_files_.clear();

        char *abspath = realpath(value.c_str(), nullptr);
        if (abspath != nullptr) {
          config_files_.push_back(string(abspath));
          free(abspath);
        }
      });

  arg_handler_.add_option(OptionNames({"-a", "--extra-config"}),
                          "Read this file after configuration files are read from either "
                              "default locations or from files specified by the --config option.",
                          CmdOptionValueReq::required, "path", [this](const string &value) throw(std::runtime_error) {
        char *abspath = realpath(value.c_str(), nullptr);
        if (abspath != nullptr) {
          extra_config_files_.push_back(string(abspath));
          free(abspath);
        }
      });
}

void MySQLRouter::show_help() NOEXCEPT {
  FILE *fp;
  std::cout << WELCOME << std::endl;

  for (auto line: wrap_string("Configuration read from the following files in the given order"
                                  " (enclosed in parentheses means not available for reading):", kHelpScreenWidth,
                              0)) {
    std::cout << line << std::endl;
  }

  for (auto file : default_config_files_) {

    if ((fp = std::fopen(file.c_str(), "r")) == nullptr) {
      std::cout << "  (" << file << ")" << std::endl;
    } else {
      std::fclose(fp);
      std::cout << "  " << file << std::endl;
    }
  }

  std::cout << "\n";

  show_usage();
}

void MySQLRouter::show_usage(bool include_options) NOEXCEPT {
  for (auto line: arg_handler_.usage_lines("Usage: mysqlrouter", "", kHelpScreenWidth)) {
    std::cout << line << std::endl;
  }

  if (!include_options) {
    return;
  }

  std::cout << "\nOptions:" << std::endl;
  for (auto line: arg_handler_.option_descriptions(kHelpScreenWidth, kHelpScreenIndent)) {
    std::cout << line << std::endl;
  }

  std::cout << "\n";
}

void MySQLRouter::show_usage() NOEXCEPT {
  show_usage(true);
}
