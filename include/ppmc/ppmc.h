#pragma once
#include <fstream>
#include <format>
#include "..\BitIO.h"
#include <string>
#include <bitset>
#include "model.h"


void encodeSymbol(std::unique_ptr<stl::BitFile>& output, Symbol& s, USHORT& low, USHORT& high, USHORT& underflowBits) {
	unsigned long range = (high - low) + 1;
	high = low + static_cast<USHORT>((range * s.highCount) / s.scale - 1);
	low = low + static_cast<USHORT>((range * s.lowCount) / s.scale);
	//the following loop churns out new bits until high and low are far enough apart to have stabilized
	for (;;) {
		//if their MSBs are the same
		if ((high & 0x8000) == (low & 0x8000)) {
			stl::outputBit(output, high & 0x8000);
			while (underflowBits > 0) {
				stl::outputBit(output, (~high) & 0x8000);
				underflowBits--;
			}
		}
		//if low's first and second MSBs are 01 and high's first and second MSBs are 10, and underflow is about to occur
		else if ((low & 0x4000) && !(high & 0x4000)) {
			underflowBits++;
			//toggle the second MSB in both low and high.
			high |= (1 << 14);
			low &= ~(1 << 14);
		}
		else {
			return;
		}
		low <<= 1;
		high <<= 1;
		high |= 1;
	}
}

void flushArithmeticEncoder(std::unique_ptr<stl::BitFile>& output, USHORT high, USHORT& underflowBits) {
	stl::outputBit(output, high & 0x8000);
	++underflowBits;
	while (underflowBits > 0) {
		stl::outputBit(output, ~high & 0x8000);
		underflowBits--;
	}
}


inline void initializeArithmeticDecoder(std::unique_ptr<stl::BitFile>& input, USHORT& code) {
	for (int i{ 0 }; i < 16; ++i) {
		code <<= 1;
		code |= stl::inputBit(input);
	}
}

inline long getCurrentIndex(Symbol& s, USHORT low, USHORT high, USHORT code) {
	long range{ high - low + 1 };
	long index = (long)(((code - low) + 1) * s.scale - 1) / range;
	return index;
}

void removeSymbolFromStream(std::unique_ptr<stl::BitFile>& input, Symbol& s, USHORT& low, USHORT& high, USHORT& code) {
	long range{ (high - low) + 1 };
	high = low + (USHORT)((range * s.highCount) / s.scale - 1);
	low = low + (USHORT)((range * s.lowCount) / s.scale);
	for (;;) {
		if ((high & 0x8000) == (low & 0x8000)) {
			//do nothing
		}
		else if ((low & 0x4000) && !(high & 0x4000)) {
			code ^= 0x4000;
			high |= (1 << 14);//set bit
			low &= ~(1 << 14);//clear bit
		}
		else
			return;
		low <<= 1;
		high <<= 1;
		high |= 1;
		code <<= 1;
		code |= stl::inputBit(input);
	}
}

void compressFile(std::fstream& input, std::unique_ptr<stl::BitFile>& output, uint32_t order) {
	int c{};
	USHORT low{ 0 }, high{ 0xffff }, underflowBits{ 0 };
	Symbol s;
	initializeModel(order);
	bool escaped{};
	for (;;) {
		c = input.get();
		if (c == EOF)
			c = END_OF_STREAM;
		escaped = convertIntToSymbol(c, s);
		encodeSymbol(output, s, low, high, underflowBits);
		while (escaped) {
			escaped = convertIntToSymbol(c, s);
			encodeSymbol(output, s, low, high, underflowBits);
		}
		if (c == END_OF_STREAM)
			break;
		updateModel(c);
	}
	flushArithmeticEncoder(output, high, underflowBits);
	stl::outputBits(output, 0L, 16);
}


void expandFile(std::unique_ptr<stl::BitFile>& input, std::fstream& output, uint32_t order) {
	Symbol s;
	int c{};
	USHORT low{ 0 }, high{ 0xffff }, code{ 0 };
	long index{ 0 };
	initializeModel(order);
	initializeArithmeticDecoder(input, code);
	for (;;) {
		do {
			getSymbolScale(s);
			index = getCurrentIndex(s, low, high, code);
			c = convertSymbolToInt(index, s);
			removeSymbolFromStream(input, s, low, high, code);
		} while (c == ESCAPE);
		if (c == END_OF_STREAM)
			break;
		output.put(c);
		updateModel(c);
	}
}