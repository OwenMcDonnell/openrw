#ifndef RWGAME_GAMECONFIG_HPP
#define RWGAME_GAMECONFIG_HPP
#include <rw/filesystem.hpp>
#include <rw/optional.hpp>

#include <cctype>
#include <map>
#include <string>
#include <vector>

template<typename T>
struct TypeTranslator;

class BaseRWConfigField {
public:
    enum field_t {
        /// DEFAULT: represents the default value
        DEFAULT,
        /// VALUE: represents the value
        VALUE,
    };
    BaseRWConfigField(const std::string &key, bool required)
        : key(key)
        , required(required) {
    }
    BaseRWConfigField(const BaseRWConfigField &) = default;
    BaseRWConfigField(BaseRWConfigField &&) = default;
    std::string key;
    bool required = false;
    virtual bool fromString(const std::string &s) = 0;
    virtual std::string toString(field_t field=VALUE) const = 0;
    virtual bool hasValue() const = 0;
};

template<typename T>
class RWConfigField : public virtual BaseRWConfigField {
public:
    /**
     * @brief Create a new RWConfigField. This class represents an optional configuration parameter with a default that should be saved to a file.
     * @param key Location of this config in the configuration file
     * @param default Default value of this configuration parameter
     * @param required Whether this parameter **MUST** be present
     * @param value Optional initial value of this configuration paramter
     */
    RWConfigField(const std::string &key, const T &default_,
                  bool required=false, rwopt::optional<T> value={})
        : BaseRWConfigField{key, required}
        , default_(default_)
        , value(value) {
    }
    T default_;
    rwopt::optional<T> value = {};

    /**
     * @brief Set the value of this configuration parameter to the value of the argument string.
     * @param s String representation of the value to set this parameter to
     * @return true if succesful, else false.
     */
    virtual bool fromString(const std::string &s) override {
        auto optValue = TypeTranslator<T>::fromString(s);
        if (optValue) {
            value = optValue;
        }
        return !!optValue;
    }

    /**
     * @brief Return the string representation of this configuration parameter value.
     * @param field Select whether to return the actual value or the default value
     * @return String representation
     */
    virtual std::string toString(field_t field) const override {
        switch(field) {
        case VALUE:
            if (value) {
                return TypeTranslator<T>::toString(value.value());
            }
            return "";
        case DEFAULT:
            return TypeTranslator<T>::toString(default_);
        default:
            return "";
        }
    }
    /**
     * @brief Check wheter this configuration parameter has an actual value
     * @return true if it has a value, else false
     */
    virtual bool hasValue() const override {
        return !!value;
    }
    /**
     * @brief Get the actual value of this configuration parameter if it exists. Else, return the default value.
     * @return The actual value of this configuration parameter if it exists. Else, the default value.
     */
    const T &getValueOrDefault() const {
        if (hasValue())
            return value.value();
        return default_;
    }
};

class RWConfig {
public:
    /* Actual Configuration */

    /// Path to the game data
    RWConfigField<rwfs::path> gameDataPath = {"game.path", "/path/to/gta3", true};

    /// Language for game
    RWConfigField<std::string> gameLanguage{"game.language", "american", true};

    /// Invert the y axis for camera control.
    RWConfigField<bool> inputInvertY{"input.invert_y", false};

    /// Size of the window
    RWConfigField<std::size_t> windowWidth{"window.width", 800};
    RWConfigField<std::size_t> windowHeight{"window.height", 600};

    /// Set the window to fullscreen
    RWConfigField<bool> windowFullscreen{"window.fullscreen", false};

    /// Options for start game
    bool startNewGame = false;
    RWConfigField<rwfs::path> startSaveGame{"", {}};
    bool startTestGame = false;
    RWConfigField<rwfs::path> startBenchmark{"", {}};

    /// Config file path
    RWConfigField<rwfs::path> configPath{"", RWConfig::defaultConfigPath()};

    /// Display help
    bool displayHelp = false;

    /// All data from the configuration file
    std::map<std::string, std::string> allConfigData;
private:
    std::vector<const BaseRWConfigField *> iterConfigFileFields() const {
        return {
            &gameDataPath,
            &gameLanguage,
            &inputInvertY,
            &windowWidth,
            &windowHeight,
            &windowFullscreen,
        };
    }
    std::vector<BaseRWConfigField *> iterConfigFileFields() {
        return {
            &gameDataPath,
            &gameLanguage,
            &inputInvertY,
            &windowWidth,
            &windowHeight,
            &windowFullscreen,
        };
    }
public:
    /**
     * @brief Create a new configuration. All settings are undefined or have default values.
     */
    RWConfig() = default;
    /**
     * @brief Read the configuration file at path.
     * Whitespace will be stripped from unknown data.
     * @param path Location of the configuration file.
     * @throws std::runtime_error if a problem occurs while reading the configuration file.
     */
    void readConfigFile(const rwfs::path &path);
    /**
     * @brief Write the configuration to a file at path.
     * @param path Location of the configuration file.
     * @throws std::runtime_error if a problem occurs while writing the configuration file.
     */
    void writeConfigFile(const rwfs::path &);
    /**
     * @brief Returns the content of the default INI
     * configuration.
     * @return INI string
     */
    std::string getDefaultINIString() const;

    /**
     * @brief Parse the command line arguments
     * @param argc Number of arguments
     * @param argv Arguments as an array of C strings.
     * @throws std::runtime_Error if an incorrect argument is passed
     */
    void parseArguments(int argc, const char *argv[]);
    /**
     * @brief Write the help for the arguments to a stream
     * @param os Output stream to write the help to
     * @param argv Arguments as an array of C strings.
     * @throws std::runtime_Error if an incorrect argument is passed
     */
    std::ostream &printArgumentsHelpText(std::ostream &os);
    /**
     * @brief Returns the path for the configuration file.
     */
    static rwfs::path defaultConfigPath();
};


template<>
struct TypeTranslator<std::string> {
    static rwopt::optional<std::string> fromString(const std::string &s) {
        return s;
    }
    static std::string toString(std::string s) {
        return s;
    }
};

template<>
struct TypeTranslator<bool> {
    static rwopt::optional<bool> fromString(const std::string &s) {
        std::string c = s;
        std::transform(c.begin(), c.end(), c.begin(), ::tolower);
        rwopt::optional<bool> res;
        try {
            int value = std::stoi(c);
            return value != 0;
        } catch (...) {
        }
        if (!c.compare("true") || !c.compare("on")) {
            return true;
        }
        if (!c.compare("false") || !c.compare("off")) {
            return false;
        }
        return res;
    }
    static std::string toString(const bool b) {
        return b ? "1" : "0";
    }
};

template<>
struct TypeTranslator<std::size_t> {
    static rwopt::optional<std::size_t> fromString(const std::string &s) {
        auto c = s;
        std::transform(c.begin(), c.end(), c.begin(), ::tolower);
        rwopt::optional<std::size_t> res;
        try {
            auto value = std::stoll(c, nullptr, 0);
            return static_cast<std::size_t>(value);
        } catch (...) {
        }
        return res;
    }
    static std::string toString(std::size_t s) {
        return std::to_string(s);
    }
};

template<>
struct TypeTranslator<rwfs::path> {
    static rwopt::optional<rwfs::path> fromString(const std::string &s) {
        return rwfs::path(s);
    }
    static std::string toString(const rwfs::path &path) {
        return path.string();
    }
};

#endif
