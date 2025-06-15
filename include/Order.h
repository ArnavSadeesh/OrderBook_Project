#pragma once

#include <list>
#include <exception>
#include <format>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

class Order 
{
public: 
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }, orderId_{ orderId }, side_{ side },
            price_{ price }, initialQuantity_{ quantity }, 
            remainingQuantity_{ quantity }
    { }

    Order(OrderId orderId, Side side, Quantity quantity) 
        : Order(OrderType::Market, orderId, side, std::nullopt, quantity)
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

    void ToGoodTillCancel(Price price) 
    { 
        if (GetOrderType() != OrderType::Market)
            throw std::logic_error(std::format(
                "Order {} is not a market order. Cannot modify price and type.", 
                GetOrderId())
            ); 
        
        price_ = price; 
        orderType_ = OrderType::GoodTillCancel; 
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

