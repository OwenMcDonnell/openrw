#include <GameConfig.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fstream>

#include <iostream>
#include <map>

#include <rw/defines.hpp>
#include <rw/filesystem.hpp>

template<class T, size_t N>
constexpr size_t array_size(T(&)[N]) {
    return N;
}

namespace pt = boost::property_tree;

typedef std::map<std::string, std::map<std::string, std::string>>
    simpleConfig_t;

simpleConfig_t readConfig(const rwfs::path &path) {
    simpleConfig_t cfg;
    pt::ptree tree;
    pt::read_ini(path.string(), tree);

    for (const auto &section : tree) {
        for (const auto &subKey : section.second) {
            cfg[section.first][subKey.first] = subKey.second.data();
        }
    }
    return cfg;
}

std::string stripWhitespace(const std::string &in) {
    static const std::string whitespace = " \n\r\t";
    auto start = in.find_first_not_of(whitespace);
    auto end = in.find_last_not_of(whitespace) + 1;
    return std::string(in, start, end - start);
}

simpleConfig_t getValidConfig() {
    simpleConfig_t result;
    // Some values and subkeys are surrounded by whitespace
    // to test the robustness of the INI parser.
    // Don't change game.path and input.invert_y keys. Tests depend on them.
    result["game"]["path"] = "\t/dev/test  \t \r\n";
    result["game"]["\tlanguage\t "] =
        "      american ;american english french german italian spanish.";
    result["input"]["invert_y"] =
        "1 #values != 0 enable input inversion. Optional.";
    return result;
}

std::ostream &operator<<(std::ostream &os, const simpleConfig_t &config) {
    for (auto &section : config) {
        os << "[" << section.first << "]"
           << "\n";
        for (auto &keyValue : section.second) {
            os << keyValue.first << "=" << keyValue.second << "\n";
        }
    }
    return os;
}

class Temp {
    // An object of type Temp file will be removed on destruction
public:
    virtual ~Temp() {
    }
    bool exists() const {
        return rwfs::exists(this->m_path);
    }
    const rwfs::path &path() const {
        return this->m_path;
    }
    std::string filename() const {
        return this->m_path.filename().string();
    }
    std::string dirname() const {
        return this->m_path.parent_path().string();
    }
    virtual void change_perms_normal() const = 0;
    virtual void change_perms_readonly() const = 0;
    virtual void remove() const = 0;
    virtual void touch() const = 0;

protected:
    Temp(const Temp &) = delete;
    Temp() : m_path(getRandomFilePath()) {
    }
    Temp(const rwfs::path &dirname) : m_path(getRandomFilePath(dirname)) {
    }

private:
    static rwfs::path getRandomFilePath(const rwfs::path &dirname) {
        return rwfs::unique_path(dirname / "openrw_test_%%%%%%%%%%%%%%%%");
    }
    static rwfs::path getRandomFilePath() {
        return getRandomFilePath(rwfs::temp_directory_path());
    }
    rwfs::path m_path;
};

class TempFile;

class TempDir : public Temp {
public:
    TempDir() : Temp() {
    }
    TempDir(const TempDir &dirname) : Temp(dirname.path()) {
    }
    virtual ~TempDir() {
        this->remove();
    }
    virtual void change_perms_normal() const override {
        rwfs::permissions(this->path(),
            rwfs::perms::owner_read | rwfs::perms::owner_write | rwfs::perms::owner_exe |
            rwfs::perms::group_read | rwfs::perms::group_exe |
            rwfs::perms::others_read | rwfs::perms::others_exe);
    }
    virtual void change_perms_readonly() const override {
        rwfs::permissions(this->path(),
                        rwfs::perms::owner_read | rwfs::perms::owner_exe |
                        rwfs::perms::group_read | rwfs::perms::group_exe |
                        rwfs::perms::others_read | rwfs::perms::others_exe);
    }
    virtual void remove() const override {
        // Remove may fail if this directory contains a read-only entry. Ignore.
        rwfs::error_code ec;
        rwfs::remove_all(this->path(), ec);
    }
    void touch() const override {
        rwfs::create_directories(this->path());
    }
    friend class TempFile;
};

class TempFile : public Temp {
public:
    TempFile() : Temp() {
    }
    TempFile(const TempDir &dirname) : Temp(dirname.path()) {
    }
    virtual ~TempFile() {
        this->remove();
    }
    virtual void change_perms_normal() const override {
        rwfs::permissions(this->path(),
            rwfs::perms::owner_read | rwfs::perms::owner_write |
            rwfs::perms::group_read |
            rwfs::perms::others_read);
    }
    virtual void change_perms_readonly() const override {
        rwfs::permissions(this->path(), rwfs::perms::owner_read |
                                                        rwfs::perms::group_read |
                                                        rwfs::perms::others_read);
    }
    virtual void remove() const override {
        rwfs::error_code ec;
        rwfs::remove_all(this->path(), ec);
    }
    virtual void touch() const override {
        std::ofstream ofs(this->path().string(), std::ios::out | std::ios::app);
        ofs.close();
    }
    template <typename T>
    bool append(T t) const {
        // Append argument at the end of the file.
        // File is open/closes repeatedly. Not optimal.
        std::ofstream ofs(this->path().string(), std::ios::out | std::ios::app);
        ofs << t;
        ofs.close();
        return ofs.good();
    }
    template <typename T>
    bool write(T t) const {
        // Write the argument to the file, discarding all contents.
        // File is open/closes repeatedly. Not optimal.
        std::ofstream ofs(this->path().string(), std::ios::out | std::ios::trunc);
        ofs << t;
        ofs.close();
        return ofs.good();
    }
};

BOOST_AUTO_TEST_SUITE(ConfigTests)

BOOST_AUTO_TEST_CASE(test_stripWhitespace) {
    std::map<std::string, std::string> map;
    map["abc"] = "abc";
    map["\tabc"] = "abc";
    map["abc\t"] = "abc";
    map[" abc"] = "abc";
    map["abc "] = "abc";
    map[" abc "] = "abc";
    map["  abc  "] = "abc";
    for (const auto &keyValue : map) {
        BOOST_CHECK_EQUAL(keyValue.second, stripWhitespace(keyValue.first));
    }
}

BOOST_AUTO_TEST_CASE(test_TempDir) {
    // Check the behavior of TempFile
    TempDir tempDir;
    BOOST_CHECK(!tempDir.exists());
    tempDir.touch();
    BOOST_CHECK(tempDir.exists());
    tempDir.remove();
    BOOST_CHECK(!tempDir.exists());

    tempDir.touch();
    BOOST_CHECK(tempDir.exists());

    TempDir tempChildDir(tempDir);
    BOOST_CHECK(!tempChildDir.exists());

    tempChildDir.touch();
    BOOST_CHECK(tempChildDir.exists());

    tempDir.remove();
    BOOST_CHECK(!tempChildDir.exists());
    BOOST_CHECK(!tempDir.exists());

    tempChildDir.touch();
    BOOST_CHECK(tempChildDir.exists());

    rwfs::path path;
    {
        TempDir tempLocal;
        tempLocal.touch();
        BOOST_CHECK(tempLocal.exists());
        path = tempLocal.path();
    }
    BOOST_CHECK(!rwfs::exists(path));
}

BOOST_AUTO_TEST_CASE(test_TempFile) {
    // Check the behavior of TempFile
    TempFile tempFile;
    BOOST_CHECK(!tempFile.exists());
    tempFile.touch();
    BOOST_CHECK(tempFile.exists());
    tempFile.remove();
    BOOST_CHECK(!tempFile.exists());

    tempFile.touch();
    BOOST_CHECK(tempFile.exists());
    tempFile.remove();

    BOOST_CHECK(tempFile.append("abc"));
    BOOST_CHECK(tempFile.append("def"));
    BOOST_CHECK(tempFile.exists());
    tempFile.touch();
    std::ifstream ifs(tempFile.path().string());
    std::string line;
    std::getline(ifs, line);
    BOOST_CHECK_EQUAL(line, "abcdef");

    tempFile.change_perms_readonly();
    BOOST_CHECK(!tempFile.write("abc"));
    BOOST_CHECK(!tempFile.append("def"));

    rwfs::path path;
    {
        TempFile tempLocal;
        tempLocal.touch();
        BOOST_CHECK(tempLocal.exists());
        path = tempLocal.path();
    }
    BOOST_CHECK(!rwfs::exists(path));
}

BOOST_AUTO_TEST_CASE(test_config_initial) {
    // Test an initial config
    RWConfig cfg;
}

BOOST_AUTO_TEST_CASE(test_config_valid) {
    // Test reading a valid configuration file
    TempFile tempFile;
    {
        auto cfg = getValidConfig();
        tempFile.append(cfg);
    }

    RWConfig config;
    config.readConfigFile(tempFile.path());

    BOOST_CHECK_EQUAL(config.gameDataPath.value.value().string(), "/dev/test");
    BOOST_CHECK_EQUAL(config.gameLanguage.value.value(), "american");
    BOOST_CHECK(config.inputInvertY.value.value());
}

BOOST_AUTO_TEST_CASE(test_config_valid_modified) {
    // Test reading a valid modified configuration file
    TempFile tempFile;
    {
        auto cfg = getValidConfig();
        cfg["game"]["path"] = "Liberty City";
        cfg["input"]["invert_y"] = "0";

        tempFile.append(cfg);
    }

    RWConfig config;
    config.readConfigFile(tempFile.path());

    BOOST_CHECK(!config.inputInvertY.value.value());
    BOOST_CHECK_EQUAL(config.gameDataPath.value.value().string(), "Liberty City");
}

BOOST_AUTO_TEST_CASE(test_config_save) {
    // Test saving a configuration file
    TempFile tempFile;
    {
        auto cfg = getValidConfig();
        cfg["game"]["path"] = "Liberty City";

        tempFile.append(cfg);
    }

    RWConfig config;
    config.readConfigFile(tempFile.path());

    tempFile.remove();
    BOOST_CHECK(!tempFile.exists());

    config.writeConfigFile(tempFile.path());

    BOOST_CHECK(tempFile.exists());

    RWConfig config2;
    config2.readConfigFile(tempFile.path());

    BOOST_CHECK_EQUAL(config2.gameDataPath.value.value().string(), "Liberty City");

    simpleConfig_t cfg2 = readConfig(tempFile.path());
    BOOST_CHECK_EQUAL(cfg2["game"]["path"], "Liberty City");
}

BOOST_AUTO_TEST_CASE(test_config_valid_unknown_keys) {
    // Test reading a valid modified configuration file with unknown data
    auto cfg = getValidConfig();
    cfg["game"]["unknownkey"] = "descartes";
    cfg["dontknow"]["dontcare"] = "\t$%!$8847  %%$  ";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    config.readConfigFile(tempFile.path());

    BOOST_CHECK_GT(config.allConfigData.size(), 2);

    BOOST_CHECK_EQUAL(config.allConfigData.count("game.unknownkey"), 1);
    BOOST_CHECK_EQUAL(config.allConfigData.at("game.unknownkey"),
                      stripWhitespace(cfg["game"]["unknownkey"]));

    BOOST_CHECK_EQUAL(config.allConfigData.count("dontknow.dontcare"), 1);
    BOOST_CHECK_EQUAL(config.allConfigData.at("dontknow.dontcare"),
                      stripWhitespace(cfg["dontknow"]["dontcare"]));

    BOOST_CHECK_EQUAL(config.allConfigData.count("game.path"), 1);

    tempFile.remove();
    config.writeConfigFile(tempFile.path());

    RWConfig config2;
    config2.readConfigFile(tempFile.path());

    BOOST_CHECK_GT(config2.allConfigData.size(), 2);

    BOOST_CHECK_EQUAL(config2.allConfigData.count("game.unknownkey"), 1);
    BOOST_CHECK_EQUAL(config2.allConfigData.at("game.unknownkey"),
                      stripWhitespace(cfg["game"]["unknownkey"]));

    BOOST_CHECK_EQUAL(config2.allConfigData.count("dontknow.dontcare"), 1);
    BOOST_CHECK_EQUAL(config2.allConfigData.at("dontknow.dontcare"),
                      stripWhitespace(cfg["dontknow"]["dontcare"]));

    BOOST_CHECK_EQUAL(config2.allConfigData.count("game.path"), 1);
}

BOOST_AUTO_TEST_CASE(test_config_save_readonly) {
    // Test whether saving to a readonly INI file fails
    auto cfg = getValidConfig();

    TempFile tempFile;
    tempFile.append(cfg);
    tempFile.change_perms_readonly();

    RWConfig config;
    config.readConfigFile(tempFile.path());

    BOOST_CHECK_THROW(config.writeConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_valid_default) {
    // Test whether the default INI string is valid
    TempFile tempFile;
    BOOST_CHECK(!tempFile.exists());

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);

    auto defaultINI = config.getDefaultINIString();
    tempFile.append(defaultINI);
    BOOST_CHECK(tempFile.exists());

    config.readConfigFile(tempFile.path());
}

BOOST_AUTO_TEST_CASE(test_config_valid_alternative_bool_true) {
    // Test wrong data type
    auto cfg = getValidConfig();
    cfg["input"]["invert_y"] = "true";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK(!config.inputInvertY.hasValue());
    config.readConfigFile(tempFile.path());

    BOOST_CHECK(config.inputInvertY.hasValue());
    BOOST_CHECK(config.inputInvertY.value.value());
}

BOOST_AUTO_TEST_CASE(test_config_valid_alternative_bool_false) {
    // Test wrong data type
    auto cfg = getValidConfig();
    cfg["input"]["invert_y"] = "false";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK(!config.inputInvertY.hasValue());
    config.readConfigFile(tempFile.path());

    BOOST_CHECK(config.inputInvertY.hasValue());
    BOOST_CHECK(!config.inputInvertY.value.value());
}

BOOST_AUTO_TEST_CASE(test_config_valid_alternative_bool_on) {
    // Test wrong data type
    auto cfg = getValidConfig();
    cfg["input"]["invert_y"] = "on";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK(!config.inputInvertY.hasValue());
    config.readConfigFile(tempFile.path());

    BOOST_CHECK(config.inputInvertY.hasValue());
    BOOST_CHECK(config.inputInvertY.value.value());
}

BOOST_AUTO_TEST_CASE(test_config_valid_alternative_bool_off) {
    // Test wrong data type
    auto cfg = getValidConfig();
    cfg["input"]["invert_y"] = "off";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK(!config.inputInvertY.hasValue());
    config.readConfigFile(tempFile.path());

    BOOST_CHECK(config.inputInvertY.hasValue());
    BOOST_CHECK(!config.inputInvertY.value.value());
}

BOOST_AUTO_TEST_CASE(test_config_invalid_emptykey) {
    // Test duplicate keys in invalid configuration file
    auto cfg = getValidConfig();
    cfg["game"][""] = "0";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_invalid_duplicate) {
    // Test duplicate keys in invalid configuration file
    auto cfg = getValidConfig();
    cfg["input"]["invert_y    "] = "0";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_invalid_required_missing) {
    // Test missing required keys in invalid configuration file
    auto cfg = getValidConfig();
    cfg["game"].erase("path");

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);

    BOOST_CHECK(!config.gameDataPath.hasValue());
}

BOOST_AUTO_TEST_CASE(test_config_invalid_wrong_type) {
    // Test wrong data type
    auto cfg = getValidConfig();
    cfg["input"]["invert_y"] = "d";

    TempFile tempFile;
    tempFile.append(cfg);

    RWConfig config;

    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_invalid_empty) {
    // Test reading empty configuration file
    // An empty file has a valid data structure, but has missing keys and is
    // thus invalid.
    TempFile tempFile;
    tempFile.touch();
    BOOST_CHECK(tempFile.exists());

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_invalid_nodir) {
    // Test reading non-existing configuration file in non-existing directory
    TempDir tempDir;
    TempFile tempFile(tempDir);

    BOOST_CHECK(!tempDir.exists());
    BOOST_CHECK(!tempFile.exists());

    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_config_invalid_nonexisting) {
    // Test reading non-existing configuration file
    TempFile tempFile;

    BOOST_CHECK(!tempFile.exists());
    RWConfig config;
    BOOST_CHECK_THROW(config.readConfigFile(tempFile.path()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_arguments_nullptr) {
    RWConfig config;
    config.parseArguments(0, nullptr);
}

BOOST_AUTO_TEST_CASE(test_arguments_one) {
    RWConfig config;
    const char *args[] = {""};
    config.parseArguments(array_size(args), args);
}

BOOST_AUTO_TEST_CASE(test_arguments_notexisting) {
    RWConfig config;
    const char *args[] = {"", "--nonexistingprop"};
    BOOST_CHECK_THROW(config.parseArguments(array_size(args), args), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_arguments_positional) {
    RWConfig config;
    const char *args[] = {"", "positional"};
    BOOST_CHECK_THROW(config.parseArguments(array_size(args), args), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_arguments_bool) {
    RWConfig config;
    BOOST_CHECK(!config.displayHelp);

    const char *args[] = {"", "--help"};
    config.parseArguments(array_size(args), args);

    BOOST_CHECK(config.displayHelp);
}

BOOST_AUTO_TEST_CASE(test_arguments_path) {
    RWConfig config;
    BOOST_CHECK(!config.configPath.hasValue());

    auto path = rwfs::path("/some/path");
    const char *args[] = {"", "-c", path.string().c_str()};
    config.parseArguments(array_size(args), args);

    BOOST_CHECK(config.configPath.hasValue());
    BOOST_CHECK_EQUAL(path, config.configPath.value.value());
}

BOOST_AUTO_TEST_CASE(test_arguments_int) {
    RWConfig config;
    BOOST_CHECK(!config.windowWidth.hasValue());

    const size_t width = 1920;
    auto widthStr = std::to_string(width);
    const char *args[] = {"", "-w", widthStr.c_str()};
    config.parseArguments(array_size(args), args);

    BOOST_CHECK(config.windowWidth.hasValue());
    BOOST_CHECK_EQUAL(width, config.windowWidth.value.value());
}

BOOST_AUTO_TEST_CASE(test_arguments_int_invalid) {
    RWConfig config;
    BOOST_CHECK(!config.windowWidth.hasValue());

    const std::string widthStr = "1920d";
    const char *args[] = {"", "-w", widthStr.c_str()};

    BOOST_CHECK_THROW(config.parseArguments(array_size(args), args), std::runtime_error);

    BOOST_CHECK(!config.windowWidth.hasValue());
}

BOOST_AUTO_TEST_SUITE_END()
