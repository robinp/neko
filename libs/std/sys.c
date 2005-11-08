/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
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
#include <neko.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#	include <windows.h>
#	include <direct.h>
#else
#	include <unistd.h>
#	include <dirent.h>
#	include <sys/times.h>
#endif

static value get_env( value v ) {
	char *s;
	val_check(v,string);
	s = getenv(val_string(v));
	if( s == NULL )
		return val_null;
	return alloc_string(s);
}

static value put_env( value e, value v ) {
	buffer b;
	val_check(e,string);
	val_check(v,string);
	b = alloc_buffer(NULL);
	val_buffer(b,e);
	buffer_append_sub(b,"=",1);
	val_buffer(b,v);
	if( putenv(val_string(buffer_to_string(b))) != 0 )
		neko_error();
	return val_true;
}

static value sys_sleep( value f ) {
	val_check(f,float);
#ifdef _WIN32
	Sleep((DWORD)(val_float(f) * 1000));
#else
	if( (int)val_float(f) > 0 )
		sleep((int)val_float(f));
	usleep( (int)((val_float(f) - (int)val_float(f)) * 1000000) );
#endif
	return val_true;
}

static value set_time_locale( value l ) {
	val_check(l,string);
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
}

static value get_cwd() {
	char buf[256];
	int l;
	if( getcwd(buf,256) == NULL )
		neko_error();
	l = (int)strlen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_string(buf);
}

static value set_cwd( value d ) {
	val_check(d,string);
	if( chdir(val_string(d)) )
		neko_error();
	return val_true;
}

static value sys_string() {
#if defined(_WIN32)
	return alloc_string("Windows");
#elif defined(linux) || defined(__linux__)
	return alloc_string("Linux");
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	return alloc_string("BSD");
#elif defined(__APPLE__) || defined(__MACH__) || defined(macintosh)
	return alloc_string("Mac");
#else
#error Unknow system string
#endif
}

static value sys_command( value cmd ) {
	val_check(cmd,string);
	if( val_strlen(cmd) == 0 )
		return alloc_int(-1);
	return alloc_int( system(val_string(cmd)) );
}

static value sys_exit( value ecode ) {
	val_check(ecode,int);
	exit(val_int(ecode));
	return val_true;
}

static value file_exists( value path ) {
	struct stat st;
	val_check(path,string);
	return alloc_bool(stat(val_string(path),&st) == 0);
}

static value file_delete( value path ) {
	val_check(path,string);
	if( unlink(val_string(path)) != 0 )
		neko_error();
	return val_true;
}

static value file_rename( value path, value newname ) {
	val_check(path,string);
	val_check(newname,string);
	if( rename(val_string(path),val_string(newname)) != 0 )
		neko_error();
	return val_true;
}

#define STATF(f) alloc_field(o,val_id(#f),alloc_int(s.st_##f))
#define STATF32(f) alloc_field(o,val_id(#f),alloc_int32(s.st_##f))

static value sys_stat( value path ) {
	struct stat s;
	value o;
	val_check(path,string);
	if( stat(val_string(path),&s) != 0 )
		neko_error();
	o = alloc_object(NULL);
	STATF(gid);
	STATF(uid);
	STATF32(atime);
#ifdef _WIN32
	alloc_field(o,val_id("mtime"),alloc_int32(s.st_ctime));
#else
	STATF(mtime);
#endif
	STATF32(ctime);
	STATF(dev);
	STATF(ino);
	STATF(mode);
	STATF(nlink);
	STATF(rdev);
	STATF(size);
	return o;
}

static value sys_file_type( value path ) {
	struct stat s;
	val_check(path,string);
	if( stat(val_string(path),&s) != 0 )
		neko_error();
	if( s.st_mode & S_IFREG )
		return alloc_string("file");
	if( s.st_mode & S_IFDIR )
		return alloc_string("dir");
	if( s.st_mode & S_IFCHR )
		return alloc_string("char");
#ifndef _WIN32
	if( s.st_mode & S_IFLNK )
		return alloc_string("symlink");
	if( s.st_mode & S_IFBLK )
		return alloc_string("block");
	if( s.st_mode & S_IFIFO )
		return alloc_string("fifo");
	if( s.st_mode & S_IFSOCK )
		return alloc_string("sock");
#endif
	neko_error();
}

static value sys_create_dir( value path, value mode ) {
	val_check(path,string);
	val_check(mode,int);
#ifdef _WIN32
	if( mkdir(val_string(path)) != 0 )
#else
	if( mkdir(val_string(path),val_int(mode)) != 0 )
#endif
		neko_error();
	return val_true;
}

static value sys_remove_dir( value path ) {
	val_check(path,string);
	if( rmdir(val_string(path)) != 0 )
		neko_error();
	return val_true;
}

static value sys_time() {
#ifdef _WIN32
	FILETIME unused;
	FILETIME stime;
	FILETIME utime;
	if( !GetProcessTimes(GetCurrentProcess(),&unused,&unused,&stime,&utime) )
		neko_error();
	return alloc_float( ((tfloat)(utime.dwHighDateTime+stime.dwHighDateTime)) * 65.536 * 6.556 + (((tfloat)utime.dwLowDateTime + (tfloat)stime.dwLowDateTime) / 10000000) );
#else
	struct tms t;
	times(&t);
	return alloc_float( ((tfloat)(t.tms_utime + t.tms_stime)) / CLK_TCK );
#endif
}

static value sys_read_dir( value path ) {
	value h = val_null;
	value cur = NULL, tmp;
#ifdef _WIN32
	WIN32_FIND_DATA d;
	HANDLE handle;
	buffer b;
	int len;
	val_check(path,string);
	len = val_strlen(path);
	b = alloc_buffer(NULL);
	val_buffer(b,path);
	if( len && val_string(path)[len-1] != '/' && val_string(path)[len-1] != '\\' )
		buffer_append(b,"/*.*");
	else
		buffer_append(b,"*.*");
	path = buffer_to_string(b);
	handle = FindFirstFile(val_string(path),&d);
	if( handle == INVALID_HANDLE_VALUE )
		neko_error();
	while( true ) {
		// skip magic dirs
		if( d.cFileName[0] != '.' || (d.cFileName[1] != 0 && (d.cFileName[1] != '.' || d.cFileName[2] != 0)) ) {
			tmp = alloc_array(2);
			val_array_ptr(tmp)[0] = alloc_string(d.cFileName);
			val_array_ptr(tmp)[1] = val_null;
			if( cur )
				val_array_ptr(cur)[1] = tmp;
			else
				h = tmp;
			cur = tmp;
		}
		if( !FindNextFile(handle,&d) )
			break;
	}
	CloseHandle(handle);	
#else
	DIR *d;
	struct dirent *e;
	val_check(path,string);
	d = opendir(val_string(path));
	if( d == NULL )
		neko_error();
	while( true ) {
		e = readdir(d);
		if( e == NULL )
			break;
		// skip magic dirs
		if( e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0)) )
			continue;
		tmp = alloc_array(2);
		val_array_ptr(tmp)[0] = alloc_string(e->d_name);
		val_array_ptr(tmp)[1] = val_null;
		if( cur )
			val_array_ptr(cur)[1] = tmp;
		else
			h = tmp;
		cur = tmp;
	}
#endif
	return h;
}

static value file_full_path( value path ) {
#ifdef _WIN32
	char buf[MAX_PATH+1];
	val_check(path,string);
	if( GetFullPathName(val_string(path),MAX_PATH+1,buf,NULL) == 0 )
		neko_error();
	return alloc_string(buf);
#else
	char buf[PATH_MAX];	
	val_check(path,string);	
	if( realpath(val_string(path),buf) == NULL )
		neko_error();
	return alloc_string(buf);
#endif
}

static value sys_exe_path() {
#ifdef _WIN32
	char path[MAX_PATH];
	if( GetModuleFileName(NULL,path,MAX_PATH) == 0 )
		neko_error();
	return alloc_string(path);
#elif __APPLE__
	char path[MAXPATHLEN+1];
	unsigned long path_len = MAXPATHLEN;
	if( _NSGetExecutablePath(path, &path_len) )
		neko_error();
	return alloc_string(path);
#else
	const char *p = getenv("_");
	if( p != NULL )
		return alloc_string(p);
	{
		char path[PATH_MAX];
		int length = readlink("/proc/self/exe", path, sizeof(path));
		if( length < 0 )
			neko_error();
	    path[length] = '\0';
		return alloc_string(path);
	}
#endif
}

#ifndef _WIN32
extern char **environ;
#endif

static value sys_env() {
	value h = val_null;
	value cur = NULL, tmp, key;
	char **e = environ;
	while( *e ) {
		char *x = strchr(*e,'=');
		if( x == NULL ) {
			e++;
			continue;
		}
		tmp = alloc_array(3);
		key = alloc_empty_string((int)(x - *e));
		memcpy(val_string(key),*e,(int)(x - *e));
		val_array_ptr(tmp)[0] = key;
		val_array_ptr(tmp)[1] = alloc_string(x+1);
		val_array_ptr(tmp)[2] = val_null;
		if( cur )
			val_array_ptr(cur)[2] = tmp;
		else
			h = tmp;
		cur = tmp;
		e++;
	}
	return h;
}

DEFINE_PRIM(get_env,1);
DEFINE_PRIM(put_env,2);
DEFINE_PRIM(set_time_locale,1);
DEFINE_PRIM(get_cwd,0);
DEFINE_PRIM(set_cwd,1);
DEFINE_PRIM(sys_sleep,1);
DEFINE_PRIM(sys_command,1);
DEFINE_PRIM(sys_exit,1);
DEFINE_PRIM(sys_string,0);
DEFINE_PRIM(sys_stat,1);
DEFINE_PRIM(sys_time,0);
DEFINE_PRIM(sys_env,0);
DEFINE_PRIM(sys_create_dir,2);
DEFINE_PRIM(sys_remove_dir,1);
DEFINE_PRIM(sys_read_dir,1);
DEFINE_PRIM(file_full_path,1);
DEFINE_PRIM(file_exists,1);
DEFINE_PRIM(file_delete,1);
DEFINE_PRIM(file_rename,1);
DEFINE_PRIM(sys_exe_path,0);
DEFINE_PRIM(sys_file_type,1);

/* ************************************************************************ */
