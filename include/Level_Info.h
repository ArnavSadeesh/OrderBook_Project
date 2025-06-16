#pragma once
#include "Usings.h"

/*A LevelInfo bundles a price with quantity of the security and 
quantity of orders at that price level*/
  
struct LevelInfo 
{
    Price price_; 
    Quantity quantity_; 
    Quantity orderCount_; 
}; 

using LevelInfos = std::vector<LevelInfo>; 