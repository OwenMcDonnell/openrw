#include "GameBase.hpp"

//#include <rw/filesystem.hpp>

#include <iostream>

#include <SDL.h>

#include <rw/defines.hpp>

#include "GitSHA1.h"

#include <iostream>

// Use first 8 chars of git hash as the build string
const std::string kBuildStr(kGitSHA1Hash, 8);
const std::string kWindowTitle = "RWGame";

GameBase::GameBase(Logger &inlog, const RWConfig &config) :
        log(inlog),
        window(),
        config(config) {
    log.info("Game", "Build: " + kBuildStr);

    std::size_t w = config.windowWidth.getValueOrDefault();
    std::size_t h = config.windowHeight.getValueOrDefault();
    bool fullscreen = config.windowFullscreen.getValueOrDefault();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        throw std::runtime_error("Failed to initialize SDL2!");

    window.create(kWindowTitle + " [" + kBuildStr + "]", w, h, fullscreen);

    SET_RW_ABORT_CB([this]() {window.showCursor();},
            [this]() {window.hideCursor();});
}

GameBase::~GameBase() {
    SDL_Quit();

    log.info("Game", "Done cleaning up");
}
