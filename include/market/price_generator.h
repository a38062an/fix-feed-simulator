//
// Created by Anthony Nguyen on 13/11/2025.
//
#include <type_traits>


#ifndef MARKET_DATA_SYSTEM_PRICE_GENERATOR_H
#define MARKET_DATA_SYSTEM_PRICE_GENERATOR_H

// Concept to be later used
template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

// Forces PriceType to be of concept Arithmetic (float, double, int ... )
template<Arithmetic PriceType>
class IPriceGenerator
{
public:
   // Forcing derived child classes to call their destructors first
   virtual ~IPriceGenerator() = default;

   // Assigning 0 makes this the class abstract making children override the function
   virtual PriceType getNextPrice() = 0;
};


#endif //MARKET_DATA_SYSTEM_PRICE_GENERATOR_H