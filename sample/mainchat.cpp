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
			if ((len>=4) && (memcmp(msg, "exit", len) ==0)) {
				return WEBSOCKMSGEND;
			}
			if ((len>=5) && (memcmp(msg, "debug", len) == 0)) {
				printf("numuser=%d\n", room1->numuser);
				printf("numreadset=%d\n", room1->numreadset);
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
			//websocksettext(n, msg); // echo back to self
			return WEBSOCKMSGCONTINUE;
		} break;
		case WEBSOCKSTATEDESTROY:
		{
            printf("destroy connection\n");
			return WEBSOCKMSGEND;
		} break;
	}
	return WEBSOCKMSGEND;
}

int main(int argc, char** argv)
{
	printf("Web chat server 1.0:\n");
	WebSockServerNetwork* n3 = new WebSockServerNetwork(38894);
	n3->setOnMsg(onnetwork);
    room1=n3->createRoom(); // create a room for gaming
	n3->begin();
	delete n3;
	return 0;
}