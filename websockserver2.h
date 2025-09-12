#ifndef WEBSOCKSERVER_H_
#define WEBSOCKSERVER_H_

/**
* @file websockserver.h
* @brief standalone text-base websock network 
*
* one listening thread
* multiple process thread
* จะสร้าง thread สำหรับแต่ละ connection ไปเลย
* best for streaming
* 
* 30/08/2025 room support
* 25/06/2023 Tested
*/

//#define _PTHREAD
#define ALLOCMEM malloc
#define FREEMEM free
#define CPYMEM memcpy
#define SETMEM memset
#define TIME time_t

#if defined(_PTHREAD)
#include <pthread.h>
#else
#include <atomic>
#include <thread> 
#include <mutex>
#endif

#include <string.h>
#include <stdio.h>		
#include <stdlib.h>   
#include <ctype.h>
#include <sys/types.h>
#include <time.h>

#define FILEPTR FILE*
#define FOPEN fopen
#define FREAD fread
#define FFLUSH fflush
#define FWRITE fwrite
#define FCLOSE fclose
#define FEOF feof

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__APPLE__)
#define MSG_NOSIGNAL 0
#if !defined(__APPLE__)
#define MSG_DONTWAIT 0
#endif
#endif

#if defined(_MSC_VER)
 //Windows
 //we do not want the warnings about the old deprecated and unsecure CRT functions 
 //since these examples can be compiled under *nix as well
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <stdint.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma warning( disable : 4996 )				// disable deprecated warning

#if defined(_M_X64) 
#define FSEEK _fseeki64
#define FTELL _ftelli64
#define X64
#else
#define FSEEK fseek
#define FTELL ftell
#define X32
#endif

#define SOCKET SOCKET
#define CLOSESOCKET closesocket
#define SEPERATORCHAR '\\'
#define SEPERATORSTR "\\"
#define PACKED

#else 

#if defined(__APPLE__)
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netdb.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <sys/time.h>
#include <utime.h>
#include <unistd.h>     
#include <signal.h>

#else	
//linux
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/mman.h>	
#include <utime.h>
#include <unistd.h>  
#include <signal.h>	
#endif

#if defined(__arm__) || defined(__aarch64__)
#define X64
#define FTELL ftello
#define FSEEK fseeko
#else
#define X32
#define FTELL ftell
#define FSEEK fseek
#endif

#define SOCKET int
#define CLOSESOCKET close
#define SEPERATORCHAR '/'
#define SEPERATORSTR "/"
#define PACKED

#endif

#pragma pack(push,1)

typedef struct WebSockNetworkS* WebSockNetwork;
typedef struct WebSockNetworkConfigS* WebSockNetworkConfig;
typedef struct WebSockListenThreadS* WebSockListenThread;
typedef struct WebSockProcThreadS* WebSockProcThread;
typedef struct WebSockRoomProcThreadS* WebSockRoomProcThread;
typedef struct WebSockRoomProcThreadS* WebSockRoom;

//send and disconnect(destroy)
#define WEBSOCKMSGEND 0
//send and continue
#define WEBSOCKMSGCONTINUE 1
//send leave (not destroy)
#define WEBSOCKMSGLEAVE 2

/**
* main function
* @return WEBSOCKMSGEND, WEBSOCKSTATECONTINUE, WEBSOCKMSGLEAVE
*/
typedef int WebSockMsgFunc(WebSockNetwork n,const char* data,size_t len);

#define WEBSOCKSTATEINIT 0
#define WEBSOCKSTATECONTINUE 1
#define WEBSOCKSTATEDESTROY -1

/**
* Client network structure
*/
struct WebSockNetworkS
{
	SOCKET socket;
	struct sockaddr addr;
	/**
	* last time that client send msg to server
	*/
	TIME lastping;
	/**
	 * receive buffer
	 */
	char* buffer;
	size_t bufferSize;
	size_t bufferIndex;
	/**
	* send buffer
	*/
	char* sendmsg;
	size_t sendSize;
	size_t sendIndex;
	int state;
	void* data;
	/**
	* already handshake
	*/
	bool handshake;
	/**
	* public key from client
	*/
	char key[25];
	int index;
} PACKED;

/**
* Main Structure
*/
struct WebSockNetworkConfigS
{
	/**
	* Only one reading network thread
	*/
	WebSockListenThread listenthread;
	/**
	* num of active proc thread
	*/
#if defined(_PTHREAD)
	unsigned int numprocthread;
	pthread_mutex_t mutex;
#else
	std::atomic_uint numprocthread;
#endif
	/**
	* message function
	*/
	WebSockMsgFunc* msgFunc;
	/**
	* Exit signal
	*/
	bool exitflag;
	/**
	* max user for the whole system
	*/
	unsigned int maxconnection;
	/**
	* ping time out
	*/
	unsigned int pingtimeout;
	/**
	* msg size
	*/
	unsigned int msgsize;
} PACKED;

/**
* Thread data for listening
*/
struct WebSockListenThreadS
{
	/**
	* server address
	*/
	SOCKET server;
	/**
	* port
	*/
	int port;
	/**
	* the maximum tick per loop for optimize
	*/
	unsigned int tick;
	/**
	* tick helper
	*/
	TIME lasttime;
	/**
	* Config
	*/
	WebSockNetworkConfig config;
} PACKED;

struct WebSockProcThreadS
{
	WebSockNetworkConfig config;
	WebSockNetwork n;
} PACKED;

#if !defined(_PTHREAD)
struct WebSockMutex
{ 
	std::mutex mutex;
};
#endif

struct WebSockRoomProcThreadS
{
	WebSockNetworkConfig config;
	fd_set** tcpreadset;		//vector of fd_set 64 at maximum
 	SOCKET* tcpreadsocks;		//max socket for each fd_set
	int numreadset;
	int sizereadset;

	WebSockNetwork* user;
	int numuser;	//จำนวน user ที่อยู่ในห้อง
	int sizeuser;	//ขนาดของ ห้อง

#if defined(_PTHREAD)
	pthread_mutex_t mutex;
#else
	struct WebSockMutex* mutex;
#endif
	/**
	 * Network ที่จะใส่เข้าไป ถ้า remove แล้วจะใส่เป็น null
	 */
	WebSockNetwork add;
    size_t addId;
	/**
	 * Network ที่จะเอาออก
	 */
	WebSockNetwork remove;
	size_t removeId;
	
	/**
	 * 
	 */
	int pingturnid;
	/**
	 * เก็บว่า user ไหนที่จะ ออก
	//int numleaveuser;
	//WebSockNetwork* leaveuser;
	 */
	int numenduser;
	WebSockNetwork* enduser;
	/**
	* Exit signal
	*/
	bool exitflag;
} PACKED;

#pragma pack(pop)

/**
* Web Socket Network class
*/
class WebSockServerNetwork
{
private:
	WebSockNetworkConfig config;
public:
	/**
	 * Default 1024 connections
	 */
	static unsigned int MAXCONNECTION;
	/**
	* Default 128 concurrent
	*/
	static unsigned int MAXLISTEN;
	/**
	* Default 10000 bytes
	*/
	static unsigned int MSGSIZE;
	/**
	 * socket buffer
	 * default 1,000,000
	 */
	static unsigned int MAXBUFFER;
	/**
	* Default 20000 millisec
	*/
	static unsigned int PINGTIMEOUT;
	/**
	* @param port
	*/
	WebSockServerNetwork(int port);
	/**
	* set incoming callback
	*/
	void setOnMsg(WebSockMsgFunc* f);
	/**
	* Destructor
	*/
	~WebSockServerNetwork();
	/**
	* forever loop
	*/
	void begin();
	/**
	* thread safe
	*/
	void exit();
	/**
	 * สร้างห้อง
	 */
	WebSockRoom createRoom();
	/**
	 * ทำลายห้อง
	 */
	void destroyRoom(WebSockRoom c);
	/**
	 * WIP
	 * เพิ่ม network เข้าไปในห้อง  เอาไว้ใช้กับ thread 
	 */
	void addToRoom(WebSockRoom c, WebSockNetwork n);
	/**
	 * WIP
	 * เอา network ออกจากห้อง
	 */
	void removeFromRoom(WebSockRoom c, WebSockNetwork n);

	/**
	 * จะย้าย network จากห้องหนึ่งไปอีกห้องหนึ่ง
	 */
	void switchRoom(WebSockRoom from, WebSockRoom to, WebSockNetwork n);
};

/**
* set text msg
*/
bool websocksettext(WebSockNetwork n, const char* msg);
bool websocksetcontent(WebSockNetwork n, const char* content,size_t size);

/**
* expand the sending buffer
*/
void websockset(WebSockNetwork n, unsigned int size);
/**
* expand the receiving buffer
*/
void websockbuffer(WebSockNetwork n, unsigned int size);


#endif

