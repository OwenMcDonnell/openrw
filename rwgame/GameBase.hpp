#ifndef RWGAME_GAMEBASE_HPP
#define RWGAME_GAMEBASE_HPP
#include "RWArguments.hpp"
#include "GameConfig.hpp"
#include "GameWindow.hpp"

#include <core/Logger.hpp>

#include <boost/program_options.hpp>

/**
 * @brief Handles basic window and setup
 */
class GameBase {
public:
    GameBase(Logger& inlog, const RWConfig &conf);

    virtual ~GameBase() = 0;

    GameWindow& getWindow() {
        return window;
    }

    const RWConfig& getConfig() const {
        return config;
    }

protected:
    Logger& log;
    GameWindow window;
    RWConfig config;
};

#endif
