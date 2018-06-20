#include "GameConfig.hpp"
#include "GitSHA1.h"

#include <SDL2/SDL.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace py = pybind11;

#include "RWGame.hpp"

std::string pythonExecutable() {
    auto modSys = py::module::import("sys");
    return py::cast<std::string>(modSys.attr("executable"));
}

//FIXME: pybind11 supports optional
template<typename T>
void registerOptional(py::module &m, const std::string &name) {
    py::class_<rwopt::optional<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const T &>(), py::arg("optional"))
        .def("__repr__", [name](const rwopt::optional<T> &o) {
            std::string contents;
            if (!o) {
                contents = "nil";
            } else {
                contents = static_cast<std::string>(py::repr(py::cast(o.value())));
            }
            return "<" + name + ":" + contents + ">";
        });
}

template<typename T>
void registerRWConfigField(py::module &m, const std::string &name) {
    py::class_<RWConfigField<T>>(m, name.c_str())
        .def(py::init<const std::string &, const T&, bool, rwopt::optional<T>>(), py::arg("key"), py::arg("default"), py::arg("required")=false, py::arg("value")=rwopt::optional<T>{})
        .def("__repr__", [name](const RWConfigField<T> &f) {
            return "<" + name + ":key=" + static_cast<std::string>(py::repr(py::cast(f.key)))
                    + ",default=" + static_cast<std::string>(py::repr(py::cast(f.default_)))
                    + ",required=" + static_cast<std::string>(py::repr(py::cast(f.required)))
                    + ",value=" + static_cast<std::string>(py::repr(py::cast(f.value))) + ">";
        }).def_readonly("required", &RWConfigField<T>::required)
        .def_readonly("default", &RWConfigField<T>::default_)
        .def_readonly("key", &RWConfigField<T>::key)
        .def_readwrite("value", &RWConfigField<T>::value);
}

void registerPath(py::module &m) {
    py::class_<rwfs::path>(m, "Path")
        .def(py::init<>())
        .def(py::init<std::string>())
        .def(py::self / std::string{})
        .def("__str__", (const std::string &(rwfs::path::*)() const) &rwfs::path::string)
        .def("__repr__", [](const rwfs::path &p) {
            return "<pyrw.Path=\"" + p.string() + "\">";
        });
}


void registerRWConfig(py::module &m) {
    auto rwConfig = py::class_<RWConfig>(m, "RWConfig")
        .def(py::init<>())
        .def("parse_arguments", [](RWConfig &c, py::list args) {
            std::vector<std::string> vStrArgs;
            vStrArgs.reserve(args.size());

            for (const auto &argI : args) {
                vStrArgs.push_back(py::cast<std::string>(argI));
            }
            std::vector<const char *> vArgs;
            std::transform(vStrArgs.cbegin(), vStrArgs.cend(), std::back_inserter(vArgs), [](const std::string &s) { return s.data();});

            return c.parseArguments(vArgs.size(), vArgs.data());
        }).def("read_config_file", &RWConfig::readConfigFile, py::arg("path") = RWConfig::defaultConfigPath())
        .def("write_config_file", &RWConfig::writeConfigFile, py::arg("path") = RWConfig::defaultConfigPath())
        .def_readwrite("start_new_game", &RWConfig::startNewGame)
        .def_readwrite("start_test_game", &RWConfig::startTestGame)
        .def_readwrite("display_help", &RWConfig::displayHelp)
        .def_readonly("game_data_path", &RWConfig::gameDataPath)
        .def_readonly("game_language", &RWConfig::gameLanguage)
        .def_readonly("input_invert_y", &RWConfig::inputInvertY)
        .def_readonly("window_width", &RWConfig::windowWidth)
        .def_readonly("window_height", &RWConfig::windowHeight)
        .def_readonly("window_fullscreen", &RWConfig::windowFullscreen)
        .def_readonly("start_savegame", &RWConfig::startSaveGame)
        .def_readonly("start_benchmark", &RWConfig::startBenchmark)
        .def_readonly("config_path", &RWConfig::configPath)
        .def_static("default_config_path", &RWConfig::defaultConfigPath);
}

PYBIND11_MODULE(pyrw, m) {
    SDL_SetMainReady();

    m.attr("__version__") = kGitSHA1Hash;

    registerPath(m  );
/*
    registerOptional<rwfs::path>(m, "OptionalPath");
    registerOptional<std::size_t>(m, "OptionalSizeT");
    registerOptional<std::string>(m, "OptionalStr");
    registerOptional<bool>(m, "OptionalBool");
*/
    registerRWConfigField<rwfs::path>(m, "RWConfigFieldPath");
    registerRWConfigField<std::size_t>(m, "RWConfigFieldSizeT");
    registerRWConfigField<std::string>(m, "RWConfigFieldString");
    registerRWConfigField<bool>(m, "RWConfigFieldBool");

    registerRWConfig(m);
}
