#ifndef TG2SIP_CONTROLLER_LIBRARY_HPP
#define TG2SIP_CONTROLLER_LIBRARY_HPP

#include "controller/controller.hpp"

namespace voip {
class Library {
public:
    virtual ~Library() = default;
    virtual int callProtoMinLayer() = 0;
    virtual int callProtoMaxLayer() = 0;
    virtual std::vector<std::string> versions() = 0;

    virtual std::unique_ptr<Controller> createController(Controller::CreateCtx &&) = 0;

    static Library& instance();
};
}

#endif // TG2SIP_CONTROLLER_LIBRARY_HPP
