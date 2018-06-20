#include "GameConfig.hpp"

#include <rw/defines.hpp>
#include <rw/filesystem.hpp>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <algorithm>
#include <iostream>
#include <stdexcept>

const std::string kConfigDirectoryName("OpenRW");

static std::string stripConfigString(const std::string &str) {
    auto s = std::string(str, str.find_first_not_of(" \t"), str.find_first_of(";#"));
    return s.erase(s.find_last_not_of(" \n\r\t") + 1);
}

void RWConfig::readConfigFile(const rwfs::path &path) {
    pt::ptree ptree;
    try {
        pt::read_ini(path.string(), ptree);
    } catch (const pt::ini_parser_error &e) {
        throw std::runtime_error(e.what());
    }

    for (auto &field : iterConfigFileFields()) {
        if (field->hasValue()) {
            // If the field is already initialized, don't read config.
            continue;
        }
        auto data = ptree.get_optional<std::string>(field->key);
        if (field->required && !data) {
            throw std::runtime_error(std::string("Required key \"") + field->key + "\" missing.");
        }
        if (data) {
            bool result = field->fromString(stripConfigString(data.get()));
            if (!result) {
                throw std::runtime_error(std::string("Key \"") + field->key + "\" is of invalid type.");
            }
        }
    }

    for (const auto &section : ptree) {
        for (const auto &SectionItem : section.second) {
            std::string key = section.first + "." + SectionItem.first;
            allConfigData[key] = SectionItem.second.data();
        }
    }
}

void RWConfig::writeConfigFile(const rwfs::path &path) {
    pt::ptree ptree;

    for (const auto &configItem : allConfigData) {
        ptree.add(configItem.first, configItem.second);
    }

    for (auto &field : iterConfigFileFields()) {
        if (!field->hasValue()) {
            continue;
        }
        auto data = field->toString(BaseRWConfigField::VALUE);
        ptree.put(field->key, data);
    }
    try {
        pt::write_ini(path.string(), ptree);
    } catch (const pt::ini_parser_error &e) {
        throw std::runtime_error(e.what());
    }
}

std::string RWConfig::getDefaultINIString() const {
    pt::ptree ptree;

    for (auto &field : iterConfigFileFields()) {
        auto data = field->toString(BaseRWConfigField::DEFAULT);
        ptree.put(field->key, data);
    }
    std::ostringstream oss;
    pt::write_ini(oss, ptree);
    return oss.str();
}

static const auto DEFAULT_CONFIG_FILE_NAME = "openrw.ini";

rwfs::path RWConfig::defaultConfigPath() {
#if defined(RW_LINUX) || defined(RW_FREEBSD) || defined(RW_NETBSD) || \
    defined(RW_OPENBSD)
    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home != nullptr) {
        return rwfs::path(config_home) / kConfigDirectoryName / DEFAULT_CONFIG_FILE_NAME;
    }
    char *home = getenv("HOME");
    if (home != nullptr) {
        return rwfs::path(home) / ".config/" / kConfigDirectoryName / DEFAULT_CONFIG_FILE_NAME;
    }

#elif defined(RW_OSX)
    char *home = getenv("HOME");
    if (home)
        return rwfs::path(home) / "Library/Preferences/" / kConfigDirectoryName / DEFAULT_CONFIG_FILE_NAME;

#else
    return rwfs::path();
#endif

    // Well now we're stuck.
    RW_ERROR("No default config path found.");
    return rwfs::path() / DEFAULT_CONFIG_FILE_NAME;
}

static po::options_description getOptionDescription() {
    po::options_description desc_window("Window options");
    desc_window.add_options()(
        "width,w", po::value<size_t>()->value_name("WIDTH"), "Game resolution width in pixel")(
        "height,h", po::value<size_t>()->value_name("HEIGHT"), "Game resolution height in pixel")(
        "fullscreen,f", "Enable fullscreen mode");
    po::options_description desc_game("Game options");
    desc_game.add_options()(
        "newgame,n", "Start a new game")(
        "load,l", po::value<std::string>()->value_name("PATH"), "Load save file");
    po::options_description desc_devel("Developer options");
    desc_devel.add_options()(
        "test,t", "Starts a new game in a test location")(
        "benchmark,b", po::value<std::string>()->value_name("PATH"), "Run benchmark from file");
    po::options_description desc("Generic options");
    desc.add_options()(
        "config,c", po::value<rwfs::path>()->value_name("PATH"), "Path of configuration file")(
        "gamedata,g", po::value<rwfs::path>()->value_name("PATH"), "Path of game data")(
        "language", po::value<std::string>()->value_name("LANG"), "Language")(
        "help", "Show this help message");
    desc.add(desc_window).add(desc_game).add(desc_devel);
    return desc;
}

void RWConfig::parseArguments(int argc, const char *argv[]) {
    auto desc = getOptionDescription();
    po::variables_map vm;

    try {
        auto parsedOptions = po::parse_command_line(argc, argv, desc);
        po::store(parsedOptions, vm);
        auto additional = po::collect_unrecognized(parsedOptions.options, po::include_positional);
        if (additional.size()) {
            throw std::runtime_error(std::string("Unrecognized option: \"") + additional[0] + "\"");
        }
        po::notify(vm);
    } catch (po::error &e) {
       throw std::runtime_error(e.what());
    }

    displayHelp = vm.count("help");

    if (vm.count("width")) {
       windowWidth.value = vm["width"].as<size_t>();
    }
    if (vm.count("height")) {
       windowHeight.value = vm["height"].as<size_t>();
    }
    if (vm.count("fullscreen")) {
       windowFullscreen.value = true;
    }

    startNewGame = vm.count("newgame");
    if (vm.count("load")) {
        startSaveGame.value = vm["load"].as<rwfs::path>();
    }
    startTestGame = vm.count("test");
    if (vm.count("benchmark")) {
        startBenchmark.value = vm["benchmark"].as<rwfs::path>();
    }

    if (vm.count("config")) {
       configPath.value = vm["config"].as<rwfs::path>();
    }
    if (vm.count("gamedata")) {
       gameDataPath.value = vm["gamedata"].as<rwfs::path>();
    }
    if (vm.count("language")) {
       gameLanguage.value = vm["language"].as<std::string>();
    }
}

std::ostream &RWConfig::printArgumentsHelpText(std::ostream &os) {
    return os << getOptionDescription();
}
