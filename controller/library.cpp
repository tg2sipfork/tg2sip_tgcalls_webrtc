#include "controller/library.hpp"
#include "controller/tgcalls.hpp"
#define LibraryClass TgCallsLibrary

voip::Library& voip::Library::instance() {
    static LibraryClass instance;
    return instance;
}
