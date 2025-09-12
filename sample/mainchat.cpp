/**
* Web Socket server
* จะทำหน้า echo msg ไปยังทุกคนที่อยู่ใน ห้อง
* for gaming
*
* 12/09/2025 tested
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
				std::string s = "numuser=" + std::to_string(room1->numuser) + "\n";
				s += "numreadset=" + std::to_string(room1->numreadset) + "\n";
				s += "numprocthread=" + std::to_string(room1->config->numprocthread);
				websocksettext(n, s.c_str());
				return WEBSOCKMSGCONTINUE;
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