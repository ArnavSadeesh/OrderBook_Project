#pragma once 
#include <vector>
#include <optional>

using Price = std::optional<std::int32_t>; 
using Quantity = std::uint32_t; 
using OrderId = std::uint64_t; 
using OrderIds = std::vector<OrderId>; 
