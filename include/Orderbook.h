#pragma once 

#include <map>
#include <unordered_map>
#include <thread>
#include <mutex> 
#include <condition_variable>

#include "Order.h"
#include "OrderModify.h"
#include "Orderbook_Level_Infos.h"
#include "Trade.h"
#include "Usings.h"

class Orderbook
{ 
    private: 
        /* Bundles an order with its location to enable O(1) 
        access within its level list*/
        struct OrderEntry
        {
            OrderPointer order_{ nullptr }; 
            OrderPointers::iterator location_; 
        };

        /* Metadata with quantity of financial product and 
        order count at each level */
        struct LevelData
        {
            Quantity quantity_{ }; 
            Quantity orderCount_{ }; 
            
            //Encapsulating actions that alter LevelData
            enum class Action
            { 
                Add, 
                Remove, 
                Match
            }; 
        }; 
        
        std::unordered_map<Price, LevelData> data_; 
        std::map<Price, OrderPointers, std::greater<Price>> bids_; 
        std::map<Price, OrderPointers> asks_; 
        std::unordered_map<OrderId, OrderEntry> orders_; 

        mutable std::mutex ordersMutex_; 
        std::thread orderPruningThread_; 
        std::condition_variable shutdownConditionVariable_; 
        std::atomic<bool> shutdown_ { false }; 

        /*A pruning clock that cancels GoodForDay orders at 4PM every day, designed to run on its own thread*/
        void PruneGoodForDayOrders();

        /*Allows efficient bulk cancellation, for internal use*/
        void CancelOrders(OrderIds orderIds);

        /*Primary cancel function intended to be called only through other
        thread-safe functions*/
        void CancelOrderInternal(OrderId orderId);

        /*APIs to update LevelData upon an order action*/
        void OnOrderAdded(OrderPointer order); 
        void OnOrderCancelled(OrderPointer order); 
        void OnOrderMatched(Price price, Quantity quantity, 
        bool isFullyFilled); 
        void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
        

        /*APIs to return whether an order can be matched for a trade or be
        fully filled across one or more trades*/
        bool CanMatch(Side side, Price price) const; 
        bool CanFullyFill(Side side, Price price, Quantity quantity) const; 

        /*Matches as many bid/ask orders as possible and returns 
        resulting trades*/
        Trades MatchOrders(); 


    public: 
        
        Orderbook();
        ~Orderbook(); 
        Orderbook(const Orderbook&) = delete; 
        void operator=(const Orderbook&) = delete; 
        Orderbook(Orderbook&&) = delete; 
        void operator=(Orderbook&& orderbook) = delete; 

        /*Adds order and returns any resulting Trades*/
        Trades AddOrder(OrderPointer order); 
      
        void CancelOrder(OrderId orderId); 

        /*Takes in an OrderModify object to find and cancel old order, 
        re-adding the modified version. Returns any resulting Trades*/
        Trades ModifyOrder(OrderModify order); 

        std::size_t Size() const; 
        
        //Returns compilation of orderbook's current bid/ask information  
        OrderbookLevelInfos GetOrderInfos() const; 
    
}; 
