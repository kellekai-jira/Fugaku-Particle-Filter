#include "helpers.hpp"

std::string melissa::helpers::make_uuid()
{
  return boost::lexical_cast<std::string>((boost::uuids::random_generator())());
}
