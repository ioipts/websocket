/**
* Web Socket server
* จะทำหน้า echo msg ไปยังทุกคนที่อยู่ใน ห้อง
* for gaming
*/

#include "websockserver2.h"
#include <string>

WebSockServerNetwork* websock;
WebSockRoomProcThread room1;

int onnetwork(WebSockNetwork n,const char* msg,size_t len)
{
	switch (n->state)
	{
		case WEBSOCKSTATEINIT:
		{
	 	    //GET /echo HTTP/1.1
            printf("New connection\n");
            //websock->addToRoom(room1,n); // add new connection to room1
            return WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATECONTINUE: 
		{   // relay message to all
			websocksettext(n, (std::string("hello ")+msg).c_str()); 
			return WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATEDESTROY:
		{
			return WEBSOCKMSGEND;
		} break;
	}
}

int main(int argc, char** argv)
{
	printf("Theif server 1.0:\n");
	WebSockServerNetwork* n3 = new WebSockServerNetwork(38894);
	n3->setOnMsg(onnetwork);
    room1=n3->createRoom(); // create a room for gaming
	n3->begin();
	delete n3;
	return 0;
}