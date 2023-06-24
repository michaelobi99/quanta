#pragma once
#include <string>
#include <ranges>
#include <execution>
#include <filesystem>
#include "huffman.h"

namespace fs = std::filesystem;

#define BLOCK_SIZE ((1 << 10) * 750)

//#define END_OF_BLOCK 255 //I assume that the 255th ASCII doesn't appear in the input text

struct suffix {
	char* ch{};
	int index{};
	int blockSize{};
};

struct suffixCompare {
	bool operator() (suffix const& s1, suffix const& s2) const {
		int blockSize = s1.blockSize;
		int i = s1.index;
		int j = s2.index;
		int res = strcmp_(s1.ch + i, s2.ch + j);
		while (--blockSize && res == 0) {
			if ((++i) == s1.blockSize)
				i = (i) % s1.blockSize;
			if ((++j) == s1.blockSize)
				j = (j) % s1.blockSize;
			res = strcmp_(s1.ch + i, s2.ch + j);
		}
		return res < 0;
	}
	inline int strcmp_(char* c1, char* c2) const {
		if (*c1 == *c2) return 0;
		if (*c1 < *c2) return -1;
		return 1;
	}
};

//**************************************************************************************************************************
//BW Transform
char* getLastChars(suffix* sortedChars, char* originalString, int& originalStringLocation, int length) {
	int len = length;
	char* bwtString = new char[len];
	for (int i{ 0 }; i < len; ++i) {
		int j = sortedChars[i].index;
		if (j == 0) {
			j += len;
			originalStringLocation = i;
		}
		bwtString[i] = originalString[j - 1];
	}
	return bwtString;
}

char* burrowsWheelerForwardTransform(char* inputString, int length, int& originalStringLocation) {
	suffix* vec = new suffix[length];
	for (int i{ 0 }; i < length; ++i) {
		vec[i].ch = inputString;
		vec[i].index = i;
		vec[i].blockSize = length;
	}
	std::sort(std::execution::par, vec, vec + length, suffixCompare());
	char* lastChars = getLastChars(vec, inputString, originalStringLocation, length);
	delete[] vec;
	return lastChars;
}


char* burrowsWheelerReverseTransform(char* bwtString, int length, int position) {
	struct charLocation {
		int indexInFirstCol{};
		int indexInLastCol{};
		char ch{};
		bool operator< (charLocation const& b) const {
			return ch < b.ch;
		};
	};
	char* originalString = new char[length];
	charLocation* firstColunmStr = new charLocation[length];
	for (int i = 0; i < length; ++i) {
		firstColunmStr[i].indexInLastCol = i;
		firstColunmStr[i].ch = bwtString[i];
	}
	std::stable_sort(std::execution::par, firstColunmStr, firstColunmStr + length);
	for (int i = 0; i < length; ++i) {
		firstColunmStr[i].indexInFirstCol = i;
	}
	int* tempArray = new int[length];
	for (int i = 0; i < length; ++i) {
		tempArray[firstColunmStr[i].indexInLastCol] = firstColunmStr[i].indexInFirstCol;
	}
	int n = length - 1, i = 0, T = position;
	originalString[n] = bwtString[T];
	for (i = 1; i < length; ++i) {
		originalString[--n] = bwtString[tempArray[T]];
		T = tempArray[T];
	}
	delete[] firstColunmStr;
	delete[] tempArray;
	return originalString;
}
//***************************************************************************************************************************



//***************************************************************************************************************************
//Move to front encoding
unsigned char* mtfEncode(char* bwtString, int length) {
	unsigned char alphabets[256];
	for (unsigned i = 0; i < 256; ++i)
		alphabets[i] = (unsigned char)i;
	auto s = sizeof(int) * 2;
	unsigned char* mtfString = new unsigned char[length + s];
	for (unsigned i{ 0 }; i < length; ++i) {
		for (unsigned j = 0; j < 256; ++j) {
			if (bwtString[i] == alphabets[j]) {
				mtfString[i + s] = (unsigned char)j;
				unsigned char temp = alphabets[j];
				memmove(alphabets + 1, alphabets, j);
				alphabets[0] = temp;
				break;
			}
		}
	}
	return mtfString;
}

//move to front decoding
char* mtfDecode(unsigned char* mtfString, int length) {
	char alphabets[256];
	for (unsigned i = 0; i < 256; ++i)
		alphabets[i] = (char)i;
	char* bwtString = new char[length];
	unsigned index;
	for (unsigned i{ 0 }; i < length; ++i) {
		index = (unsigned)mtfString[i];
		bwtString[i] = alphabets[index];
		memmove(alphabets + 1, alphabets, index);
		alphabets[0] = bwtString[i];
	}
	return bwtString;
}
//****************************************************************************************************************************




void BWCompress(std::fstream& input, std::unique_ptr<stl::BitFile>& output) {
	char* originalString = new char[BLOCK_SIZE]; //additional space for length and position
	int length{};
	int originalStringLocation{};
	int extraSpace = sizeof(int) * 2;
	do {
		input.read(originalString, BLOCK_SIZE);
		length = input.gcount();
		char* bwtString = burrowsWheelerForwardTransform(originalString, length, originalStringLocation);
		unsigned char* mtfString = mtfEncode(bwtString, length);
		delete[] bwtString;
		unsigned char* ptr = reinterpret_cast<unsigned char*>(&originalStringLocation);
		unsigned char* ptr1 = reinterpret_cast<unsigned char*>(&length);
		int i = 0;
		mtfString[i++] = ptr[0];
		mtfString[i++] = ptr[1];
		mtfString[i++] = ptr[2];
		mtfString[i++] = ptr[3];
		mtfString[i++] = ptr1[0];
		mtfString[i++] = ptr1[1];
		mtfString[i++] = ptr1[2];
		mtfString[i] = ptr1[3];
		huffCompress(mtfString, length + extraSpace, output);
		delete[] mtfString;
	} while (length == BLOCK_SIZE);
	delete[]originalString;
}

void BWExpand(std::unique_ptr<stl::BitFile>& input, std::fstream& output) {
	int extraSpace = sizeof(int) * 2;
	unsigned char* mtfString = new unsigned char[BLOCK_SIZE + extraSpace];
	int length{}; //block length
	int originalStringLocation{};
	do {
		huffExpand(input, mtfString);
		originalStringLocation = *(int*)(mtfString);
		length = *(int*)(mtfString + sizeof(int));
		char* bwtString = mtfDecode(mtfString + extraSpace, length);
		char* originalString = burrowsWheelerReverseTransform(bwtString, length, originalStringLocation);
		output.write(originalString, length);
		delete[]bwtString;
		delete[]originalString;
	} while (length == BLOCK_SIZE);
	delete[]mtfString;
}