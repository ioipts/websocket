/**
* Web Socket server
* for streaming chat
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
		}
		case WEBSOCKSTATECONTINUE: 
		{
			if (len > 0) {
				printf("received: %s\n", msg);
				websocksettext(n, (std::string("hello ")+msg).c_str());
			}
			return (strcmp(msg, "bye") == 0)?WEBSOCKMSGEND:WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATEDESTROY:
		{
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
