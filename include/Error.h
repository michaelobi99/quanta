#pragma once
#include <string>
#include <string_view>

void usage() {
	printf("QUANTA 1.0: quanta compressed archive manager\n");
	printf("USAGE: quanta -[command] carfile [file...]");
	printf("\nx: [extract file from archive]");
	printf("\nr: [replace files in archive]");
	printf("\np: [print files in archive to screen]");
	printf("\nt: [test files in archive]");
	printf("\nl: [list files in archive]");
	printf("\na: [add file to archive(replace if present)]");
	printf("\nd: [delete file from archive]\n");
}

void fatalError(std::string const& errorMessage) {
	printf("ERROR: %s", errorMessage.c_str());
	exit(1);
}