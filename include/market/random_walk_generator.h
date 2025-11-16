//
// Created by Anthony Nguyen on 13/11/2025.
//
#include <market/price_generator.h>
#include <random>

#ifndef MARKET_DATA_SYSTEM_RANDOM_WALK_GENERATOR_H
#define MARKET_DATA_SYSTEM_RANDOM_WALK_GENERATOR_H

template <Arithmetic PriceType>

class RandomWalkGenerator : public IPriceGenerator<PriceType>
{
public:
    RandomWalkGenerator(PriceType startPrice, PriceType stepSize)
        : currentPrice_{startPrice},
          stepSize_{std::abs(stepSize)},
          rngEngine_{randomDevice_()},
          normalDistribution_{0.0, 1.0}
    {
    }

    PriceType getNextPrice() override
    {
        // 1. Generate random value
        double randomNumber = normalDistribution_(rngEngine_);

        // 2. Determine the step direction (negative if randomNumber is negative)
        PriceType step = (randomNumber > 0.0) ? stepSize_ : -stepSize_;

        // 3. Apply the step
        currentPrice_ += step;

        // 4. Check if price is 0 or less (edge case)
        if (currentPrice_ <= 0)
        {
            // Reset value to small positive value (stepSize_ is always positive)
            currentPrice_ = stepSize_;
        }

        return currentPrice_;
    }

private:
    PriceType currentPrice_;
    PriceType stepSize_;

    std::random_device randomDevice_;
    std::mt19937 rngEngine_;
    std::normal_distribution<double> normalDistribution_;
};

#endif // MARKET_DATA_SYSTEM_RANDOM_WALK_GENERATOR_H