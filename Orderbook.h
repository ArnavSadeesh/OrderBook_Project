#pragma once 

#include <map>
#include <unordered_map>

#include "Order.h"
#include "OrderModify.h"
#include "Orderbook_Level_Infos.h"
#include "Trades.h"
#include "Usings.h"


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
