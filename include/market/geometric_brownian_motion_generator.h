#ifndef MARKET_DATA_SYSTEM_GEOMETRIC_BROWNIAN_MOTION_H
#define MARKET_DATA_SYSTEM_GEOMETRIC_BROWNIAN_MOTION_H

#include <market/price_generator.h>
#include <random>
#include <cmath>
#include <iostream>

template <Arithmetic PriceType>
class GBMGenerator : public IPriceGenerator<PriceType>
{
public:
    // Parameters: startPrice, mu (drift), sigma (volatility), dt (time step)
    GBMGenerator(PriceType startPrice, double mu, double sigma, double dt_s)
        : currentPrice_{startPrice},
          mu_{mu},
          sigma_{sigma},
          dt_{dt_s},
          rngEngine_{randomDevice_()},
          normalDistribution_{0.0, 1.0}
    {
        if (startPrice <= 0)
        {
            currentPrice_ = 1.0;
        }
    }

    /**
     * @brief Calculates the next price using the Geometric Brownian Motion (GBM) model.
     *
     * This function implements the Euler-Maruyama discretization:
     * S(t+dt) = S(t) * exp( log_return )
     * where log_return = (mu - 0.5*sigma^2)dt + sigma*sqrt(dt)*Z
     *
     * VARIABLE GLOSSARY:
     *
     * S(t)         : The **Current Price** of the asset (currentPrice_ before update).
     * S(t+dt)      : The **Simulated Price** at the next time step (currentPrice_ after update).
     * mu           : The **Drift** (annualized expected rate of return, mu_).
     * sigma        : The **Volatility** (annualized standard deviation, sigma_).
     * dt           : The **Time Step** (time increment in years, dt_).
     * Z            : The **Standard Normal Random Variable** ($\mathcal{N}(0, 1)$), the source of random shock.
     * sqrt(dt)     : The time-scaling factor applied to the random shock.
     * 0.5 * sigma^2: The **Itô Correction** (adjustment term for volatility).
     */
    PriceType getNextPrice() override
    {
        // 1. Generate standard normal random sample (Z)
        double Z = normalDistribution_(rngEngine_);
        double sqrt_dt = std::sqrt(dt_);

        // 2. Use the Multiplicative (Log-Normal) Update Formula
        // S(t+dt) = S(t) * exp( log_return )
        // log_return = (mu - 0.5*sigma^2)dt + sigma*sqrt(dt)*Z
        double log_return = (mu_ - 0.5 * sigma_ * sigma_) * dt_ + sigma_ * sqrt_dt * Z;

        // 3. Apply the update
        currentPrice_ *= std::exp(log_return);

        // 4. Ensure price remains positive
        if (currentPrice_ <= 0)
        {
            currentPrice_ = 0.01;
        }
        return currentPrice_;
    }

private:
    PriceType currentPrice_;
    double mu_;
    double sigma_;
    double dt_;
    std::random_device randomDevice_;
    std::mt19937 rngEngine_;
    std::normal_distribution<double> normalDistribution_;
};

#endif // MARKET_DATA_SYSTEM_GEOMETRIC_BROWNIAN_MOTION_H