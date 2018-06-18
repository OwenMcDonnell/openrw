#include "test_Globals.hpp"

#include <GameConfig.hpp>

#if RW_TEST_WITH_DATA
std::string Global::getGamePath() {
    RWConfig config;
    config.readConfigFile(RWConfig::defaultConfigPath());
    return config.gameDataPath.getValueOrDefault().string(); //FIXME: use path
}
#endif
