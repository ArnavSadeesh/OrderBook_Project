#pragma once
#include "Usings.h"

//A LevelInfo object bundles a price with quantity of orders at that level
struct LevelInfo 
{
    Price price_; 
    Quantity quantity_; 
}; 

using LevelInfos = std::vector<LevelInfo>; 