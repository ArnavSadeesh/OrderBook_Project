#include "pch.h" 
#include "Orderbook.h"    

enum class ActionType
{ 
    Add, 
    Modify,
    Cancel
}; 

struct Action 
{ 
    ActionType type_; 
    OrderType orderType_; 
    Side side_; 
    Price price_; 
    Quantity quantity_; 
    OrderId orderId_; 
}; 

using Actions = std::vector<Action>; 

 struct Result
 { 
    std::size_t allCount_; 
    std::size_t bidCount_; 
    std::size_t askCount_; 
 };

 struct InputHandler
 { 
    private: 
        std::uint64_t ToNumber(const std::string_view& str) const 
        { 
            std::int64_t value{}; 
            std::from_chars(str.data(), str.data() + str.size(), value); 

            if (value < 0)
                throw std::logic_error("Only non-negative values allowed!"); 
            return static_cast<std::uint64_t> (value); 
        }

        std::vector<std::string_view> Split(const std::string_view& str, 
            char delimiter) const
        { 
            std::vector<std::string_view> columns{ }; 
            std::size_t startIndex{ }, endIndex{ }; 
            while ((endIndex = str.find(delimiter, startIndex)) && 
                   endIndex != std::string::npos)
            { 
                auto partLen = endIndex - startIndex; 
                columns.push_back(str.substr(startIndex, partLen)); 
                startIndex = endIndex+1; 
            }

            columns.push_back(str.substr(startIndex)); 
            return columns; 
        }

        bool ParseResult(const std::string_view& str, Result& result) const 
        { 
            if (str[0] != 'R')
                return false; 
            
            auto values = Split(str, ' ');
            result.allCount_ = ToNumber(values[1]); 
            result.bidCount_ = ToNumber(values[2]); 
            result.askCount_ = ToNumber(values[3]); 
            
            return true; 
        }

        bool ParseInfoLine(const std::string_view& str, Action& info) const 
        {
            auto values = Split(str, ' '); 
            auto actionType = values[0]; 
            if (actionType == "A")
            { 
                info.type_ = ActionType::Add; 
                info.side_ = ParseSide(values[1]); 
                info.orderType_ = ParseOrderType(values[2]); 
                info.price_ = ParsePrice(values[3]); 
                info.quantity_ = ParseQuantity(values[4]); 
                info.orderId_ = ParseOrderId(values[5]); 
            }
            else if (actionType == "M")
            { 
                info.type_ = ActionType::Modify; 
                info.orderId_ = ParseOrderId(values[1]); 
                info.side_ = ParseSide(values[2]); 
                info.price_ = ParsePrice(values[3]); 
                info.quantity_ = ParseQuantity(values[4]); 
            }
            else if (actionType == "C")
            {
                info.type_ = ActionType::Cancel; 
                info.orderId_ = ParseOrderId(values[1]); 
            }
            else 
                return false; 
            
            return true; 
        }

        Side ParseSide(const std::string_view& str) const 
        { 
            if (str == "B")
                return Side::Buy; 
            else if (str == "S")
                return Side::Sell; 
            else 
                throw std::logic_error("Invalid Side!"); 
        }

        OrderType ParseOrderType(const std::string_view& str) const 
        { 
            if (str == "GoodTillCancel")
                return OrderType::GoodTillCancel; 
            else if (str == "GoodForDay")
                return OrderType::GoodForDay; 
            else if (str == "FillAndKill")
                return OrderType::FillAndKill; 
            else if (str == "FillOrKill")
                return OrderType::FillOrKill; 
            else if (str == "Market")
                return OrderType::Market; 
            else 
                throw std::logic_error("Invalid order type!"); 
        }

        Price ParsePrice(const std::string_view& str) const 
        { 
            if (str.empty())
                throw std::logic_error("Invalid Price"); 

            return ToNumber(str); 
        }

        Quantity ParseQuantity(const std::string_view& str) const 
        { 
            if (str.empty())
                throw std::logic_error(std::format("Invalid Quantity: {}", str));  

            return ToNumber(str); 
        }

        OrderId ParseOrderId(const std::string_view& str) const 
        { 
            if (str.empty())
                throw std::logic_error("Invalid OrderId"); 

            return ToNumber(str); 
        }

    public: 
        
        std::tuple<Actions, Result> GetInputInfo(
            const std::filesystem::path& path) const
        { 
            Actions actions; 
            actions.reserve(1000); 

            std::string line; 
            std::ifstream file{ path }; 

            while (std::getline(file, line)) 
            { 
                if (line.empty())
                    break; 

                const bool isResult = (line[0] == 'R'); 
                const bool isAction = !isResult; 

                if (isAction)
                { 
                    Action action; 

                    auto isValid = ParseInfoLine(line, action); 
                    if (!isValid)
                        throw std::logic_error(std::format(
                            "Invalid action: {}", line
                        )); 
                
                    actions.push_back(action); 
                }
                else 
                { 
                    if (!file.eof())
                        throw std::logic_error("Result line must be at end of file!");

                    Result result; 
                    ParseResult(line, result); 
    
                    return { actions, result}; 
                }
            }

            throw std::logic_error("No result specified!"); 
        }
 }; 

 //Testing framework that each test instance inherits from
 class OrderbookTestsFixture : public testing::TestWithParam<const char*>
 { 
    private:  
        const static inline std::filesystem::path Root { 
            std::filesystem::current_path()
        };
        const static inline std::filesystem::path TestFilesDir{ "Test_Files"}; 
    public: 
        const static inline std::filesystem::path TestFolderPath{ Root / "Testing" / TestFilesDir }; 
 }; 

 //Parametrized test to be used for every input instance 
 TEST_P (OrderbookTestsFixture, OrderbookTestSuite) 
 { 
    //Arrange
    const auto file = OrderbookTestsFixture::TestFolderPath / GetParam(); 

    InputHandler handler; 
    const auto [actions, result] = handler.GetInputInfo(file); 

    //Convert text info to an order
    auto GetOrder = [](const Action& action)
    { 
        return std::make_shared<Order>(
            action.orderType_, 
            action.orderId_, 
            action.side_,
            action.price_, 
            action.quantity_
        ); 
    };
    
    //Convert text info to an order modify 
    auto GetOrderModify = [](const Action& action)
    { 
        return OrderModify {
            action.orderId_, 
            action.side_,
            action.price_, 
            action.quantity_
        }; 
    }; 

    //Process actions
    Orderbook orderbook; 
    for (const auto& action: actions)
    { 
        switch(action.type_)
        { 
        case ActionType::Add: 
            orderbook.AddOrder(GetOrder(action)); 
            break; 
        case ActionType::Modify: 
            orderbook.ModifyOrder(GetOrderModify(action)); 
            break; 
        case ActionType::Cancel: 
            orderbook.CancelOrder(action.orderId_); 
            break; 
        default: 
            throw std::logic_error("Unsupported Action!"); 
        } 
    }  

    //Compare against Result 
    const auto& orderbookLevelInfos = orderbook.GetOrderInfos(); 
    ASSERT_EQ(orderbook.Size(), result.allCount_); 
    ASSERT_EQ(orderbookLevelInfos.GetBidCount(), result.bidCount_); 
    ASSERT_EQ(orderbookLevelInfos.GetAskCount(), result.askCount_); 
 }

 //Creates a derived fixture instance for every specified file 
 INSTANTIATE_TEST_SUITE_P(Tests, OrderbookTestsFixture, testing::ValuesIn({ 
    "Cancel_Success.txt", 
    "Match_FillAndKill.txt", 
    "Match_FillOrKill_Hit.txt",
    "Match_FillOrKill_Miss.txt", 
    "Match_GoodTillCancel.txt",
    "Match_Market.txt",
    "Modify_Price.txt", 
    "Modify_Side.txt",
    "NoMatch_GoodTillCancel.txt"
    })); 
  