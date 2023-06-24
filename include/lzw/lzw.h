#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>
#include "..\BitIO.h"

#define BITS 16
#define MAX_CODE ((1 << BITS) - 1)
#define TABLE_SIZE 78643L
#define TABLE_BANKS ((TABLE_SIZE >> 8) + 1)
#define END_OF_STREAM 256
#define BUMP_CODE 257
#define FLUSH_CODE 258
#define FIRST_CODE 259
#define UNUSED -1

struct Dictionary {
	int parentCode{};
	int codeValue{};
	char character{};
};

Dictionary* dict[TABLE_BANKS];

#define DICT(i) dict[i >> 8][i & 0xff]

char decodeStack[TABLE_SIZE]; //used during decoding to collect and decode strings
unsigned int nextCode{}; //next code to be added to the dictionary
int currentCodeBits{}; //defines how many bits are currently used for output
unsigned int nextBumpCode{}; //code that triggers the next jump in word size


//This routine allocates the dictionary. Since the total size of the dictionary is
//much larger than 64K, it can't be allocated as a single object. Instead, it is
//allocated as a set of pointers to smaller dictionary objects. The special DICT()
//macro is used to translate indices into pairs of references.
void initializeStorage() {
	for (int i{ 0 }; i < TABLE_BANKS; ++i) {
		dict[i] = new(std::nothrow) Dictionary[256];
		if (!dict[i])
			std::exit(1);
	}
}

void initializeDictionary() {
	for (std::uint32_t i{ 0 }; i < TABLE_SIZE; i++)
		DICT(i).codeValue = UNUSED;
	nextCode = FIRST_CODE;
	currentCodeBits = 9;
	nextBumpCode = 511;
}

unsigned int hashChildNode(int parentCode, int character) {
	unsigned int index{};
	int offset{};
	index = (character << (BITS - 8)) ^ parentCode;
	if (index == 0) offset = 1;
	else offset = TABLE_SIZE - index;
	for (;;) {
		if (DICT(index).codeValue == UNUSED)
			return index;
		if (DICT(index).parentCode == parentCode && DICT(index).character == (char)character)
			return index;
		if (index >= offset)
			index -= offset;
		else
			index += TABLE_SIZE - offset;
	}
}

void LZWCompress(std::fstream& input, std::unique_ptr<stl::BitFile>& output) {
	int character{}, stringCode{};
	unsigned int index{};
	initializeStorage();
	initializeDictionary();
	if ((stringCode = input.get()) == EOF)
		stringCode = END_OF_STREAM;
	while ((character = input.get()) != EOF) {
		index = hashChildNode(stringCode, character);
		if (DICT(index).codeValue != UNUSED)
			stringCode = DICT(index).codeValue;
		else {
			DICT(index).codeValue = nextCode++;
			DICT(index).parentCode = stringCode;
			DICT(index).character = (char)character;
			stl::outputBits(output, (std::uint32_t)stringCode, currentCodeBits);
			stringCode = character;
			if (nextCode > MAX_CODE) {
				stl::outputBits(output, (std::uint32_t)FLUSH_CODE, currentCodeBits);
				initializeDictionary();
			}
			else if (nextCode > nextBumpCode) {
				stl::outputBits(output, (std::uint32_t)BUMP_CODE, currentCodeBits);
				currentCodeBits++;
				nextBumpCode <<= 1;
				nextBumpCode |= 1;
			}
		}
	}
	stl::outputBits(output, (std::uint32_t)stringCode, currentCodeBits);
	stl::outputBits(output, (std::uint32_t)END_OF_STREAM, currentCodeBits);
}

unsigned int decodeString(unsigned int count, unsigned int code) {
	while (code > 255) {
		decodeStack[count++] = DICT(code).character;
		code = DICT(code).parentCode;
	}
	decodeStack[count++] = (char)code;
	return count;
}

void LZWExpand(std::unique_ptr<stl::BitFile>& input, std::fstream& output) {
	unsigned int newCode{}, oldCode{}, count{};
	int character;
	initializeStorage();
	for (;;) {
		initializeDictionary();
		oldCode = (unsigned int)stl::inputBits(input, currentCodeBits);
		if (oldCode == END_OF_STREAM)
			return;
		character = oldCode;
		output.put(oldCode);
		for (;;) {
			newCode = (unsigned int)stl::inputBits(input, currentCodeBits);
			if (newCode == END_OF_STREAM)
				return;
			if (newCode == FLUSH_CODE)
				break;
			if (newCode == BUMP_CODE) {
				currentCodeBits++;
				continue;
			}
			if (newCode >= nextCode) { //decoded an incomplete dictionary entry
				decodeStack[0] = (char)character;
				count = decodeString(1, oldCode);
			}
			else
				count = decodeString(0, newCode);
			character = decodeStack[--count]; //isolate the first character
			for (int i = count; i > -1; --i) {
				output.put(decodeStack[i]);
			}
			DICT(nextCode).parentCode = oldCode;
			DICT(nextCode).character = (char)character;
			nextCode++;
			oldCode = newCode;
		}
	}
}