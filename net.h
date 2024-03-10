
extern char initSocket(const char *srvAddr, const char *port);
extern char readData(void *dst, int len);
extern char sendData(char *src, int len);
extern void closeSocket();
