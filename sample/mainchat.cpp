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
		{	//GET /echo HTTP/1.1
            printf("New connection\n");
            websock->addToRoom(room1,n); // add new connection to room1
            return WEBSOCKMSGLEAVE;
		} break;
		case WEBSOCKSTATECONTINUE: 
		{   
			if (strnstr(msg, "exit", len) != NULL) {
				return WEBSOCKMSGEND;
			}
			if (strnstr(msg, "debug", len) != NULL) {
				printf("numuser=%d\n", room1->numuser);
				int p=room1->config->numprocthread;
				printf("numprocthread=%d\n", p);
				return WEBSOCKMSGEND;
			}
			// relay message to all
			for (int i=0;i<room1->numuser;i++)
			{
				if (room1->user[i]!=n) // not send to self
					websocksettext(room1->user[i], msg);
			}
			return WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATEDESTROY:
		{
            printf("destroy connection\n");
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