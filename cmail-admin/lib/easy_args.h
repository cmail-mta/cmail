#pragma once


typedef struct {
	char* command;
	char** arguments;
} Arguments;

int args_addArgument(char* argShort, char* argLong, void* func, unsigned arguments);
int args_parse(int argc, char** argv, char** output);
