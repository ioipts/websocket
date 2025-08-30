/**
* Web Socket server
* for streaming chat
* เพียงแค่ ถามแล้วก็ตอบออกไปเท่านั้น
* 
* 25/06/2023 tested
*/

#include "websockserver2.h"
#include <string>

WebSockServerNetwork* websock;

int onnetwork(WebSockNetwork n,const char* msg,size_t len)
{
	switch (n->state)
	{
		case WEBSOCKSTATEINIT:
		{
			printf("New connection\n");
			return WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATECONTINUE: 
		{
			if (len > 0) {
				//ตอบออกไปด้วย hello
				websocksettext(n, (std::string("hello ")+msg).c_str()); 
			}
			return (strcmp(msg, "bye") == 0)?WEBSOCKMSGEND:WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATEDESTROY:
		{
			printf("Connection closed\n");
			return WEBSOCKMSGEND;
		} break;
	}
}

int main(int argc, char** argv)
{
	printf("Web Socket server 1.0:\n");
	WebSockServerNetwork* n3 = new WebSockServerNetwork(38894);
	n3->setOnMsg(onnetwork);
	n3->begin();
	delete n3;
	return 0;
}
