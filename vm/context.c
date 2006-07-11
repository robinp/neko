/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
#include "context.h"
#include "neko.h"

#ifdef NEKO_WINDOWS
#include <windows.h>
// disable warnings for type conversions
#pragma warning(disable : 4311)
#pragma warning(disable : 4312)

_context *context_new() {
	DWORD t = TlsAlloc();
	TlsSetValue(t,NULL);
	return (_context*)t;
}

void context_delete( _context *ctx ) {
	TlsFree((DWORD)ctx);
}

void context_set( _context *ctx, void *c ) {
	TlsSetValue((DWORD)ctx,c);
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return (void*)TlsGetValue((DWORD)ctx);
}

#else
/* ************************************************************************ */
#include <stdlib.h>
#include <pthread.h>

struct _context {
	pthread_key_t key;
};

_context *context_new() {
	_context *ctx = malloc(sizeof(_context));
	pthread_key_create( &ctx->key, NULL );	
	return ctx;
}

void context_delete( _context *ctx ) {
	pthread_key_delete( ctx->key );
	free(ctx);
}

void context_set( _context *ctx, void *data ) {
	pthread_setspecific( ctx->key, data );
}

void *context_get( _context *ctx ) {
	if( ctx == NULL )
		return NULL;
	return pthread_getspecific( ctx->key );
}

#endif
/* ************************************************************************ */
