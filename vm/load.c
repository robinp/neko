/* ************************************************************************ */
/*																			*/
/*  Neko Virtual Machine													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h> 
#ifndef _WIN32
#	include <dlfcn.h>
#endif
#include "vm.h"
#include "neko_mod.h"

extern field id_cache;
extern field id_path;
extern field id_loader_libs;
DEFINE_KIND(k_loader_libs);

#ifdef NEKO_PROF

typedef void (*callb)( const char *name, neko_module *m, int *tot );

typedef struct {
	callb callb;
	int tot;
} dump_param;

static void profile_total( const char *name, neko_module *m, int *tot ) {
	unsigned int i;
	unsigned int n = 0;
	for(i=0;i<m->codesize;i++)
		n += (int)m->code[PROF_SIZE+i];
	*tot += n;	
}

static void profile_summary( const char *name, neko_module *m, int *ptr ) {
	unsigned int i;
	unsigned int tot = 0;
	for(i=0;i<m->codesize;i++)
		tot += (int)m->code[PROF_SIZE+i];	
	printf("%10d    %-4.1f%%  %s\n",tot,(tot * 100.0f) / (*ptr),name);
}

static void profile_details( const char *name, neko_module *m, int *tot ) {
	unsigned int i;
	printf("Details for : %s[%d]\n",name,m->codesize);
	for(i=0;i<m->codesize;i++) {
		int_val c = m->code[PROF_SIZE+i];
		if( c > 0 ) {
			unsigned int p = i;
			int param = 0;
			while( i < m->codesize ) {
				int_val k = m->code[PROF_SIZE+i];
				if( k != c ) {
					if( k == 0 ) {
						if( param == 1 )
							param = 0; 
						else
							break;
					}
				} else
					param = 1;
				i++;
			}
			i--;
			printf("%-4X %-4X    %d\n",p,i,c);
		}
	}
	printf("\n");
}

static void profile_functions( const char *name, neko_module *m, int *tot ) {
	unsigned int i;
	for(i=0;i<m->nglobals;i++) {
		value v = m->globals[i];
		if( val_is_function(v) && val_type(v) == VAL_FUNCTION && ((vfunction*)v)->module == m ) {
			int pos = (int)(((int_val)((vfunction*)v)->addr - (int_val)m->code) / sizeof(int_val));
			if( m->code[PROF_SIZE+pos] > 0 )
				printf("%-8d    %-4d %-20s %X\n",m->code[PROF_SIZE+pos],i,name,pos);
		}
	}
}

static void dump_module( value v, field f, void *p ) {
	value vname;
	const char *name;
	if( !val_is_kind(v,neko_kind_module) )
		return;
	vname = val_field_name(f);
	name = val_is_null(vname)?"???":val_string(vname);
	((dump_param*)p)->callb( name, (neko_module*)val_data(v), &((dump_param*)p)->tot );	
}

static value dump_prof() {
	dump_param p;
	value o = val_this();
	value cache;
	val_check(o,object);
	cache = val_field(o,id_cache);
	val_check(cache,object);
	p.tot = 0;
	p.callb = profile_total;
	val_iter_fields(cache,dump_module,&p);
	printf("Summary :\n");
	p.callb = profile_summary;
	val_iter_fields(cache,dump_module,&p);
	printf("%10d\n\n",p.tot);
	printf("Functions :\n");
	p.callb = profile_functions;
	val_iter_fields(cache,dump_module,&p);
	printf("\n");
	p.callb = profile_details;
	val_iter_fields(cache,dump_module,&p);
	return val_true;
}

#endif


#ifdef _WIN32
#	undef ERROR
#	include <windows.h>
#	define dlopen(l,p)		(void*)LoadLibrary(l)
#	define dlsym(h,n)		GetProcAddress((HANDLE)h,n)
#endif

static value select_file( value path, const char *file, const char *ext ) {
	struct stat s;
	value ff;
	buffer b = alloc_buffer(file);
	buffer_append(b,ext);
	ff = buffer_to_string(b);
	if( stat(val_string(ff),&s) == 0 )
		return ff;
	while( val_is_array(path) && val_array_size(path) == 2 ) {
		value p = val_array_ptr(path)[0];
		buffer b = alloc_buffer(NULL);
		path = val_array_ptr(path)[1];
		val_buffer(b,p);
		val_buffer(b,ff);
		p = buffer_to_string(b);
		if( stat(val_string(p),&s) == 0 )
			return p;
	}
	return ff;
}


static void open_module( value path, const char *mname, reader *r, readp *p ) {
	FILE *f;
	value fname = select_file(path,mname,".n");
	f = fopen(val_string(fname),"rb");
	if( f == NULL ) {
		buffer b = alloc_buffer("Module not found : ");
		buffer_append(b,mname);
		bfailure(b);
	}
	*r = neko_file_reader;
	*p = f;	
}

static void close_module( readp p ) {
	fclose(p);
}

typedef struct _liblist {
	char *name;
	void *handle;
	struct _liblist *next;
} liblist;

typedef value (*PRIM0)();

static void *load_primitive( const char *prim, int nargs, value path, liblist **libs ) {
	char *pos = strchr(prim,'@');
	int len;	
	liblist *l;
	PRIM0 ptr;
	if( pos == NULL )
		return NULL;
	l = *libs;
	*pos = 0;
	len = (int)strlen(prim) + 1;
	while( l != NULL ) {
		if( memcmp(l->name,prim,len) == 0 )
			break;
		l = l->next;
	}
	if( l == NULL ) {
		void *h;
		value pname;
		pname = select_file(path,prim,".ndll");
		h = dlopen(val_string(pname),RTLD_LAZY);
		if( h == NULL ) {
			buffer b = alloc_buffer("Library not found : ");
			val_buffer(b,pname);
#ifdef __linux__
			buffer_append(b," (");
			buffer_append(b,dlerror());
			buffer_append(b,")");
#endif
			*pos = '@';
			bfailure(b);
		}
		l = (liblist*)alloc(sizeof(liblist));
		l->handle = h;
		l->name = alloc(len);
		memcpy(l->name,prim,len);
		l->next = *libs;
		*libs = l;
		ptr = (PRIM0)dlsym(l->handle,"__neko_entry_point");
		if( ptr != NULL )
			((PRIM0)ptr())();
	}
	*pos++ = '@';
	{
		char buf[100];
		if( strlen(pos) > 90 )
			return NULL;
		if( nargs == VAR_ARGS )
			sprintf(buf,"%s__MULT",pos);
		else
			sprintf(buf,"%s__%d",pos,nargs);
		ptr = (PRIM0)dlsym(l->handle,buf);
		if( ptr == NULL )
			return NULL;
		return ptr();
	}
}

static value init_path( const char *path ) {
	value l = val_null, tmp;
	char *p;
	if( !path )
		return val_null;
	while( true ) {
		p = strchr(path,':');
		if( p != NULL )
			*p = 0;
		tmp = alloc_array(2);
		if( (p && p[-1] != '/' && p[-1] != '\\') || (!p && path[strlen(path)-1] != '/' && path[strlen(path)-1] != '\\') ) {
			buffer b = alloc_buffer(path);
			char c = '/';
			buffer_append_sub(b,&c,1);
			val_array_ptr(tmp)[0] = buffer_to_string(b);
		} else
			val_array_ptr(tmp)[0] = alloc_string(path);
		val_array_ptr(tmp)[1] = l;
		l = tmp;
		if( p != NULL )
			*p = ':';
		else
			break;
		path = p+1;
	}
	return l;
}

static value loader_loadprim( value prim, value nargs ) {
	value o = val_this();
	value libs;
	val_check(o,object);
	val_check(prim,string);
	val_check(nargs,int);
	libs = val_field(o,id_loader_libs);
	val_check_kind(libs,k_loader_libs);
	if( val_int(nargs) >= 10 || val_int(nargs) < -1 )
		neko_error();
	{		
		void *ptr = load_primitive(val_string(prim),val_int(nargs),val_field(o,id_path),(liblist**)&val_data(libs));
		if( ptr == NULL ) {
			buffer b = alloc_buffer("Primitive not found : ");
			val_buffer(b,prim);
			bfailure(b);
		}
		return alloc_function(ptr,val_int(nargs),val_string(copy_string(val_string(prim),val_strlen(prim))));
	}
}

static value loader_execute( value vm ) {
	neko_module *m;
	val_check_kind(vm,neko_kind_module);
	m = (neko_module*)val_data(vm);
	neko_vm_execute(neko_vm_current(),m);
	return m->exports;
}

static value loader_readmodule( value mname, value vthis ) {
	value o = val_this();
	value cache;
	val_check(o,object);
	val_check(mname,string);
	val_check(vthis,object);
	cache = val_field(o,id_cache);
	val_check(cache,object);
	{
		reader r;
		readp p;
		value vm;
		neko_module *m;	
		field mid = val_id(val_string(mname));
		vm = val_field(cache,mid);
		if( val_is_kind(vm,neko_kind_module) ) {
			m = (neko_module*)val_data(vm);
			return m->exports;
		}
		open_module(val_field(o,id_path),val_string(mname),&r,&p);
		m = neko_read_module(r,p,vthis);
		close_module(p);
		if( m == NULL ) {
			buffer b = alloc_buffer("Invalid module : ");
			val_buffer(b,mname);
			bfailure(b);
		}
		m->name = alloc_string(val_string(mname));
		alloc_field(cache,mid,(value)m);
		return alloc_abstract(neko_kind_module,m);
	}
}

static value loader_loadmodule( value mname, value vthis ) {
	return loader_execute(loader_readmodule(mname,vthis));
}

EXTERN value neko_default_loader() {
	value o = alloc_object(NULL);
	alloc_field(o,id_path,init_path(getenv("NEKOPATH")));
	alloc_field(o,id_cache,alloc_object(NULL));
	alloc_field(o,id_loader_libs,alloc_abstract(k_loader_libs,NULL));
	alloc_field(o,val_id("execute"),alloc_function(loader_execute,1,"execute"));
	alloc_field(o,val_id("readmodule"),alloc_function(loader_readmodule,2,"readmodule"));
	alloc_field(o,val_id("loadprim"),alloc_function(loader_loadprim,2,"loadprim"));
	alloc_field(o,val_id("loadmodule"),alloc_function(loader_loadmodule,2,"loadmodule"));
#ifdef NEKO_PROF
	alloc_field(o,val_id("dump_prof"),alloc_function(dump_prof,0,"dump_prof"));
#endif
	return o;
}

/* ************************************************************************ */
