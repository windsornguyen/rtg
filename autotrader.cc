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
/* autotrader.cc                                                              */
/* Author: Windsor Nguyen                                                     */
/* School: Princeton University                                               */
/* Class Year: 2025                                                           */
/* Major: Computer Science                                                    */
/* Minors: Machine Learning & Statistics, Applied & Computational Mathematics */
/*----------------------------------------------------------------------------*/

#include "autotrader.h"
#include <boost/asio/io_context.hpp>
#include <ready_trader_go/logging.h>

using namespace ReadyTraderGo;

RTG_INLINE_GLOBAL_LOGGER_WITH_CHANNEL(LG_AT, "AUTO")

constexpr int LOT_SIZE = 200;
constexpr int UNLOAD = 25;
constexpr int BASELINE_SIZE = 9;
constexpr int POSITION_LIMIT = 100;
constexpr int CONVERSION_LINE_SIZE = 10;
constexpr int TICK_SIZE_IN_CENTS = 100;
constexpr int MIN_BID_NEAREST_TICK = (MINIMUM_BID + TICK_SIZE_IN_CENTS) / TICK_SIZE_IN_CENTS * TICK_SIZE_IN_CENTS;
constexpr int MAX_ASK_NEAREST_TICK = MAXIMUM_ASK / TICK_SIZE_IN_CENTS * TICK_SIZE_IN_CENTS;

/*----------------------------------------------------------------------------*/

enum class IchimokuSignal
{
  BUY,
  SELL,
  NEUTRAL
};

int conversion_line_size = 9;
int baseline_size = 26;
int leading_span_b_window = 52;

// Declare circular buffers and sums for indicators
std::vector<unsigned long> conversionLineBuffer(conversion_line_size);
std::vector<unsigned long> baselineBuffer(baseline_size);
std::vector<unsigned long> leadingSpanBBuffer(leading_span_b_window);
std::vector<unsigned long> laggingSpanBuffer(baseline_size);
unsigned long conversionLineSum = 0;
unsigned long baselineSum = 0;
unsigned long leadingSpanBSum = 0;
int conversionLineIndex = 0;
int baselineIndex = 0;
int leadingSpanBIndex = 0;
int laggingSpanIndex = 0;
int conversionLineCount = 0;
int baselineCount = 0;
int leadingSpanBCount = 0;
int laggingSpanCount = 0;

/*----------------------------------------------------------------------------*/

AutoTrader::AutoTrader(boost::asio::io_context &context) : BaseAutoTrader(context) {}

/*----------------------------------------------------------------------------*/

void AutoTrader::DisconnectHandler()
{
  BaseAutoTrader::DisconnectHandler();
  RLOG(LG_AT, LogLevel::LL_INFO) << "execution connection lost";
}

/*----------------------------------------------------------------------------*/

void AutoTrader::ErrorMessageHandler(unsigned long clientOrderId,
                                     const std::string &errorMessage)
{
  ;
  RLOG(LG_AT, LogLevel::LL_INFO) << "error with order " << clientOrderId << ": " << errorMessage;
  if (clientOrderId != 0 && ((mAsks.count(clientOrderId) == 1) ||
                             (mBids.count(clientOrderId) == 1)))
  {
    OrderStatusMessageHandler(clientOrderId, 0, 0, 0);
  }
}

/*----------------------------------------------------------------------------*/

void AutoTrader::HedgeFilledMessageHandler(unsigned long clientOrderId,
                                           unsigned long price,
                                           unsigned long volume)
{
  RLOG(LG_AT, LogLevel::LL_INFO) << "hedge order " << clientOrderId << " filled for " << volume << " lots at $" << price << " average price in cents";
}

/*----------------------------------------------------------------------------*/

void AutoTrader::updateCircularBufferWithNewValue(std::vector<unsigned long> &buffer, int &index, int &count, unsigned long newValue, unsigned long &sum, int maxSize)
{
  if (count < maxSize)
  {
    buffer[index] = newValue;
    sum += newValue;
    count++;
  }
  else
  {
    sum -= buffer[index];
    buffer[index] = newValue;
    sum += newValue;
  }
  index = (index + 1) % maxSize;
}

/*----------------------------------------------------------------------------*/

AutoTrader::IchimokuSignal AutoTrader::Ichimoku(const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                                const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                                const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                                const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes)
{
  unsigned long currentPrice = (bidPrices[0] + askPrices[0]) >> 1;

  // Conversion Line calculation
  updateCircularBufferWithNewValue(conversionLineBuffer, conversionLineIndex, conversionLineCount, currentPrice, conversionLineSum, conversion_line_size);
  unsigned long conversionLine = conversionLineSum / conversionLineCount;

  // Base Line calculation
  updateCircularBufferWithNewValue(baselineBuffer, baselineIndex, baselineCount, conversionLine, baselineSum, baseline_size);
  unsigned long baseline = baselineSum / baselineCount;

  // Leading Span A calculation
  unsigned long leadingSpanA = (conversionLine + baseline) >> 1;

  // Leading Span B calculation
  updateCircularBufferWithNewValue(leadingSpanBBuffer, leadingSpanBIndex, leadingSpanBCount, conversionLine, leadingSpanBSum, leading_span_b_window);
  unsigned long leadingSpanB = (leadingSpanBSum + baseline) >> 1;

  // Lagging Span (Chikou Span) calculation
  updateCircularBufferWithNewValue(laggingSpanBuffer, laggingSpanIndex, laggingSpanCount, currentPrice, leadingSpanBSum, baseline_size);
  unsigned long laggingSpan = laggingSpanBuffer[laggingSpanIndex];

  // Trading strategy
  bool priceAboveCloud = currentPrice > std::min(leadingSpanA, leadingSpanB);
  bool priceBelowCloud = currentPrice < std::max(leadingSpanA, leadingSpanB);
  bool priceAboveConversionAndBase = currentPrice > conversionLine && currentPrice > baseline;
  bool priceBelowConversionAndBase = currentPrice < conversionLine && currentPrice < baseline;
  bool laggingSpanAbovePrice = laggingSpan > laggingSpanBuffer[(laggingSpanIndex - baseline_size + laggingSpanBuffer.size()) % laggingSpanBuffer.size()];
  bool laggingSpanBelowPrice = laggingSpan < laggingSpanBuffer[(laggingSpanIndex - baseline_size + laggingSpanBuffer.size()) % laggingSpanBuffer.size()];

  // Initialize ICHIMOKU as 'no signal'
  IchimokuSignal signal = IchimokuSignal::NEUTRAL;

  // Positive indicator => BUY
  if ((priceAboveCloud || priceAboveConversionAndBase) && laggingSpanAbovePrice)
  {
    signal = IchimokuSignal::BUY;
  }

  // Negative indicator => SELL
  if ((priceBelowCloud || priceBelowConversionAndBase) && laggingSpanBelowPrice)
  {
    signal = IchimokuSignal::SELL;
  }

  // If neither of the indicators are hit, the value of ICHIMOKU remains 'no signal'
  return signal;
}

/*----------------------------------------------------------------------------*/

static double WeightedAverageSpread(const std::array<unsigned long, TOP_LEVEL_COUNT> &askPrices,
                                    const std::array<unsigned long, TOP_LEVEL_COUNT> &askVolumes,
                                    const std::array<unsigned long, TOP_LEVEL_COUNT> &bidPrices,
                                    const std::array<unsigned long, TOP_LEVEL_COUNT> &bidVolumes)
{
  double totalSpreadVolume = 0.0;
  double weightedSpreadSum = 0.0;

  for (int i = 0; i < TOP_LEVEL_COUNT; ++i)
  {
    if (askPrices[i] != 0 && bidPrices[i] != 0)
    {
      double spread = (askPrices[i] - bidPrices[i]) >> 1;
      double spreadVolume = std::min(askVolumes[i], bidVolumes[i]);
      totalSpreadVolume += spreadVolume;
      weightedSpreadSum += spread * spreadVolume;
    }
  }

  if (totalSpreadVolume > 0.0)
  {
    return weightedSpreadSum / totalSpreadVolume;
  }
  else
  {
    return 0.0;
  }
}

/*----------------------------------------------------------------------------*/

void AutoTrader::OrderBookMessageHandler(Instrument instrument,
                                         unsigned long sequenceNumber,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &askPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &askVolumes,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &bidPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &bidVolumes)
{
  RLOG(LG_AT, LogLevel::LL_INFO) << "order book received for "
                                 << instrument << " instrument"
                                 << ": ask prices: " << askPrices[0]
                                 << "; ask volumes: " << askVolumes[0]
                                 << "; bid prices: " << bidPrices[0]
                                 << "; bid volumes: " << bidVolumes[0];

  double weightedAverageSpread = WeightedAverageSpread(askPrices, askVolumes, bidPrices, bidVolumes);
  unsigned long priceAdjustment = -(mPosition / LOT_SIZE) * TICK_SIZE_IN_CENTS;
  unsigned long currentPrice = (bidPrices[0] + askPrices[0]) >> 1;
  unsigned long newAskPrice = (askPrices[0] != 0) ? askPrices[0] + priceAdjustment + weightedAverageSpread : 0;
  unsigned long newBidPrice = (bidPrices[0] != 0) ? bidPrices[0] + priceAdjustment - weightedAverageSpread : 0;
  newAskPrice = (newAskPrice / TICK_SIZE_IN_CENTS) * TICK_SIZE_IN_CENTS;
  newBidPrice = (newBidPrice / TICK_SIZE_IN_CENTS) * TICK_SIZE_IN_CENTS;
  unsigned long askQuantity = std::min<unsigned long>(LOT_SIZE, static_cast<unsigned long>(POSITION_LIMIT + mPosition));
  unsigned long bidQuantity = std::min<unsigned long>(LOT_SIZE, static_cast<unsigned long>(POSITION_LIMIT - mPosition));

  if (instrument == Instrument::FUTURE)
  {
    if (newAskPrice != 0 && newAskPrice != mAskPrice)
    {
      if (mAskId != 0)
      {
        SendCancelOrder(mAskId);
        PendingCancelAsk = true;
      }
      if (!PendingCancelAsk && askQuantity > 0)
      {

        mAskId = mNextMessageId++;
        SendInsertOrder(mAskId, Side::SELL, newAskPrice, askQuantity, Lifespan::GOOD_FOR_DAY);
        mAsks.emplace(mAskId);
      }
      mAskPrice = newAskPrice;
    }
    if (newBidPrice != 0 && newBidPrice != mBidPrice)
    {
      if (mBidId != 0)
      {
        SendCancelOrder(mBidId);
        PendingCancelBid = true;
      }
      if (!PendingCancelBid && bidQuantity > 0)
      {

        mBidId = mNextMessageId++;
        SendInsertOrder(mBidId, Side::BUY, newBidPrice, bidQuantity, Lifespan::GOOD_FOR_DAY);
        mBids.emplace(mBidId);
      }
      mBidPrice = newBidPrice;
    }

    IchimokuSignal signal = Ichimoku(askPrices, askVolumes, bidPrices, bidVolumes);
    if (leadingSpanBBuffer.size() >= 52 && signal == IchimokuSignal::BUY)
    {
      // BUY SIGNAL
      if (newBidPrice != 0 && newBidPrice != mBidPrice)
      {
        if (mBidId != 0)
        {
          SendCancelOrder(mBidId);
          PendingCancelBid = true;
        }
        if (!PendingCancelBid && bidQuantity > 0)
        {
          mBidId = mNextMessageId++;
          SendInsertOrder(mBidId, Side::BUY, newBidPrice, bidQuantity, Lifespan::GOOD_FOR_DAY);
          mBids.emplace(mBidId);
        }
        mBidPrice = newBidPrice;
      }
    }
    else if (signal == IchimokuSignal::SELL)
    {
      // SELL SIGNAL
      if (newAskPrice != 0 && newAskPrice != mAskPrice)
      {
        if (mAskId != 0)
        {
          SendCancelOrder(mAskId);
          PendingCancelAsk = true;
        }
        if (!PendingCancelAsk && askQuantity > 0)
        {
          mAskId = mNextMessageId++;
          SendInsertOrder(mAskId, Side::SELL, newAskPrice, askQuantity, Lifespan::GOOD_FOR_DAY);
          mAsks.emplace(mAskId);
        }
        mAskPrice = newAskPrice;
      }
    }

    // Check if the position is outside of the unload range
    if (mPosition >= UNLOAD || mPosition <= -UNLOAD)
    {
      // Check profit-taking for each long position
      for (const auto &[longId, longCostBasis] : longInventory)
      {
        if (newAskPrice != 0 && newAskPrice != mAskPrice)
        {

          // Check profit-taking for long position
          if (longCostBasis < currentPrice)
          {
            if (newAskPrice != 0 && newAskPrice != mAskPrice)
            {
              if (mAskId != 0)
              {
                SendCancelOrder(mAskId);
                PendingCancelAsk = true;
              }
              if (!PendingCancelAsk && askQuantity > 0)
              {
                mAskId = mNextMessageId++;
                SendInsertOrder(mAskId, Side::SELL, newAskPrice, askQuantity, Lifespan::GOOD_FOR_DAY);
                mAsks.emplace(mAskId);
              }
              mAskPrice = newAskPrice;
            }
          }
        }
      }
      // Check profit-taking for each short position
      for (const auto &[shortId, shortCostBasis] : shortInventory)
      {
        // Check profit-taking for short position
        if (shortCostBasis > currentPrice)
        {
          // Place new bid order if none exist and new bid price is valid
          if (newBidPrice != 0 && newBidPrice != mBidPrice)
          {
            if (mBidId != 0)
            {
              SendCancelOrder(mBidId);
              PendingCancelBid = true;
            }
            if (!PendingCancelBid && bidQuantity > 0)
            {
              mBidId = mNextMessageId++;
              SendInsertOrder(mBidId, Side::BUY, newBidPrice, bidQuantity, Lifespan::GOOD_FOR_DAY);
              mBids.emplace(mBidId);
            }
            mBidPrice = newBidPrice;
          }
        }
      }
    }
  }
}

/*----------------------------------------------------------------------------*/

void AutoTrader::OrderFilledMessageHandler(unsigned long clientOrderId,
                                           unsigned long price,
                                           unsigned long volume)
{
  if (mAsks.count(clientOrderId) == 1)
  {
    long newPosition = mPosition - static_cast<long>(volume);
    if (std::abs(newPosition + mHedges) <= POSITION_LIMIT && std::abs(newPosition) <= POSITION_LIMIT)
    {
      mPosition = newPosition;
    }
    SendHedgeOrder(mNextMessageId++, Side::BUY, MAX_ASK_NEAREST_TICK, volume);
    shortInventory.emplace(clientOrderId, price);
  }
  else if (mBids.count(clientOrderId) == 1)
  {
    long newPosition = mPosition + static_cast<long>(volume);
    if (std::abs(newPosition - mHedges) <= POSITION_LIMIT && std::abs(newPosition) <= POSITION_LIMIT)
    {
      mPosition = newPosition;
    }
    SendHedgeOrder(mNextMessageId++, Side::SELL, MIN_BID_NEAREST_TICK, volume);
    longInventory.emplace(clientOrderId, price);
  }
  lotSize.emplace(clientOrderId, volume);
}

/*----------------------------------------------------------------------------*/

void AutoTrader::OrderStatusMessageHandler(unsigned long clientOrderId,
                                           unsigned long fillVolume,
                                           unsigned long remainingVolume,
                                           signed long fees)
{
  if (remainingVolume == 0)
  {
    if (clientOrderId == mAskId)
    {
      mAskId = 0;
      PendingCancelAsk = false;
    }
    else if (clientOrderId == mBidId)
    {
      mBidId = 0;
      PendingCancelBid = false;
    }

    // Remove from mAsks and mBids only if they exist there
    if (mAsks.count(clientOrderId) == 1)
    {
      mAsks.erase(clientOrderId);
    }
    if (mBids.count(clientOrderId) == 1)
    {
      mBids.erase(clientOrderId);
    }
  }
  else
  {
    longInventory[clientOrderId] += fees;
    shortInventory[clientOrderId] -= fees;
    lotSize[clientOrderId] = remainingVolume;
  }
}

/*----------------------------------------------------------------------------*/

void AutoTrader::TradeTicksMessageHandler(Instrument instrument,
                                          unsigned long sequenceNumber,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &askPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &askVolumes,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &bidPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &bidVolumes)
{
  RLOG(LG_AT, LogLevel::LL_INFO) << "trade ticks received for " << instrument << " instrument"
                                 << ": ask prices: " << askPrices[0]
                                 << "; ask volumes: " << askVolumes[0]
                                 << "; bid prices: " << bidPrices[0]
                                 << "; bid volumes: " << bidVolumes[0];
}


