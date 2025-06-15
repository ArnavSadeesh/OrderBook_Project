#pragma once

#include "Usings.h"

//A TradeInfo bundles details of a filled order (bid or ask) in a trade
struct TradeInfo 
{ 
    OrderId orderId_; 
    Price price_; 
    Quantity quantity_; 
}; 
