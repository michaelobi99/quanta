#pragma once
#include <string>
#include <iomanip>

void usage() {
	printf("\nQUANTA 1.0: quanta compressed archive manager\n");
	printf("USAGE: quanta -[command] [archive file] [files...]\n");
	printf("\nx: [extract file from archive]");
	printf("\nr: [replace files in archive]");
	printf("\np: [print files in archive to screen]");
	printf("\nt: [test files in archive]");
	printf("\nl: [list files in archive]");
	printf("\na: [add file to archive(replace if present)]");
	printf("\nd: [delete file from archive]\n");
	exit(0);
}

void fatalError(std::string const& errorMessage) {
	printf("ERROR: %s", errorMessage.c_str());
	exit(1);
}