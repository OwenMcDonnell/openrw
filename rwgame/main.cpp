#define SDL_MAIN_HANDLED
#include "RWArguments.hpp"
#include "RWGame.hpp"
#include "SDL.h"

#include <rw/optional.hpp>
#include <core/Logger.hpp>

#include <iostream>

static rwopt::optional<RWConfig> createConfig(int argc, const char *argv[]) {
    RWConfig conf;
    try {
        conf.parseArguments(argc, argv);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        conf.printArgumentsHelpText(std::cerr);
        throw std::invalid_argument(e.what());
    }
    if (conf.displayHelp) {
        conf.printArgumentsHelpText(std::cout);
        return {};
    }
    const auto configPath = conf.configPath.getValueOrDefault();
    try {
        conf.readConfigFile(configPath);
    } catch (const std::runtime_error &e) {
        std::string msg = std::string(e.what()) + "\nInvalid INI file at \""  + configPath.string() + "\"."
                + "\nAdapt the following default INI to your configuration.\n"
                + conf.getDefaultINIString();
        throw std::runtime_error(msg);
    }
    return conf;
}

int main(int argc, const char* argv[]) {
    SDL_SetMainReady();
    // Initialise Logging before anything else happens
    StdOutReceiver logstdout;
    Logger logger({ &logstdout });
    try {
        auto config = createConfig(argc, argv);
        if (!config) {
            return 0;
        }
        RWGame game(logger, config.value());

        return game.run();
    } catch (std::invalid_argument& ex) {
        // This exception is thrown when either an invalid command line option
        // is found. The RWGame constructor prints a usage message in this case
        // and then throws this exception.
        return -2;
    } catch (std::runtime_error& ex) {
        // Catch runtime_error as these are fatal issues the user may want to
        // know about like corrupted files or GL initialisation failure.
        // Catching other types (out_of_range, bad_alloc) would just make
        // debugging them more difficult.

        logger.error("exception", ex.what());
        const char* kErrorTitle = "Fatal Error";
        if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kErrorTitle,
                                     ex.what(), nullptr) < 0) {
            SDL_Log("Failed to show message box\n");
        }

        SDL_Quit();

        return -1;
    }
}
