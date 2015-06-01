#include "getchar.h"

int ask_password(char* buffer, int maxlength){
	int i=0,c;

	if(!buffer||maxlength<1){
		return -1;
	}

	do{
		c=getch();
		switch(c){
			case 3:
				//sigint during input
				return -1;
			case 8:
				i-=2;
				if(i<0){
					i=0;
				}
				break;
			case '\n':
			case '\r':
			case EOF:
				buffer[i]=0;
				return i;
			default:
				buffer[i]=c;
				break;
		}
		i++;
	}
	while(i<maxlength);
	return -1;
}