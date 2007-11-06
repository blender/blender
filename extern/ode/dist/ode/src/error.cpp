/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

#include <ode/config.h>
#include <ode/error.h>


static dMessageFunction *error_function = 0;
static dMessageFunction *debug_function = 0;
static dMessageFunction *message_function = 0;


extern "C" void dSetErrorHandler (dMessageFunction *fn)
{
  error_function = fn;
}


extern "C" void dSetDebugHandler (dMessageFunction *fn)
{
  debug_function = fn;
}


extern "C" void dSetMessageHandler (dMessageFunction *fn)
{
  message_function = fn;
}


extern "C" dMessageFunction *dGetErrorHandler()
{
  return error_function;
}


extern "C" dMessageFunction *dGetDebugHandler()
{
  return debug_function;
}


extern "C" dMessageFunction *dGetMessageHandler()
{
  return message_function;
}


static void printMessage (int num, const char *msg1, const char *msg2,
			  va_list ap)
{
  fflush (stderr);
  fflush (stdout);
  if (num) fprintf (stderr,"\n%s %d: ",msg1,num);
  else fprintf (stderr,"\n%s: ",msg1);
  vfprintf (stderr,msg2,ap);
  fprintf (stderr,"\n");
  fflush (stderr);
}

//****************************************************************************
// unix

#ifndef WIN32

extern "C" void dError (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (error_function) error_function (num,msg,ap);
  else printMessage (num,"ODE Error",msg,ap);
  exit (1);
}


extern "C" void dDebug (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (debug_function) debug_function (num,msg,ap);
  else printMessage (num,"ODE INTERNAL ERROR",msg,ap);
  // *((char *)0) = 0;   ... commit SEGVicide
  abort();
}


extern "C" void dMessage (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (message_function) message_function (num,msg,ap);
  else printMessage (num,"ODE Message",msg,ap);
}

#endif

//****************************************************************************
// windows

#ifdef WIN32

// isn't cygwin annoying!
#ifdef CYGWIN
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif


#include "windows.h"


extern "C" void dError (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (error_function) error_function (num,msg,ap);
  else {
    char s[1000],title[100];
    _snprintf (title,sizeof(title),"ODE Error %d",num);
    _vsnprintf (s,sizeof(s),msg,ap);
    s[sizeof(s)-1] = 0;
    MessageBox(0,s,title,MB_OK | MB_ICONWARNING);
  }
  exit (1);
}


extern "C" void dDebug (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (debug_function) debug_function (num,msg,ap);
  else {
    char s[1000],title[100];
    _snprintf (title,sizeof(title),"ODE INTERNAL ERROR %d",num);
    _vsnprintf (s,sizeof(s),msg,ap);
    s[sizeof(s)-1] = 0;
    MessageBox(0,s,title,MB_OK | MB_ICONSTOP);
  }
  abort();
}


extern "C" void dMessage (int num, const char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  if (message_function) message_function (num,msg,ap);
  else printMessage (num,"ODE Message",msg,ap);
}


#endif
