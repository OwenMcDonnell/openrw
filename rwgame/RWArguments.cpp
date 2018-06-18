#include "RWArguments.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>

namespace po = boost::program_options;

static po::options_description getOptionDescription() {
    namespace po = boost::program_options;
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
       "help", "Show this help message");
    desc.add(desc_window).add(desc_game).add(desc_devel);
    return desc;
}

void RWArguments::parse(int argc, char *argv[]) {
    auto desc = getOptionDescription();
    po::variables_map vm;
    try {
       po::store(po::parse_command_line(argc, argv, desc), vm);
       po::notify(vm);
    } catch (po::error &ex) {
       std::cerr << "Error parsing arguments: " << ex.what() << std::endl;
       std::cerr << desc;
       return;
    }

    if (vm.count("help")) {
       help = true;
    }

    if (vm.count("width")) {
       width = vm["width"].as<size_t>();
    }
    if (vm.count("height")) {
       height = vm["height"].as<size_t>();
    }
    if (vm.count("fullscreen")) {
       fullscreen = true;
    }

    newGame = vm.count("newgame");
    if (vm.count("load")) {
        savegame = vm["load"].as<rwfs::path>();
    }

    test = vm.count("test");
    if (vm.count("benchmark")) {
        benchmark = vm["benchmark"].as<rwfs::path>();
    }

    if (vm.count("config")) {
       configPath = vm["config"].as<rwfs::path>();
    }

    initialized = true;
}

std::ostream &operator<<(std::ostream &os, const RWArguments &) {
    return os << getOptionDescription();
}
