/******************************************************************
 *****                                                        *****
 *****  Name: easyweb.h                                       *****
 *****  Ver.: 1.0                                             *****
 *****  Date: 07/05/2001                                      *****
 *****  Auth: Andreas Dannenberg                              *****
 *****        HTWK Leipzig                                    *****
 *****        university of applied sciences                  *****
 *****        Germany                                         *****
 *****  Func: header-file for easyweb.c                       *****
 *****                                                        *****
 ******************************************************************/

#ifndef __EASYWEB_H
#define __EASYWEB_H

// CodeRed - added #define extern on next line (else variables
// not defined). This has been done due to include the .h files
// rather than the .c files as in the original version of easyweb.
//#define extern

// CodeRed - removed header for original ethernet controller
//#include "cs8900.c"                              // ethernet packet driver

//CodeRed - added for LPC ethernet controller
//#include "ethmac.h"

// CodeRed - include .h rather than .c file
// #include "tcpip.c"                               // easyWEB TCP/IP stack
//#include "tcpip.h"                               // easyWEB TCP/IP stack

// CodeRed - added NXP LPC register definitions header
//#include "LPC17xx.h"


// CodeRed - include renamed .h rather than .c file
// #include "webside.c"                             // webside for our HTTP server (HTML)
//#include "webside.h"                             // webside for our HTTP server (HTML)


int easyweb (void);
void start(void);
void InitOsc(void);                              // prototypes
void InitPorts(void);
void HTTPServer(void);
void InsertDynamicValues(void);
unsigned int GetAD7Val(void);
unsigned int GetTempVal(void);

unsigned char *PWebSide;                         // pointer to webside
unsigned int HTTPBytesToSend;                    // bytes left to send

unsigned char HTTPStatus;                        // status byte
#define HTTP_SEND_PAGE               0x01        // help flag

#endif

