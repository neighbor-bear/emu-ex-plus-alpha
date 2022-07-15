#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <concepts>
#include <limits>

namespace IG
{

template <class T>
static constexpr unsigned bitSize = std::numeric_limits<T>::digits;

template <std::unsigned_integral T = unsigned>
constexpr static T bit(unsigned bitIdx)
{
	return (T)1 << bitIdx;
}

template <std::unsigned_integral T = unsigned>
constexpr static T bits(unsigned numBits)
{
	return std::numeric_limits<T>::max() >> (bitSize<T> - numBits);
}

constexpr static auto setBits(std::integral auto x, std::integral auto mask)
{
	return x | mask; // OR mask to set
}

constexpr static auto clearBits(std::integral auto x, std::integral auto mask)
{
	return x & ~mask; // AND with the NOT of mask to unset
}

constexpr static auto setOrClearBits(std::integral auto x, std::integral auto mask, bool condition)
{
	return condition ? setBits(x, mask) : clearBits(x, mask);
}

constexpr static auto flipBits(std::integral auto x, std::integral auto mask)
{
	return x ^ mask; // XOR mask to flip
}

constexpr static auto updateBits(std::integral auto x, std::integral auto mask, std::integral auto updateMask)
{
	return setBits(clearBits(x, updateMask), mask);
}

constexpr static auto swapBits(std::integral auto x, std::integral auto range1, std::integral auto range2, std::integral auto rangeSize)
{
	auto t = ((x >> range1) ^ (x >> range2)) & ((1 << rangeSize) - 1); // XOR temporary
	return x ^ ((t << range1) | (t << range2));
}

constexpr static bool isBitMaskSet(std::integral auto x, std::integral auto mask)
{
	return (x & mask) == mask; //AND mask, if the result equals mask, all bits match
}

}
