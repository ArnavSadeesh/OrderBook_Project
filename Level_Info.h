#pragma once
#include "Usings.h"

/*A LevelInfo bundles a price with total quantity of the security across
  all orders at that price level*/
  
struct LevelInfo 
{
    Price price_; 
    Quantity quantity_; 
}; 

using LevelInfos = std::vector<LevelInfo>; 