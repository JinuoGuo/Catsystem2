#pragma once

#include <stdexcept>
#include <string>

namespace catsystem2
{

class Error final : public std::runtime_error
{
  public:
    explicit Error(const std::string& message) : std::runtime_error(message)
    {
    }
};

}
