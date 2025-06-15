#pragma once 

#include "Order.h"

/*A Data Transfer Object that encapsulates a modify request for an order. 
  Intended to be used with the ModifyOrder api of an orderbook. */
class OrderModify 
{
public: 
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity) 
      : orderId_{ orderId }, price_ { price }, 
        side_ { side }, quantity_ { quantity } 
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