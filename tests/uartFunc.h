#ifndef __UARTFUNC_H__
#define __UARTFUNC_H__

int uartOpen(char *Dev);
void uartSetSpeed(int fd, int speed);
int uartSetParity(int fd,int databits,int stopbits,int parity);

#endif // end of __UARTFUNC_H__

// end of file

