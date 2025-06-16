#pragma once
#include "Level_Info.h"
#include <numeric>

//A summary of orderbook internals
class OrderbookLevelInfos 
{
public: 
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
    : bids_{ bids }, asks_{ asks } 
    { }

    const LevelInfos& GetBidInfos() const { return bids_; }
    const LevelInfos& GetAskInfos() const { return asks_; }
    Quantity GetBidCount() const
    { 
        return std::accumulate(bids_.begin(), bids_.end(), Quantity(0), 
                                [](Quantity runningSum, const auto& currLevel)
                                { 
                                    return runningSum + currLevel.orderCount_; 
                                });           
    }
    Quantity GetAskCount() const
    { 
        return std::accumulate(asks_.begin(), asks_.end(), Quantity(0), 
                                [](Quantity runningSum, const auto& currLevel)
                                { 
                                    return runningSum + currLevel.orderCount_; 
                                });           
    }

private: 
    LevelInfos bids_; 
    LevelInfos asks_; 
}; 