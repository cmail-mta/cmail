#ifndef _WIN32
	#include <termios.h>
	#include <unistd.h>
	#include <stdio.h>

	//this emulates conio.h's getch().
	//via http://cboard.cprogramming.com/faq-board/27714-faq-there-getch-conio-equivalent-linux-unix.html
	
	int _getch(void){
		struct termios canon,raw;
		int ch;
		tcgetattr(STDIN_FILENO,&canon);
		raw=canon;
		raw.c_lflag&=~(ICANON|ECHO);
		tcsetattr(STDIN_FILENO,TCSANOW,&raw);
		ch=getchar();
		tcsetattr(STDIN_FILENO,TCSANOW,&canon);
		return ch;
	}

	#define getch _getch
#else
	#include <conio.h>
#endif
