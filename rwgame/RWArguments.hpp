#ifndef _RWGAME_RWARGUMENTS_HPP
#define _RWGAME_RWARGUMENTS_HPP

#include <rw/filesystem.hpp>
#include <rw/optional.hpp>

#include <iosfwd>

class RWArguments {
public:
    bool initialized = false;
    bool help = false;

    rwopt::optional<std::size_t> width;
    rwopt::optional<std::size_t> height;
    rwopt::optional<std::size_t> fullscreen;

    bool newGame = false;
    rwopt::optional<rwfs::path> savegame;

    bool test = false;
    rwopt::optional<rwfs::path> benchmark;

    rwopt::optional<rwfs::path> configPath;

    RWArguments() =default;
    void parse(int argc, char *argv[]);

    std::ostream &outputHelpText(std::ostream &os);
};

std::ostream &operator<<(std::ostream &, const RWArguments &);

#endif
