
extern char initSocket(char *srvAddr, int port);
extern char readData(void *dst, int len);
extern char sendData(char *src, int len);
extern void closeSocket();
