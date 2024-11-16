/* This file is part of munin-node-win32
 * Copyright (C) 2006-2007 Jory Stone (jcsston@jory.info)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "StdAfx.h"
#include "JCSocket.h"
#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <ws2tcpip.h> 
#include <Winsock2.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <ws2ipdef.h>

#pragma comment(lib, "Ws2_32.lib")

#define STRICMP _stricmp

int JCSocket::m_RefCount = 0;

JCSocket::JCSocket()
{
  WSADATA wsaData;

  if (++m_RefCount == 1)
  {
      WSAStartup(MAKEWORD(2, 2), &wsaData);
  }

  strncpy(m_Address.sa_data, "::", 2);
  m_hSocket = NULL; 
}


JCSocket::~JCSocket()
{
  Close();

  if( --m_RefCount == 0 )
  {
    ::WSACleanup();
  }
}


bool JCSocket::Create( int af, ProtocolType type )
{
    switch (type)
    {
    case TCP:
        m_hSocket = ::socket(af, SOCK_STREAM, IPPROTO_TCP);
        break;
    case UDP:
        m_hSocket = ::socket(af, SOCK_DGRAM, IPPROTO_UDP);
        break;
    default:
        assert(false);
    };

    if (m_hSocket == INVALID_SOCKET)
    {
        return false;
    }
    else
    {
        return true;
    }
}


bool JCSocket::Connect( const char *host, int nPort )
{
  ADDRINFO Hints, * AddrInfo;  
  char Port[10];

  itoa(nPort, Port, 10);
 
  Hints.ai_family = AF_INET6;
  Hints.ai_socktype = SOCK_STREAM;
  Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE; 

  auto RetVal = getaddrinfo(host, Port, &Hints, &AddrInfo);
  if (RetVal != 0) {
      fprintf(stderr, "getaddrinfo failed with error %d: %s\n",
          RetVal, gai_strerror(RetVal));
      WSACleanup();
  }

  if( connect( m_hSocket, AddrInfo->ai_addr, (int)AddrInfo->ai_addrlen) == SOCKET_ERROR )
  {
    return false;
  }
  else
  {
    return true;
  }
}


bool JCSocket::Bind(int nLocalPort, const char *address)
{
    char Port[10];
    char* Address = NULL;
    int i, isBound;
    ADDRINFO Hints, * AddrInfo, * AI;
    SOCKET ServSock[FD_SETSIZE];

    itoa(nLocalPort, Port, 10);
    strncpy(m_Address.sa_data, address ? address : "::", address ? sizeof(address) : 3);

    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = PF_INET6;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
    getaddrinfo(Address, Port, &Hints, &AddrInfo);

    for (i = 0, AI = AddrInfo; AI != NULL; AI = AI->ai_next) {

        if (i == FD_SETSIZE) {
            printf("getaddrinfo returned more addresses than we could use.\n");
            break;
        }
        if ((AI->ai_family != PF_INET) && (AI->ai_family != PF_INET6))
            continue;

        ServSock[i] = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
        if (ServSock[i] == INVALID_SOCKET) {
            fprintf(stderr, "socket() failed with error %d: \n", WSAGetLastError());
            continue;
        }

        isBound = bind(ServSock[i], AI->ai_addr, (int)AI->ai_addrlen);
        if (isBound == SOCKET_ERROR)
        {
            closesocket(ServSock[i]);
            continue;
        }
        else
        {
            m_hSocket = ServSock[i];
            return true;
        }
    }

    return false;
}


bool JCSocket::Accept( JCSocket *pSocket )
{
    sockaddr_storage From;

  if( pSocket == NULL )
  {
    return false;
  }

  int len = sizeof(From);
  strncpy(pSocket->m_Address.sa_data, "::", 3);

  pSocket->m_hSocket = accept(m_hSocket, (LPSOCKADDR)&From, &len);

  if( pSocket->m_hSocket == INVALID_SOCKET )
  {
    return false;
  }
  else
  {
    return true;
  }
}


bool JCSocket::Listen( int nBackLog )
{
  if( ::listen( m_hSocket, nBackLog ) == SOCKET_ERROR )
  {
    return false;
  }
  else
  {
    return true;
  }
}


int JCSocket::Send( const void *pData, int nDataLen, int nFlags )
{
  return ::send( m_hSocket, (const char *)pData, nDataLen, nFlags );
}


int JCSocket::SendText( const char *pszText )
{
  return Send( pszText, (int)strlen( pszText ) );
}


int JCSocket::Recv( void *pData, int nDataLen, int nFlags )
{
  return ::recv( m_hSocket, (char *)pData, nDataLen, nFlags );
}


int JCSocket::RecvLine( char *pszBuf, int nLen, bool bEcho )
{
  int nCount = 0;
  int nRdLen;
  char ch = 0;

  while( ch != '\n' && nCount < nLen )
  {
    nRdLen = Recv( &ch, 1 );

    if( nRdLen == 0 || nRdLen == SOCKET_ERROR )
    {
      nCount = 0;
      break;
    }

    if( ch != '\n' && ch != '\r' )
    {
      pszBuf[nCount] = ch;
      nCount++;
    }

    if( bEcho )
    {
      Send( &ch, 1 );
    }
  }

  if( nCount != 0 )
  {
    pszBuf[nCount] = 0;
  }

  return nCount ? nCount : nRdLen;
}

int JCSocket::RecvFrom(void* pData, int nDataLen, int nFlags)
{
    int fromAddressLen = sizeof(m_FromAddress);
    return ::recvfrom(m_hSocket, (char*)pData, nDataLen, nFlags, (sockaddr*)&m_FromAddress, &fromAddressLen);
}

bool JCSocket::Shutdown( int nHow )
{
  return ::shutdown( m_hSocket, nHow ) == SOCKET_ERROR ? false : true;
}


bool JCSocket::Close( void )
{
  return ::closesocket( m_hSocket ) == SOCKET_ERROR ? false : true;
}
