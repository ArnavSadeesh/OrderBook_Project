#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <deque>
#include <queue>
#include <stack>
#include <list> 
#include <algorithm>
#include <cmath>
#include <ctime>
#include <numeric>
#include <format>

enum class OrderType { 
    GoodTillCancel, 
    FillAndKill, 
}; 

enum class Side { 
    Buy, 
    Sell
}; 

using Price = std::int32_t;  
using Quantity = std::uint32_t; 
using OrderId = std::uint64_t; 

//A LevelInfo object bundles a price with quantity of orders at that level
struct LevelInfo 
{
    Price price_; 
    Quantity quantity_; 
}; 

using LevelInfos = std::vector<LevelInfo>; 

//An internal summary of orderbook
class OrderbookLevelInfos 
{
    public: 
        OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{ bids }, asks_{ asks } 
        { }

        const LevelInfos& GetBids() const { return bids_; }
        const LevelInfos& GetAsks() const { return asks_; }

    private: 
        LevelInfos bids_; 
        LevelInfos asks_; 
}; 


class Order 
{
    public: 
        Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{orderType}, orderId_{orderId}, side_{side},
        price_{price}, initialQuantity_{quantity}, 
        remainingQuantity_{quantity}
        { }
    
        OrderId GetOrderId() const { return orderId_; }
        Side GetSide() const { return side_; }
        Price GetPrice() const { return price_; }
        OrderType GetOrderType() const { return orderType_;}
        Quantity GetInitialQuantity() const { return initialQuantity_; }
        Quantity GetRemainingQuantity() const { return remainingQuantity_; }
        Quantity GetFilledQuantity() const 
        { 
            return GetInitialQuantity() - GetRemainingQuantity(); 
        }
        bool IsFilled() const { return GetRemainingQuantity() == 0;}
        void Fill(Quantity quantity) 
        {
            if (quantity > GetRemainingQuantity()) 
                throw std::logic_error(std::format(
                    "Order {} cannot be filled for more than its remaining quantity {}", 
                    GetOrderId(), 
                    GetRemainingQuantity())
                ); 
            remainingQuantity_ -= quantity; 
        }

    private: 
        OrderType orderType_; 
        OrderId orderId_; 
        Side side_; 
        Price price_; 
        Quantity initialQuantity_; 
        Quantity remainingQuantity_;  
}; 

using OrderPointer = std::shared_ptr<Order>; 
//List allows dispersed memory storage 
using OrderPointers = std::list<OrderPointer>;  



class OrderModify 
{
    public: 
         OrderModify(OrderId orderId, Side side, Price price, 
                     Quantity quantity) 
            : orderId_{ orderId }, price_ { price }, side_ { side }, 
            quantity_ { quantity } 
            { }
        OrderId GetOrderId() const { return orderId_; }
        Price GetPrice() const { return price_; }
        Side GetSide() const { return side_; }
        Quantity GetQuantity() const {return quantity_;}
        
        OrderPointer ToOrderPointer(OrderType type) const 
        { 
            return std::make_shared<Order>(type, GetOrderId(), 
                                           GetSide(), GetPrice(),
                                           GetQuantity()); 
        }

    private: 
        OrderId orderId_; 
        Price price_; 
        Side side_; 
        Quantity quantity_; 
}; 

//A TradeInfo captures the details of a filled order (bid or ask) in a trade
struct TradeInfo 
{ 
    OrderId orderId_; 
    Price price_; 
    Quantity quantity_; 
}; 


class Trade
{ 
    public: 
        Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
            : bidTrade_{ bidTrade }, askTrade_ { askTrade }
            { }; 
        const TradeInfo& GetBidTrade() const { return bidTrade_; }
        const TradeInfo& GetAskTrade() const { return askTrade_; }
    
    private: 
        TradeInfo bidTrade_; 
        TradeInfo askTrade_; 
}; 

using Trades = std::vector<Trade>; 

class OrderBook
{ 
    private: 
        struct OrderEntry
        {
            OrderPointer order_{ nullptr }; 
            OrderPointers::iterator location_; 
        };
        
        std::map<Price, OrderPointers, std::greater<Price>> bids_; 
        std::map<Price, OrderPointers> asks_; 
        std::unordered_map<OrderId, OrderEntry> orders_; 

        //Returns whether an order of 'side' with 'price' can be matched for a trade
        bool CanMatch(Side side, Price price) const 
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

        /*Matches as many bid/ask orders as possible and returns resulting trades*/
        Trades MatchOrders()
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
                    //Quantity should be part of Trade constructor 
                    trades.push_back(Trade{ 
                        TradeInfo {
                            bid->GetOrderId(), bid->GetPrice(), quantity
                        }, 
                        TradeInfo{
                            ask->GetOrderId(), ask->GetPrice(), quantity, 
                        }
                    }); 
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

    public: 
        //Adds order to the orderbook and returns Trades resulting from the addition
        Trades AddOrder(OrderPointer order)
        { 
            if (orders_.contains(order->GetOrderId()))
                return { }; 
            
            if (order->GetOrderType() == OrderType::FillAndKill
                && !CanMatch(order->GetSide(), order->GetPrice()))
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

            return MatchOrders(); 
        }

        //Removes order with orderId from orderbook
        void CancelOrder(OrderId orderId) 
        { 
            if (!orders_.contains(orderId))
                return; 
        
            const auto [order, orderLocation] = orders_[orderId];  
            orders_.erase(orderId); 

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
                    bids_.erase(order->GetPrice()); 
            }
        }
        /*Takes in an OrderModify object order to find and remove original 
        version and add the modified order. Returns Trades made as a result 
        of the addition*/
        Trades ModifyOrder(OrderModify order) 
        { 
            if (!orders_.contains(order.GetOrderId()))
                return { }; 
        
            const auto& [existingOrder, _] = orders_[order.GetOrderId()]; 
            CancelOrder(order.GetOrderId()); 
            return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
        }

        std::size_t Size() const { return orders_.size(); }
        
        //Returns compilation of orderbook's current bid/ask information  
        OrderbookLevelInfos GetOrderInfos() const 
        { 
            LevelInfos bidInfos, askInfos; 
            bidInfos.reserve(orders_.size()); 
            askInfos.reserve(orders_.size()); 
            
            //Generic function to create LevelInfo for a price level
            auto CreateLevelInfo = [](Price price, const OrderPointers& orders)
            { 
                return LevelInfo
                { 
                price,
                std::accumulate(orders.begin(), orders.end(), Quantity(0), 
                                [](Quantity runningSum, const OrderPointer& order)
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

}; 


int main() { 
    OrderBook orderbook; 
    const OrderId orderId = 1; 
    orderbook.AddOrder(std::make_shared<Order>
        (OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10)); 
    std::cout << orderbook.Size() << std::endl; //1

    orderbook.CancelOrder(orderId); 
    std::cout << orderbook.Size() << std::endl;  //0

    return 0;
}

