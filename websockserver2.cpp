#include "websockserver2.h"

//both receiving and sending buffer
unsigned int WebSockServerNetwork::MSGSIZE = 100000;
unsigned int WebSockServerNetwork::MAXLISTEN = 128;
unsigned int WebSockServerNetwork::PINGTIMEOUT = 20000;
unsigned int WebSockServerNetwork::MAXCONNECTION = 2000;
unsigned int WebSockServerNetwork::MAXBUFFER = 1000000;

#define TCPREADSETSIZE 64		


void sha1(const void* src, size_t bytelength, unsigned char* hash);
void base64_encode(const unsigned char* data, size_t input_length, char* encoded_data); 
void tcpunsetsocket(WebSockRoomProcThread c,WebSockNetwork n);
void tcpsetsocket(WebSockRoomProcThread c,int index);

SOCKET initwebsockserver(int port, unsigned int maxbuffer, unsigned int maxlisten)
{
	struct sockaddr_in local;
	SOCKET sock;

#if defined(_MSC_VER) || defined(__MINGW32__)
	WSADATA wsaData;
	WSAStartup(0x101, &wsaData);
#endif
	//Now we populate the sockaddr_in structure
	local.sin_family = AF_INET; //Address family
	local.sin_addr.s_addr = INADDR_ANY; //Wild card IP address
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons((u_short)port); //port to use	
	//the socket function creates our SOCKET
	sock = socket(AF_INET, SOCK_STREAM, 0);
	//If the socket() function fails we exit
#if defined(_MSC_VER)    
	if (sock == INVALID_SOCKET) return INVALID_SOCKET;
#else
	if (sock < 0) return -1;
#endif
	int enable = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int));
#if defined(__APPLE__)
	setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&enable, sizeof(int));
#endif
	int msgSize = maxbuffer;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&msgSize, sizeof(msgSize));
	msgSize = maxbuffer;
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&msgSize, sizeof(msgSize));
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) != 0) return -1;
	listen(sock, maxlisten);			//max connection request at the same time
	return sock;
}

bool initwebsockconfig(WebSockNetworkConfig c, int port, int numproc, unsigned int maxbuffer, unsigned int maxlisten)
{
	c->numprocthread = numproc;
	SOCKET server = initwebsockserver(port, maxbuffer, maxlisten);
	if (server <= 0) return false;
	WebSockListenThread t = c->listenthread = (WebSockListenThread)ALLOCMEM(sizeof(struct WebSockListenThreadS));
	if (t == NULL) return false;
	t->port = port;
	t->server = server;
	t->config = c;
	t->lasttime = time(NULL);
	c->pingtimeout = WebSockServerNetwork::PINGTIMEOUT;
	c->maxconnection = numproc;
	c->numprocthread = 0;
#if defined(_PTHREAD)
	pthread_mutex_init(&c->mutex, NULL);
#endif
	return true;
}

void destroywebsockconfig(WebSockNetworkConfig c)
{
	WebSockListenThread t = c->listenthread;
	CLOSESOCKET(c->listenthread->server);
	FREEMEM(t);
#if defined(_PTHREAD)
	pthread_mutex_destroy(&c->mutex);
#endif
	FREEMEM(c);
}

WebSockNetwork initwebsocknetwork(SOCKET csocket, int buffersize)
{
	WebSockNetwork n = (WebSockNetwork)ALLOCMEM(sizeof(struct WebSockNetworkS));
	if (n == NULL) return NULL;
	n->socket = csocket;
	n->handshake = false;
	n->lastping = time(NULL);
	n->buffer = (char*)ALLOCMEM(buffersize+1024);
	n->bufferIndex = 0;
	n->bufferSize = buffersize;
	n->sendmsg = (char*)ALLOCMEM(buffersize+1024);
	n->sendIndex = 0;
	n->sendSize = buffersize;
	n->state = WEBSOCKSTATEINIT;
	n->data = NULL;
	if ((n->buffer == NULL) || (n->sendmsg == NULL))
	{
		FREEMEM(n);
		n = NULL;
	}
	return n;
}

void destroywebsocknetwork(WebSockNetwork n)
{
	CLOSESOCKET(n->socket);
	FREEMEM(n->buffer);
	FREEMEM(n->sendmsg);
	FREEMEM(n);
}

WebSockProcThread initwebsockproc(WebSockNetwork n, WebSockNetworkConfig config)
{
	WebSockProcThread p = (WebSockProcThread)ALLOCMEM(sizeof(struct WebSockProcThreadS));
	if (p == NULL) return NULL;
	p->config = config;
	p->n = n;
	return p;
}

void destroywebsockproc(WebSockProcThread p)
{
	#if defined(_PTHREAD)
	pthread_mutex_lock(&config->mutex);
#endif
	p->config->numprocthread--;
#if defined(_PTHREAD)
	pthread_mutex_unlock(&config->mutex);
#endif
	FREEMEM(p);
}

WebSockRoomProcThread initwebsockroomproc(WebSockNetworkConfig config) {
	WebSockRoomProcThread c=(WebSockRoomProcThread)ALLOCMEM(sizeof(WebSockRoomProcThreadS));
	if (c == NULL) return NULL;
	c->config = config;
	c->exitflag = false;
#if defined(_PTHREAD)
	pthread_init_mutex(&c->mutex);
#else
	c->mutex = new WebSockMutex();
#endif
	c->tcpreadset=(fd_set**)ALLOCMEM(256*sizeof(fd_set*));
	c->tcpreadsocks=(SOCKET*)ALLOCMEM(256*sizeof(SOCKET));
	c->sizereadset=256;
	c->numreadset=0;
	c->user=(WebSockNetwork*)ALLOCMEM(256*sizeof(WebSockNetwork));
	//c->leaveuser=(WebSockNetwork*)ALLOCMEM(256*sizeof(WebSockNetwork));
	c->enduser=(WebSockNetwork*)ALLOCMEM(256*sizeof(WebSockNetwork));
	c->sizeuser=256;
	c->numuser=0;
	c->numenduser=0;
	//c->numleaveuser=0;
	c->add=NULL;
	c->remove=NULL;
	c->pingturnid=0;
	return c;
}

void destroywebsockroomproc(WebSockRoomProcThread c)
{
	for (int i=0;i<c->numreadset;i++)
	{
		FREEMEM(c->tcpreadset[i]);
	}
#if defined(_PTHREAD)
	pthread_mutex_destroy(&c->mutex);
#else
	delete c->mutex;
#endif
	FREEMEM(c->tcpreadset);
	FREEMEM(c->tcpreadsocks);
	for (int i=0;i<c->numuser;i++)
	{
		destroywebsocknetwork(c->user[i]);
	}
	FREEMEM(c->user);
	FREEMEM(c->enduser);
	//FREEMEM(c->leaveuser);
	FREEMEM(c);
}

void websockshiftbuffer(WebSockNetwork n, size_t len)
{
	size_t v = n->bufferIndex - len;
	if (len >= v)
	{
		CPYMEM(n->buffer, &n->buffer[len], v);
	}
	else {			//Double copy
		CPYMEM(n->buffer, &n->buffer[len], len);
		CPYMEM(&n->buffer[len], &n->buffer[len * 2], v - len);
	}
	n->bufferIndex -= len;
	n->buffer[n->bufferIndex] = 0;
}

/**
* check required buffer len
* @return size required in bytes
*/
int websockencodelen(size_t blen)
{
	int k = 0;
	if (blen <= 125) {
		k = 2;
	}
	else if (blen >= 126 && blen <= 65535) {
		k = 4;
	}
	else {
		k = 10;
	}
	return k + blen;
}

/**
* insert data
*/
int websockencode(const char* bytesRaw,unsigned int blen,unsigned char* bytesFormatted)
{ 
    bytesFormatted[0] = 129;		//begin with 129
	int k=0;
    if ( blen<= 125) {
        bytesFormatted[1] = blen;
		k=2;
    } else if (blen >= 126 && blen <= 65535) {
        bytesFormatted[1] = 126;
        bytesFormatted[2] = ( blen >> 8 ) & 255;
        bytesFormatted[3] = ( blen      ) & 255;
		k=4;
    } else {
        bytesFormatted[1] = 127;
        bytesFormatted[2] = 0; //( blen >> 56 ) & 255; too long
        bytesFormatted[3] = 0; //( blen >> 48 ) & 255;
        bytesFormatted[4] = 0; //( blen >> 40 ) & 255;
        bytesFormatted[5] = 0; //( blen >> 32 ) & 255;
        bytesFormatted[6] = ( blen >> 24 ) & 255;
        bytesFormatted[7] = ( blen >> 16 ) & 255;
        bytesFormatted[8] = ( blen >>  8 ) & 255;
        bytesFormatted[9] = ( blen       ) & 255;
		k=10;
    }
    for (unsigned int i = 0; i < blen; i++){
        bytesFormatted[k]=bytesRaw[i];
		k++;
    }
	return k;
}

/**
* check if the message is available to decode
* @return -1 error, 0 not available
*/
int websockdecodelen(unsigned char* data, unsigned int length)
{	//required at lease 2 bytes
	if (length < 2) return 0;
	if (data[0] != 129) return -1;
	unsigned char datalength = data[1] & 127;
	if (datalength <= 125) {
		unsigned int blen = datalength;
		if (blen + 2 + 4 > length) return 0;
		return blen;
	} else if (datalength == 126) {
		if (length < 4) return 0;
		unsigned int blen = ((unsigned int)(data[2]) << 8) | data[3];
		if (blen + 4 + 4 > length) return 0;
		return blen;
	} else if (datalength == 127) {
		if (length < 10) return 0;
		unsigned int blen = ((unsigned int)(data[6])<<24) | ((unsigned int)(data[7])<<16)| ((unsigned int)(data[8]) << 8) | data[9];
		if (blen + 10 + 4> length) return 0;
		return blen;
	}
	return -1;
}

/**
* decode the message
* @return how many bytes of data is used
*/
int websockdecode(char* data,int length,char* output) {
    unsigned char datalength = data[1] & 127;
    unsigned char indexFirstMask = 2;
	size_t blen = 0;
    if (datalength == 126) { indexFirstMask = 4;
		blen = ((unsigned int)data[2] << 8) | data[3];
	} else if (datalength == 127) { indexFirstMask = 10;
		blen = ((unsigned int)(data[6]) << 24) | ((unsigned int)(data[7]) << 16) | ((unsigned int)(data[8]) << 8) | data[9];
	} else {
		blen = datalength;
	}
    unsigned char *masks = (unsigned char*)&data[indexFirstMask]; //,indexFirstMask + 4);
    unsigned int i = indexFirstMask + 4;
    unsigned int index = 0;
	unsigned int end = indexFirstMask + 4 + blen;
    int k=0;
	while (k < blen) {
        output[k] = (data[i++] ^ masks[index++ % 4]);
		k++;
    }
	output[k]=0;
	return end;
}

/**
* handshake for read & proc thread 
* @return 0=nohandshake, 1=complete
*/
bool websockhandshake(WebSockNetworkConfig config, WebSockNetwork n,int* ret)
{
	char* key = strstr(n->buffer, "Sec-WebSocket-Key: ");
	char* endheader = strstr(n->buffer, "\r\n\r\n");
	if ((key != NULL) && (endheader!=NULL))
	{
		char* nkey = key; nkey += 19;
		n->sendIndex=0;
		if ((strlen(nkey)<25) || (nkey[24] != '\r') || (nkey[25]!='\n'))return false;
		CPYMEM(n->sendmsg, (unsigned char*)nkey,24);	//always 24 characters
		n->sendmsg[24] = 0;
		strcpy(n->key, n->sendmsg);
		CPYMEM(&n->sendmsg[24], "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
		n->sendIndex = 24+36;
		unsigned char d[20];
		sha1(n->sendmsg, n->sendIndex, d);
		unsigned char encoded_data[40];
		base64_encode(d, 20, (char*)&encoded_data);
		sprintf(n->sendmsg, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", (const char*)encoded_data);
		n->sendIndex = strlen(n->sendmsg);
		n->handshake = true;
		size_t len = endheader - n->buffer;
		*ret=config->msgFunc(n, n->buffer, len); 
		n->state = WEBSOCKSTATECONTINUE;
		websockshiftbuffer(n, len+4);
		return true;
	}
	return false;
}

int websockdefaultmsg(WebSockNetwork n,const char* data,size_t len)
{
	return WEBSOCKMSGEND;
}

/**
* for both listing and process thread 
* @return -1 if timeout
*/
int websocksend(WebSockNetwork n, unsigned int pingtimeout)
{
	int r = 0;
	TIME now = time(NULL);
	//no 'dontwait' will block if connection lost 
	int rets = send(n->socket, n->sendmsg, n->sendIndex, MSG_NOSIGNAL | MSG_DONTWAIT); 
	if (rets <= 0)
	{
		if ((unsigned int)(time(NULL)-n->lastping) < pingtimeout) {
			r = WEBSOCKMSGCONTINUE;
		}
		else {
			n->sendIndex = 0;		//freeze
			r = -1;
		}
	}
	else {
		n->lastping = now;
		if (rets < n->sendIndex) {
			size_t v = n->sendIndex - rets;
			if (rets >= v)
			{
				CPYMEM(n->sendmsg, &n->sendmsg[rets], v);
			}
			else {			//Double copy
				CPYMEM(n->sendmsg, &n->sendmsg[rets], rets);
				CPYMEM(&n->sendmsg[rets], &n->sendmsg[rets * 2], v - rets);
			}
			n->sendIndex -= rets;
			r = WEBSOCKMSGCONTINUE;
		}
		else {
			n->sendIndex = 0;
			r = WEBSOCKMSGEND;
		}
	}
	return r;
}

void endwebsockproc(WebSockProcThread p,bool destroynetwork)
{
	WebSockNetworkConfig config = p->config;
	if (destroynetwork) { 
		WebSockNetwork n = p->n;
		n->state = WEBSOCKSTATEDESTROY;
		config->msgFunc(n,NULL,-1);
		destroywebsocknetwork(p->n); 
	}
	destroywebsockproc(p);
}

void endwebsockroomproc(WebSockRoomProcThread p,WebSockNetwork n)
{
	WebSockNetworkConfig config = p->config;
	n->state = WEBSOCKSTATEDESTROY;
	config->msgFunc(n,NULL,-1);
	destroywebsocknetwork(n);
}

void websockping(WebSockRoomProcThread c)
{
	if (c->numuser==0) return ;
	  WebSockNetwork n=c->user[c->pingturnid%c->numuser];
	  c->pingturnid=(c->pingturnid+1)%c->numuser;
	  if (time(NULL)-n->lastping>c->config->pingtimeout)
	  {
#if defined(_DEBUGNETWORK) 
		printf("ping disconnect\n");
#endif 	     
		destroywebsocknetwork(n);
	  }
}

/**
 * single process thread for each connection
 */
void* websocksingleprocthread(void* arg)
{
	WebSockProcThread c = (WebSockProcThread)arg;
	WebSockNetworkConfig config = c->config;
	WebSockMsgFunc* msgfunc = config->msgFunc;
	WebSockNetwork n=c->n;
	int retval, readsocks, r;
	fd_set socks;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	r = WEBSOCKMSGCONTINUE;
	while ((r != WEBSOCKMSGEND) && (r!=WEBSOCKMSGLEAVE) && (!config->exitflag) && (time(NULL) - n->lastping < config->pingtimeout)) {
		FD_ZERO(&socks);
		FD_SET(n->socket, &socks);
		readsocks = select(FD_SETSIZE, &socks, (fd_set*)0, (fd_set*)0, &timeout);
		retval = (readsocks > 0) ? recv(n->socket, &n->buffer[n->bufferIndex], n->bufferSize - n->bufferIndex, 0) : 0;
		if (retval >= 0)		//in case retval==0 or -1
		{
			if (retval > 0) {
				n->bufferIndex += retval;
				n->buffer[n->bufferIndex] = 0;
				n->lastping = time(NULL);
			}
			if (!n->handshake) {
				int ret=0;
				if (websockhandshake(config, n,&ret))
				{
					websocksend(n, config->pingtimeout);
					r=ret;
				}
			}
			else {
				int decodelen = websockdecodelen((unsigned char*)n->buffer, n->bufferIndex);
				while (decodelen > 0) {
					char* output = (char*)ALLOCMEM(decodelen+1);
					if (output != NULL) {
						int dlen = websockdecode(n->buffer, n->bufferIndex, output);
						websockshiftbuffer(n, dlen);
						output[decodelen] = 0;		//end message for text based
						r = msgfunc(n, output, decodelen);
						FREEMEM(output);
						n->state = WEBSOCKSTATECONTINUE;
					}
					else r = WEBSOCKMSGEND;
					if (n->sendIndex > 0)
					{
						if (websocksend(n, config->pingtimeout) == -1)
						{
							endwebsockproc(c,true);
							return NULL;
						}
					}
					decodelen = websockdecodelen((unsigned char*)n->buffer, n->bufferIndex);
				}
				if (decodelen < 0) r = WEBSOCKMSGEND;
				else r = msgfunc(n, "", 0);
			}
		}
		else if (retval < 0)
		{
			endwebsockproc(c,true);
			return NULL;
		}
	}
	endwebsockproc(c,(r!=WEBSOCKMSGLEAVE));
	return NULL;
}

/**
 * Room process thread for multiple connection
 */
void* websockroomprocthread(void* arg)
{
	WebSockRoomProcThread c = (WebSockRoomProcThread)arg;
	WebSockNetworkConfig config = c->config;
	WebSockNetwork n;
	WebSockMsgFunc* msgfunc = config->msgFunc;
	int retval, readsocks, size;
	fd_set socks;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (!c->exitflag) {
	  //จัดการ network เข้าออกก่อน
#if defined(_PTHREAD)
		pthread_mutex_lock(&c->mutex);
#else
	c->mutex->mutex.lock();
#endif
	if (c->add!=NULL) {
		if (c->numuser>=c->sizeuser) { //need to expand
			int newsize=c->sizeuser+256;
			WebSockNetwork* u=(WebSockNetwork*)ALLOCMEM(newsize*sizeof(WebSockNetwork));
			WebSockNetwork* l=(WebSockNetwork*)ALLOCMEM(newsize*sizeof(WebSockNetwork));
			WebSockNetwork* e=(WebSockNetwork*)ALLOCMEM(newsize*sizeof(WebSockNetwork));
			CPYMEM(u,c->user,c->numuser*sizeof(WebSockNetwork));
			CPYMEM(e,c->enduser,c->numuser*sizeof(WebSockNetwork));
			FREEMEM(c->user);
			FREEMEM(c->enduser);
			c->user=u;
			c->enduser=e;
			c->sizeuser=newsize;
		}
		c->user[c->numuser]=c->add;
		c->numuser++;
		tcpsetsocket(c,c->numuser-1);
		printf("add\n");
		c->add=NULL;
	}
	if (c->remove!= NULL) {
		tcpunsetsocket(c,c->remove);
		c->remove = NULL;
	}
#if defined(_PTHREAD)
		pthread_mutex_unlock(&c->mutex);
#else
		c->mutex->mutex.unlock();
#endif	  
	  int s=(long)ceil((float)c->numuser/(float)TCPREADSETSIZE);
	  for (int i=0;i<s;i++)						//loop through all bucket (64 user/bucket)
	  {
       CPYMEM(&socks, (fd_set*)c->tcpreadset[i], sizeof(fd_set));
	   readsocks=select(c->tcpreadsocks[i]+1, &socks, (fd_set *) 0, (fd_set *) 0, &timeout); 
       if (readsocks>0)													 //check if any socks ready
       {
	   	size=(i+1)*TCPREADSETSIZE;
	    size=((int)c->numuser<size)?(int)c->numuser:size;
	    for (int j=i*TCPREADSETSIZE;j<size;j++) //loop all fd in the bucket
	    {
	    n=c->user[j];
		if (FD_ISSET(n->socket,&socks))	
		{ //process
			retval = (readsocks > 0) ? recv(n->socket, &n->buffer[n->bufferIndex], n->bufferSize - n->bufferIndex, 0) : 0;
			if (retval >= 0)		//in case retval==0 or -1
			{
				if (retval > 0) {
					n->bufferIndex += retval;
					n->buffer[n->bufferIndex] = 0;
					n->lastping = time(NULL);
				}
				int decodelen = websockdecodelen((unsigned char*)n->buffer, n->bufferIndex);
				while (decodelen > 0) {
					char* output = (char*)ALLOCMEM(decodelen+1);
					if (output != NULL) {
						int dlen = websockdecode(n->buffer, n->bufferIndex, output);
						websockshiftbuffer(n, dlen);
						output[decodelen] = 0;		//end message for text based
						int r = msgfunc(n, output, decodelen);
						FREEMEM(output);
						n->state = WEBSOCKSTATECONTINUE;
						if (r==WEBSOCKMSGEND) {c->enduser[c->numenduser]=n; c->numenduser++; }
					}
					decodelen = websockdecodelen((unsigned char*)n->buffer, n->bufferIndex);
				}
				if (decodelen < 0) { c->enduser[c->numenduser]=n; c->numenduser++; }
			}
			else if (retval < 0)
			{
				c->enduser[c->numenduser]=n;
				c->numenduser++;
			}
		}														//FD_ISSET
	   }													    //loop all fd in the bucket
	  }															//any ready?
	  }															//loop through all bucket
	  //*************************** write ************************
	  for (int i=0;i<c->numuser;i++)
	  {
		n=c->user[i];
		if (n->sendIndex > 0)
		{
			if (websocksend(n, config->pingtimeout) == -1)
			{
				c->enduser[c->numenduser]=n; 
				c->numenduser++;
			}
		} 
	  }
	  //************************** end user ***********************
	  for (int i=0;i<c->numenduser;i++)
	  {
		  tcpunsetsocket(c,c->enduser[i]);
		  endwebsockroomproc(c,c->enduser[i]);
	  }
	  c->numenduser=0;
	  //************************** ping ***********************
	  websockping(c);
	}
	destroywebsockroomproc(c);
	return NULL;
}

void* websocklistenthread(void* arg)
{
	WebSockListenThread c = (WebSockListenThread)arg;
	WebSockNetworkConfig config = c->config;
	SOCKET server=c->server;
	int msgsize = c->config->msgsize;
	WebSockNetwork n;
	SOCKET csocket;
	int readsocks;	
	fd_set socks;
	struct timeval timeout;
	struct sockaddr client;
	int size=sizeof(struct sockaddr);
	timeout.tv_sec=0;                    
    timeout.tv_usec=0;					

	c->lasttime = time(NULL);
	//****************** accept *****************************
	while (!config->exitflag) {
		//get current tick for process
		TIME now = time(NULL);
		unsigned int d = (unsigned int)(now - c->lasttime);
		if (d > c->tick) c->tick = d;
		c->lasttime = now;
		FD_ZERO(&socks);
		FD_SET(server, &socks);
		readsocks=select((int)server+1, &socks, (fd_set *) 0, (fd_set *) 0, &timeout);	  //select the highest socket 
		if (readsocks > 0)
		{
			size = sizeof(struct sockaddr);
#if defined(_MSC_VER) || defined(__MINGW32__)
			csocket = accept(server, (struct sockaddr*)&client, &size);
#else
			csocket = accept(server, (struct sockaddr*)&client, (socklen_t*)&size);
#endif
			if (csocket != -1) {				//2019/10/11 bug in ARM
				n = initwebsocknetwork(csocket, config->msgsize);		
				if (n != NULL) {
					n->addr = client;
#if defined(_PTHREAD)
					pthread_mutex_lock(&config->mutex);
#endif
					if (config->numprocthread >= config->maxconnection) {
						destroywebsocknetwork(n);
					}
					else {
						config->numprocthread++;
#if defined(_PTHREAD)
						pthread_t workerThreadId;
						pthread_attr_t tattr;
						sched_param schedparam;
						pthread_attr_init(&tattr);
						pthread_attr_getschedparam(&tattr, &schedparam);
						schedparam.sched_priority = 99;
						pthread_attr_setschedparam(&tattr, &schedparam);
						HTTPProcThread p = inithttpproc(n, config);
						int rc = pthread_create(&workerThreadId, &tattr, websockprocthread, (void*)p);
						if (rc != 0) {
							config->numprocthread--;
							destroywebsockproc(p);
						}
						else {
							pthread_detach(workerThreadId);
						}
#else
						WebSockProcThread p = initwebsockproc(n, config);
						std::thread proc(websocksingleprocthread, (void*)p);
						proc.detach();
#endif
					}
#if defined(_PTHREAD)
					pthread_mutex_unlock(&config->mutex);
#endif
				}
			}
		}
	}
	return NULL;
}


WebSockServerNetwork::WebSockServerNetwork(int port)
{
	config = (WebSockNetworkConfig)ALLOCMEM(sizeof(WebSockNetworkConfigS));
	if (config == NULL) return;
	config->msgFunc = websockdefaultmsg;
	config->exitflag = false;
	config->maxconnection = MAXCONNECTION;
	config->pingtimeout = PINGTIMEOUT;
	config->msgsize = MSGSIZE;
	if (!initwebsockconfig(config, port, MAXCONNECTION, MAXBUFFER, MAXLISTEN)) {
		FREEMEM(config);
		config = NULL;
	}
}

void WebSockServerNetwork::setOnMsg(WebSockMsgFunc* p)
{
	config->msgFunc = p;
}

void WebSockServerNetwork::begin()
{
	if (config == NULL) return;
#if defined(_PTHREAD)
	pthread_t workerThreadId;
	int rc = pthread_create(&workerThreadId, NULL, httplistenthread, (void*)config->listenthread);
	if (rc != 0) return;
	(void)pthread_join(workerThreadId, NULL);
#else
	std::thread workerThread(websocklistenthread, (void*)config->listenthread);
	workerThread.join();
#endif
	config->listenthread->config = NULL;
}

void WebSockServerNetwork::exit()
{
	if (config == NULL) return;
	config->exitflag = true;
	//หลังจากนี้ต้องรอให้ thread ปิดให้หมดก่อน
}

WebSockServerNetwork::~WebSockServerNetwork()
{
	if (config == NULL) return;
	destroywebsockconfig(config);
}

WebSockRoomProcThread WebSockServerNetwork::createRoom()
{
	WebSockRoomProcThread c = initwebsockroomproc(config);
#if defined(_PTHREAD)
		pthread_t workerThreadId;
		pthread_attr_t tattr;
		sched_param schedparam;
		pthread_attr_init(&tattr);
		pthread_attr_getschedparam(&tattr, &schedparam);
		schedparam.sched_priority = 99;
		pthread_attr_setschedparam(&tattr, &schedparam);
		int rc = pthread_create(&workerThreadId, &tattr, websockroomprocthread, (void*)p);
		if (rc != 0) {
			config->numprocthread--;
			destroywebsockproc(p);
		}
		else {
			pthread_detach(workerThreadId);
		}
#else
		std::thread proc(websockroomprocthread, (void*)c);
		proc.detach();
#endif
	return c;
}

void WebSockServerNetwork::destroyRoom(WebSockRoomProcThread c)
{
	destroywebsockroomproc(c);
}

void WebSockServerNetwork::addToRoom(WebSockRoomProcThread c, WebSockNetwork n)
{
	bool added=false;
	while (!added) {
#if defined(_PTHREAD)
	pthread_mutex_lock(&c->mutex);
#else
	c->mutex->mutex.lock();
#endif
	if (c->add== NULL) {
		c->add = n;
		added=true;
	}
#if defined(_PTHREAD)
	pthread_mutex_unlock(&p->mutex);
#else
	c->mutex->mutex.unlock();
#endif
	}
	//รอจนกระทั้ง c->add!=NULL
	added=false;
	while (!added) {
#if defined(_PTHREAD)
	pthread_mutex_lock(&c->mutex);	
#else
	c->mutex->mutex.lock();
#endif
	if (c->add==NULL || c->add!=n) added=true;
#if defined(_PTHREAD)
	pthread_mutex_unlock(&c->mutex);	
#else
	c->mutex->mutex.unlock();
#endif
	}
}

void WebSockServerNetwork::removeFromRoom(WebSockRoomProcThread c, WebSockNetwork n)
{
	bool removed=false;
	while (!removed) {
#if defined(_PTHREAD)
	pthread_mutex_lock(&c->mutex);
#else
	c->mutex->mutex.lock();
#endif
	if (c->remove== NULL) {
		c->remove = n;
		removed=true;
	}
#if defined(_PTHREAD)
	pthread_mutex_unlock(&c->mutex);
#else
    c->mutex->mutex.unlock();
#endif
	}
	//รอจนกระทั้ง c->remove!=NULL
	removed=false;
	while (!removed) {
#if defined(_PTHREAD)
	pthread_mutex_lock(&c->mutex);
#else
    c->mutex->mutex.lock();
#endif
	if (c->remove==NULL || c->remove!=n) removed=true;
#if defined(_PTHREAD)
	pthread_mutex_unlock(&c->mutex);
#else
	c->mutex->mutex.unlock();
#endif
	}
}

/**  
* O(1024)
* reset the maxsocket and socks
* create fd_set and maxsock for each 64 elements
*/  
void tcpsetsocket(WebSockRoomProcThread c,int index)
{
	int size,i;
	fd_set *socks;
	int s=index/TCPREADSETSIZE;
	int f=s*TCPREADSETSIZE;	//first
	int l=f+TCPREADSETSIZE;	//last
	SOCKET maxsock;

	if (c->numreadset<=s)
	{
		socks=(fd_set*)ALLOCMEM(sizeof(fd_set));
		if (c->sizereadset<=c->numreadset){
			//expand
			int numsize=c->sizereadset+256;
			fd_set** tmp = (fd_set**)ALLOCMEM(numsize*sizeof(fd_set*));
			CPYMEM(tmp, c->tcpreadset, c->sizereadset*sizeof(fd_set*));
			FREEMEM(c->tcpreadset);
			c->tcpreadset=tmp;

			SOCKET* tmpsock = (SOCKET*)ALLOCMEM(numsize*sizeof(SOCKET));
			CPYMEM(tmpsock, c->tcpreadsocks, c->sizereadset*sizeof(SOCKET));
			FREEMEM(c->tcpreadsocks);
			c->tcpreadsocks=tmpsock;

			c->sizereadset=numsize;
		}
		c->tcpreadset[c->numreadset]=socks;
		c->tcpreadsocks[c->numreadset]=0;
		c->numreadset++;
	} else
	{  
		socks=(fd_set*)c->tcpreadset[s];
	}

	FD_ZERO(socks);
	size=(int)c->numuser; 			
	if (l>size) l=size;				//last
	maxsock=0;
	for (i=f;i<l;i++)
	{
		WebSockNetwork n=c->user[i];
		FD_SET(n->socket, socks);
		if ((unsigned int)n->socket>(unsigned int)maxsock) maxsock=n->socket;
	}
	c->tcpreadsocks[s]=maxsock;
}

/**
* O(1024)
* not destroy the network just remove from the list and recalculate the FD_SET
*/
void tcpunsetsocket(WebSockRoomProcThread c, WebSockNetwork n)
{
 WebSockNetwork* v=c->user;
 int i,size=(int)c->numuser;
 int found=0;
 int index=-1;

 for (i=0;(i<size) && (found==0);i++)
 {
  if (v[i]==n) 
  {
   v[i]=v[size-1];
   c->numuser--;
   index=i;
   found=1;
  }
 }
 if (found)  //recalculate fd_set
 {
  c->numreadset=ceil(c->numuser/TCPREADSETSIZE);
  tcpsetsocket(c,c->numuser-1);
  tcpsetsocket(c,index);
 }
}

bool websocksettext(WebSockNetwork n, const char* msg)
{
	return websocksetcontent(n,msg,strlen(msg));
}

bool websocksetcontent(WebSockNetwork n, const char* content,size_t size)
{
	int s = websockencodelen(size);
	if (n->sendSize - n->sendIndex < s) return false;
	int len = websockencode(content,size, (unsigned char*)n->sendmsg);
	n->sendIndex += len;
	return true;
}

void websockset(WebSockNetwork n, unsigned int size)
{
	char* tmp = (char*)ALLOCMEM(size);
	CPYMEM(tmp, n->sendmsg, n->sendIndex);
	FREEMEM(n->sendmsg);
	n->sendmsg = tmp;
	n->sendSize = size;
}

void websockbuffer(WebSockNetwork n, unsigned int size)
{
	char* tmp = (char*)ALLOCMEM(size);
	CPYMEM(tmp, n->buffer, n->bufferIndex);
	FREEMEM(n->buffer);
	n->buffer = tmp;
	n->bufferSize = size;
}


inline const unsigned int rol(const unsigned int value,
	const unsigned int steps)
{
	return ((value << steps) | (value >> (32 - steps)));
}

// Sets the first 16 integers in the buffert to zero.
// Used for clearing the W buffert.
inline void clearWBuffert(unsigned int* buffert)
{
	for (int pos = 16; --pos >= 0;)
	{
		buffert[pos] = 0;
	}
}

void innerHash(unsigned int* result, unsigned int* w)
{
	unsigned int a = result[0];
	unsigned int b = result[1];
	unsigned int c = result[2];
	unsigned int d = result[3];
	unsigned int e = result[4];

	int round = 0;

#define sha1macro(func,val) \
			{ \
                const unsigned int t = rol(a, 5) + (func) + e + val + w[round]; \
				e = d; \
				d = c; \
				c = rol(b, 30); \
				b = a; \
				a = t; \
			}

	while (round < 16)
	{
		sha1macro((b & c) | (~b & d), 0x5a827999)
			++round;
	}
	while (round < 20)
	{
		w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
		sha1macro((b & c) | (~b & d), 0x5a827999)
			++round;
	}
	while (round < 40)
	{
		w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
		sha1macro(b ^ c ^ d, 0x6ed9eba1)
			++round;
	}
	while (round < 60)
	{
		w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
		sha1macro((b & c) | (b & d) | (c & d), 0x8f1bbcdc)
			++round;
	}
	while (round < 80)
	{
		w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
		sha1macro(b ^ c ^ d, 0xca62c1d6)
			++round;
	}

#undef sha1macro

	result[0] += a;
	result[1] += b;
	result[2] += c;
	result[3] += d;
	result[4] += e;
}

void sha1(const void* src, size_t bytelength, unsigned char* hash)
{
	// Init the result array.
	unsigned int result[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
	//unsigned char hash[40];

	// Cast the void src pointer to be the byte array we can work with.
	const unsigned char* sarray = (const unsigned char*)src;

	// The reusable round buffer
	unsigned int w[80];

	// Loop through all complete 64byte blocks.
	const int endOfFullBlocks = (unsigned int)bytelength - 64;
	int endCurrentBlock;
	int currentBlock = 0;

	while (currentBlock <= endOfFullBlocks)
	{
		endCurrentBlock = currentBlock + 64;

		// Init the round buffer with the 64 byte block data.
		for (int roundPos = 0; currentBlock < endCurrentBlock; currentBlock += 4)
		{
			// This line will swap endian on big endian and keep endian on little endian.
			w[roundPos++] = (unsigned int)sarray[currentBlock + 3]
				| (((unsigned int)sarray[currentBlock + 2]) << 8)
				| (((unsigned int)sarray[currentBlock + 1]) << 16)
				| (((unsigned int)sarray[currentBlock]) << 24);
		}
		innerHash(result, w);
	}

	// Handle the last and not full 64 byte block if existing.
	endCurrentBlock = (unsigned int)bytelength - currentBlock;
	clearWBuffert(w);
	int lastBlockBytes = 0;
	for (; lastBlockBytes < endCurrentBlock; ++lastBlockBytes)
	{
		w[lastBlockBytes >> 2] |= (unsigned int)sarray[lastBlockBytes + currentBlock] << ((3 - (lastBlockBytes & 3)) << 3);
	}
	w[lastBlockBytes >> 2] |= 0x80 << ((3 - (lastBlockBytes & 3)) << 3);
	if (endCurrentBlock >= 56)
	{
		innerHash(result, w);
		clearWBuffert(w);
	}
	w[15] = (unsigned int)bytelength << 3;
	innerHash(result, w);

	// Store hash in result pointer, and make sure we get in in the correct order on both endian models.
	for (int hashByte = 20; --hashByte >= 0;)
	{
		hash[hashByte] = (result[hashByte >> 2] >> (((3 - hashByte) & 0x3) << 3)) & 0xff;
	}
}


void base64_encode(const unsigned char* data, size_t input_length, char* encoded_data) {

	static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
								'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
								'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
								'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
								'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
								'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
								'w', 'x', 'y', 'z', '0', '1', '2', '3',
								'4', '5', '6', '7', '8', '9', '+', '/' };
	static int mod_table[] = { 0, 2, 1 };

	size_t output_length = 4 * ((input_length + 2) / 3);

	for (int i = 0, j = 0; i < input_length;) {

		unsigned int octet_a = i < input_length ? data[i++] : 0;
		unsigned int octet_b = i < input_length ? data[i++] : 0;
		unsigned int octet_c = i < input_length ? data[i++] : 0;

		unsigned int triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	for (int i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[output_length - 1 - i] = '=';
	encoded_data[output_length] = 0;
}