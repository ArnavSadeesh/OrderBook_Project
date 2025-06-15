#include "Orderbook.h"

#include <numeric>
#include <ctime>
#include <chrono>
#include <iostream>

void Orderbook::PruneGoodForDayOrders()
{
    using namespace std::chrono;
    //4 PM Pruning
    const auto end = hours(16); 

    while (true) 
    { 
        const auto now = system_clock::now(); 
        const auto now_c = system_clock::to_time_t(now); 
        std::tm now_parts;
        localtime_r(&now_c, &now_parts);

        //If already past 4 PM, increment prune time to tomorrow
        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1;

        //Set prune hour to 4 PM
        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;
        
        //Calculate time to sleep with some delay
        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100);    

        { 
            std::unique_lock ordersLock{ ordersMutex_ }; 

            //If orderbook is shutdown prematurely
            if (shutdown_.load(std::memory_order_acquire) || 
                shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
                    return;
        }

        OrderIds orderIds; 
        { 
            std::scoped_lock ordersLock{ ordersMutex_ }; 

            for (const auto& [_, orderEntry] : orders_) 
            {
                const auto& order = orderEntry.order_;  
                if (order->GetOrderType() == OrderType::Market)
                    orderIds.push_back(order->GetOrderId()); 
            }
        }

        CancelOrders(orderIds); 
    }

}

void Orderbook::CancelOrders(OrderIds orderIds)
{ 
    std::scoped_lock ordersLock{ ordersMutex_ }; 

    for (OrderId orderId: orderIds) 
        CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId) 
{ 
    if (!orders_.contains(orderId))
        return; 

    const auto& [order, orderLocation] = orders_[orderId];  

    if (order->GetSide() == Side::Buy) 
    { 
        auto& ordersAtPrice = bids_[order->GetPrice()]; 
        ordersAtPrice.erase(orderLocation); 
        if (ordersAtPrice.empty())
            bids_.erase(order->GetPrice()); 
    }  
    else 
    { 
        auto& ordersAtPrice = asks_[order->GetPrice()]; 
        ordersAtPrice.erase(orderLocation); 
        if (ordersAtPrice.empty())
            asks_.erase(order->GetPrice()); 
    }

    OnOrderCancelled(order); 
    
    orders_.erase(orderId); 
}

void Orderbook::OnOrderAdded(OrderPointer order)
{ 
    UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add); 
}


void Orderbook::OnOrderCancelled(OrderPointer order)
{ 
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove); 
}


void Orderbook::OnOrderMatched(Price price, Quantity quantity, 
    bool isFullyFilled)
{ 
    UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match); 
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action) 
{ 
    auto& data = data_[price]; 
    if (action == LevelData::Action::Remove)
    {
        data.orderCount_ -= 1; 
        data.quantity_ -= quantity; 
    }
    else if (action == LevelData::Action::Add)
    {
        data.orderCount_ += 1; 
        data.quantity_ += quantity; 
    } 
    else 
        data.quantity_ -= quantity; 

    if (!data.orderCount_)
        data_.erase(price);
}

bool Orderbook::CanMatch(Side side, Price price) const 
{ 
    if (side == Side::Buy) { 
        if (asks_.empty())
            return false; 
        
        const auto& [bestAsk, _] = *asks_.begin(); 
        return price >= bestAsk;   
    }
    else 
    { 
        if (bids_.empty())
            return false; 
        
        const auto& [bestBid, _] = *bids_.begin(); 
        return price <= bestBid;    
    }
 }

 bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
 { 
    if (!CanMatch(side, price))
        return false; 

    Price thresholdPrice; 

    if (side == Side::Buy)
    {
        const auto& [bestAskPrice, _] = *asks_.begin(); 
        thresholdPrice = bestAskPrice; 
    }
    else 
    { 
        const auto& [bestBidPrice, _] = *bids_.begin(); 
        thresholdPrice = bestBidPrice; 
    }

    for (const auto& [levelPrice, levelData] : data_)
    { 
        if ((side == Side::Buy &&
            levelPrice >= thresholdPrice && levelPrice <= price) || 
            (side == Side::Sell &&
            levelPrice <= thresholdPrice && levelPrice >= price))
            {
                if (quantity <= levelData.quantity_)
                    return true; 

                quantity -= levelData.quantity_; 
            }
    }
    return false; 
 }

Trades Orderbook::MatchOrders()
{  
    Trades trades; 
    trades.reserve(orders_.size()); 

    while (true) {
        if (bids_.empty() || asks_.empty())
            break; 
        auto&  [bidPrice, bids] = *bids_.begin(); 
        auto&  [askPrice, asks] = *asks_.begin(); 

        if (bidPrice < askPrice)
            break; 
        
        while (bids.size() && asks.size()) {
            auto& bid = bids.front(); 
            auto& ask = asks.front(); 

            Quantity quantity = std::min(bid->GetRemainingQuantity(), 
            ask->GetRemainingQuantity()); 

            bid->Fill(quantity); 
            ask->Fill(quantity); 

            if (bid->IsFilled()) { 
                bids.pop_front(); 
                orders_.erase(bid->GetOrderId()); 
            }

            if (ask->IsFilled()) { 
                asks.pop_front(); 
                orders_.erase(ask->GetOrderId()); 
            }

            trades.push_back(Trade{ 
                TradeInfo {
                    bid->GetOrderId(), bid->GetPrice(), quantity
                }, 
                TradeInfo{
                    ask->GetOrderId(), ask->GetPrice(), quantity, 
                }
            }); 

            OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled()); 
            OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled()); 
        }

        if (bids.empty())
            bids_.erase(bidPrice); 
        else
            asks_.erase(askPrice); 
    }

    if (!bids_.empty()) { 
        auto& [_, bids] = *bids_.begin(); 
        auto& order =  bids.front(); 
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrder(order->GetOrderId()); 
    }

    if (!asks_.empty()) { 
        auto& [_, asks] = *asks_.begin(); 
        auto& order =  asks.front(); 
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrder(order->GetOrderId()); 
    }
    return trades; 
}

Orderbook::Orderbook()
    : orderPruningThread_{ [this]{ PruneGoodForDayOrders(); } }
    {}

Orderbook::~Orderbook() 
{ 
    shutdown_.store(true, std::memory_order_release); 
    shutdownConditionVariable_.notify_one(); 
    orderPruningThread_.join(); 
}

Trades Orderbook::AddOrder(OrderPointer order)
{ 
    std::scoped_lock ordersLock{ ordersMutex_ }; 

    if (orders_.contains(order->GetOrderId()))
        return { }; 
    
    /*Market orders redefined as GoodTillCancel orders at worst bid or ask  
      to allow same behavior without extra branch to handle Market type */
    if (order->GetOrderType() == OrderType::Market) 
    { 
        if (order->GetSide() == Side::Buy && !asks_.empty())
        { 
            const auto& [worstAsk, _] = *asks_.rbegin(); 
            order->ToGoodTillCancel(worstAsk); 
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty()) 
        { 
            const auto& [worstBid, _] = *bids_.rbegin(); 
            order->ToGoodTillCancel(worstBid); 
        } 
        else
            return { }; 
    }
    
    if (order->GetOrderType() == OrderType::FillAndKill
        && !CanMatch(order->GetSide(), order->GetPrice()))
            return { }; 
    
    if (order->GetOrderType() == OrderType::FillOrKill
        && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
            return { }; 
    
    OrderPointers::iterator iterator; 

    if (order->GetSide() == Side::Buy) 
    { 
        auto& orders = bids_[order->GetPrice()]; 
        orders.push_back(order); 
        iterator = std::next(orders.begin(), orders.size()-1); 
    } 
    else 
    { 
        auto& orders = asks_[order->GetPrice()]; 
        orders.push_back(order); 
        iterator = std::next(orders.begin(), orders.size()-1); 
    }
    
    orders_[order->GetOrderId()] = OrderEntry { order, iterator }; ; 

    OnOrderAdded(order); 

    return MatchOrders(); 
}


//Removes order with orderId from orderbook
void Orderbook::CancelOrder(OrderId orderId) 
{ 
    std::scoped_lock ordersLock{ ordersMutex_ }; 

    CancelOrderInternal(orderId); 
}


/*Takes in an OrderModify object order to find and remove original 
version and add the modified order. Returns Trades made as a result 
of the addition*/
Trades Orderbook::ModifyOrder(OrderModify order) 
{ 
    OrderType orderType; 
    {
        std::scoped_lock ordersLock{ ordersMutex_ }; 

        if (!orders_.contains(order.GetOrderId()))
            return { }; 

        const auto& [existingOrder, _] = orders_[order.GetOrderId()]; 
        orderType = existingOrder->GetOrderType(); 
    }

    CancelOrder(order.GetOrderId()); 
    return AddOrder(order.ToOrderPointer(orderType));
}


std::size_t Orderbook::Size() const 
{   
    std::scoped_lock ordersLock{ ordersMutex_ }; 
    return orders_.size(); 
}
   

//Returns compilation of orderbook's current bid/ask information  
OrderbookLevelInfos Orderbook::GetOrderInfos() const 
{ 
    LevelInfos bidInfos, askInfos; 
    bidInfos.reserve(Size()); 
    askInfos.reserve(Size()); 
    
    //Generic function to create LevelInfo for a price level
    auto CreateLevelInfo = [](Price price, const OrderPointers& orders)
    { 
        return LevelInfo
        { 
        price,
        std::accumulate(orders.begin(), orders.end(), Quantity(0), 
                        [](Quantity runningSum, const auto& order)
                        {
                            return runningSum + order->GetRemainingQuantity(); 
                        })
        }; 
    }; 

    for (const auto& [price, orders] : bids_)
        bidInfos.push_back(CreateLevelInfo(price, orders));
    
    for (const auto& [price, orders] : asks_)
        askInfos.push_back(CreateLevelInfo(price, orders));


    return OrderbookLevelInfos { bidInfos, askInfos }; 

}

