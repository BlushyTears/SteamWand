#pragma once

#include <array>
#include <bitset>
#include <typeinfo>
#include <unordered_map>

constexpr int MAX_ITEMS = 1000;

struct DataBlock {
	std::bitset<MAX_ITEMS> Data;
	// Philosophy: This should only be utilized if we misscalculate space needed
	DataBlock* next = nullptr;
};

// This describes some internal, human-readable functions
template <typename T>
struct Attatchments {
	// Uses OR bit operator to add bit flags
	void CreateAttatchment() {

	}
	// Shifts the bits in order to sort for more common data accesses
	void ShiftAttatchment() {

	}
	void ReadAttachment() {

	}
	// Uses NOT bit operator to remove bit flags
	void RemoveAttatchment() {

	}

private:
	// Here we should describe what every attachment means (It's unique signature)
	// Here we should also 
	// Template instantiations supported
};

// Human interface with quick access iterators, template-based api, sorting, caching and interacting with binary
// This could also be how you instantiate data, but we might need one more manager for that, although I like
// The idea of having the struct that creates the data for the user, also serve as everything else the user interacts with.
struct IIterator {

};

