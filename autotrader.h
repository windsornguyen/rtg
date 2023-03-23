// Copyright 2021 Optiver Asia Pacific Pty. Ltd.
//
// This file is part of Ready Trader Go.
//
//     Ready Trader Go is free software: you can redistribute it and/or
//     modify it under the terms of the GNU Affero General Public License
//     as published by the Free Software Foundation, either version 3 of
//     the License, or (at your option) any later version.
//
//     Ready Trader Go is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU Affero General Public License for more details.
//
//     You should have received a copy of the GNU Affero General Public
//     License along with Ready Trader Go.  If not, see
//     <https://www.gnu.org/licenses/>.

/*----------------------------------------------------------------------------*/
/* autotrader.h                                                               */
/* Author: Windsor Nguyen (Team: Princeton Printers 2.024)                    */
/* School: Princeton University                                               */
/* Year: 2025                                                                 */
/* Major: Computer Science                                                    */
/* Minors: Machine Learning & Statistics, Applied & Computational Mathematics */
/*----------------------------------------------------------------------------*/

#ifndef CPPREADY_TRADER_GO_AUTOTRADER_H
#define CPPREADY_TRADER_GO_AUTOTRADER_H

#include <array>
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include <boost/asio/io_context.hpp>

#include <ready_trader_go/baseautotrader.h>
#include <ready_trader_go/types.h>

/*----------------------------------------------------------------------------*/

class AutoTrader : public ReadyTraderGo::BaseAutoTrader
{
public:
    /**

    @brief: Constructs an instance of the AutoTrader class.
    This constructor initializes an instance of the AutoTrader class with a
    boost::asio::io_context object.

    @param: `context` The boost::asio::io_context object to use
    for asynchronous operations.
    */
    explicit AutoTrader(boost::asio::io_context &context);

    /*------------------------------------------------------------------------*/

    /**

    @brief: Called when the execution connection is lost.
    */
    void DisconnectHandler() override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles error messages from the execution engine
    and logs them appropriately.

    @param: `clientOrderId` The ID of the order that generated the error message.
    @param: `errorMessage` The error message to be handled.

    Called when the matching engine detects an error. If the error pertains to a
    particular order, then the client_order_id will identify that order,
    otherwise the client_order_id will be zero.
    */
    void ErrorMessageHandler(unsigned long clientOrderId,
                             const std::string &errorMessage) override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles filled hedge order messages.

    @param: clientOrderId: The ID of the client order.
    @param: price: The price at which the order was filled.
    @param: volume: The number of lots filled.

    Called when one of your hedge orders is filled, partially or fully.
    The price is the average price at which the order was (partially) filled,
    which may be better than the order's limit price. The volume is
    the number of lots filled at that price.
    If the order was unsuccessful, both the price and volume will be zero.
    */
    void HedgeFilledMessageHandler(unsigned long clientOrderId,
                                   unsigned long price,
                                   unsigned long volume) override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles incoming order book messages for a specific instrument.

    @param: instrument The instrument for which the order book is received.
    @param: sequenceNumber The sequence number for the order book message.
    @param: askPrices The array of ask prices for the top levels of the order book.
    @param: askVolumes The array of ask volumes for the top levels of the order book.
    @param: bidPrices The array of bid prices for the top levels of the order book.
    @param: bidVolumes The array of bid volumes for the top levels of the order book.

    Called periodically to report the status of an order book.
    The sequence number can be used to detect missed or out-of-order
    messages. The five best available ask (i.e. sell) and bid (i.e. buy)
    prices are reported along with the volume available at each of those
    price levels.
    */
    void OrderBookMessageHandler(ReadyTraderGo::Instrument instrument,
                                 unsigned long sequenceNumber,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles the message when an order is filled.
    @param: clientOrderId The unique identifier of the order that was filled.
    @param: price The price at which the order was filled (in cents).
    @param: volume The volume of the order that was filled (in lots).

    Called when one of your orders is filled, partially or fully.
    */
    void OrderFilledMessageHandler(unsigned long clientOrderId,
                                   unsigned long price,
                                   unsigned long volume) override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles the message when an order status is updated.

    @param: clientOrderId The unique identifier of the order that was updated.
    @param: fillVolume The volume of the order that was filled (in lots).
    @param: remainingVolume The volume of the order that remains unfilled (in lots).
    @param: fees The fees incurred by the order (in cents).

    Called when the status of one of your orders changes.
    The fill volume is the number of lots already traded, remaining volume
    is the number of lots yet to be traded and fees is the total fees paid
    or received for this order.
    Remaining volume will be set to zero if the order is cancelled.
    */
    void OrderStatusMessageHandler(unsigned long clientOrderId,
                                   unsigned long fillVolume,
                                   unsigned long remainingVolume,
                                   signed long fees) override;

    /*------------------------------------------------------------------------*/

    /**

    @brief: Handles the message when trade ticks are received for an instrument.

    @param: instrument The instrument for which the trade ticks are received.
    @param: sequenceNumber The sequence number of the trade tick message.
    @param: askPrices An array of the ask prices for the instrument (in cents).
    @param: askVolumes An array of the ask volumes for the instrument (in lots).
    @param: bidPrices An array of the bid prices for the instrument (in cents).
    @param: bidVolumes An array of the bid volumes for the instrument (in lots).

    Called periodically when there is trading activity on the market.
    The five best ask (i.e. sell) and bid (i.e. buy) prices at which there
    has been trading activity are reported along with the aggregated volume
    traded at each of those price levels.
    If there are less than five prices on a side, then zeros will appear at
    the end of both the prices and volumes arrays.
    */
    void TradeTicksMessageHandler(ReadyTraderGo::Instrument instrument,
                                  unsigned long sequenceNumber,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

    /*------------------------------------------------------------------------*/

private:
    enum class IchimokuSignal
    {
        BUY,
        SELL,
        NEUTRAL
    };
    using PriceVolumeArray = std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT>;
    IchimokuSignal Ichimoku(const PriceVolumeArray &askPrices,
                         const PriceVolumeArray &askVolumes,
                         const PriceVolumeArray &bidPrices,
                         const PriceVolumeArray &bidVolumes);
    void updateCircularBufferWithNewValue(std::vector<unsigned long> &buffer, int &index, int &count, unsigned long newValue, unsigned long &sum, int maxSize);

    unsigned long mNextMessageId = 1;                                // The next message id to use
    unsigned long mAskId = 0;                                        // The current ask order id.
    unsigned long mBidId = 0;                                        // The current bid order id.
    unsigned long mAskPrice = 0;                                     // The current ask price.
    unsigned long mBidPrice = 0;                                     // The current bid price.
    signed long mPosition = 0;                                       // The current position (in lots).
    signed long mHedges = 0;                                         // The number of hedges (in lots).
    std::unordered_set<unsigned long> mAsks;                         // A set of active ask order ids.
    std::unordered_set<unsigned long> mBids;                         // A set of active bid order ids.
    std::unordered_map<unsigned long, unsigned long> shortInventory; // maps id and cost
    std::unordered_map<unsigned long, unsigned long> longInventory;  // maps id and cost
    std::unordered_map<unsigned long, unsigned long> lotSize;        // maps id and size
    bool PendingCancelAsk = false;
    bool PendingCancelBid = false;
};

#endif // CPPREADY_TRADER_GO_AUTOTRADER_H


