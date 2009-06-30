/*
Copyright (C) 2003-2004 Douglas Thain and the University of Wisconsin
Copyright (C) 2005- The University of Notre Dame
This software is distributed under a BSD-style license.
See the file COPYING for details.
*/

#include "stringtools.h"
#include "timestamp.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>

#define STRINGTOOLS_BUFFER_SIZE 256
#define METRIC_POWER_COUNT 6

void string_from_ip_address( const unsigned char *bytes, char *str )
{
	sprintf(str,"%u.%u.%u.%u",
		(unsigned)bytes[0],
		(unsigned)bytes[1],
		(unsigned)bytes[2],
		(unsigned)bytes[3]);
}

int  string_to_ip_address( const char * str, unsigned char *bytes )
{
	unsigned a,b,c,d;
	int fields;

	fields = sscanf( str, "%u.%u.%u.%u", &a, &b, &c, &d );
	if(fields!=4) return 0;

	if( a>255 || b>255 || c>255 || d>255 ) return 0;

	bytes[0] = a;
	bytes[1] = b;
	bytes[2] = c;
	bytes[3] = d;

	return 1;
}

int string_ip_subnet( const char *addr, char *subnet )
{
	unsigned bytes[4];
	int fields;

	fields = sscanf(addr,"%u.%u.%u.%u",&bytes[0],&bytes[1],&bytes[2],&bytes[3]);
	if(fields!=4) return 0;

	if(bytes[0]<128) {
		sprintf(subnet,"%u",bytes[0]);
	} else if(bytes[0]<192) {	
		sprintf(subnet,"%u.%u",bytes[0],bytes[1]);
	} else {
		sprintf(subnet,"%u.%u.%u",bytes[0],bytes[1],bytes[2]);
	}

	return 1;
}

void string_chomp( char *start )
{
	char *s = start;

	if(!s) return;
	if(!*s) return;

	while(*s) {
		s++;
	}

	s--;

	while( s>=start && (*s=='\n' || *s=='\r') ) {
		*s=0;
		s--;
	}
}

int string_match( const char *pattern, const char *text )
{
        char *w;
        int headlen, taillen;

        w = strchr( pattern, '*' );
        if(!w) return !strcmp(pattern,text);

        headlen = w-pattern;
        taillen = strlen(pattern)-headlen-1;

        return !strncmp(pattern,text,headlen) && !strcmp(&pattern[headlen+1],&text[strlen(text)-taillen]);
}

char * string_front( const char *str, int max )
{
	static char buffer[STRINGTOOLS_BUFFER_SIZE];
	int length;

	length = strlen(str);
	if(length<max) {
		strcpy(buffer,str);
	} else {
		strncpy(buffer,str,max);
		buffer[max] = 0;
	}
	return buffer;
}

const char * string_back( const char *str, int max )
{
	int length;

	length = strlen(str);
	if(length<max) {
		return str;
	} else {
		return &str[length-max];
	}
}

const char * string_basename( const char *s )
{
	const char *b;

	b=s;

	while(*b) b++;

	while(b>=s) {
		if(*b=='/') {
			b++;
			break;
		} else {
			b--;
		}
	}

	if(b<s) b=s;

	return b;
}

void string_remove_trailing_slashes( char *str )
{
	char *s=str;

	/* find the end of the string */
	while(*s) s++;
	s--;

	/* remove slashes going backwards */
	while(s>str && *s=='/') {
		*s = 0;
		s--;
	}
}

void string_dirname( const char *path, char *dir )
{
	char *c;

	strcpy(dir,path);
	//string_remove_trailing_slashes(dir);

	c = strrchr(dir,'/');
	if(c) {
		*c = 0;
		if(dir[0]==0) strcpy(dir,"/");		
	} else {
		strcpy(dir,".");
	}
	//string_remove_trailing_slashes(dir);
}

char * string_metric( double invalue, int power_needed, char *buffer )
{
	static char localbuffer[100];
	static char *suffix[METRIC_POWER_COUNT] =
		{ " ", "K", "M", "G", "T", "P" };

	double value=invalue;
	int power=0;

	if(power_needed==-1) {
		while( (value>=1000.0) && (power<(METRIC_POWER_COUNT-1)) ) {
			value = value / 1024.0;
			power++;
		}
	} else {
		power = power_needed;
		value = value / (pow(2,10*power));
	}

	if(!buffer) buffer = localbuffer;

	sprintf( buffer, "%.1f %s", value, suffix[power] );

	return buffer;
}

INT64_T string_metric_parse( const char *str )
{
	INT64_T result, factor;
	char prefix;
	int fields;

	fields = sscanf(str,INT64_FORMAT "%c",&result,&prefix);
	if(fields==1) return result;

	switch(toupper(prefix)) {
		case 'K':
			factor = 1024LL;
			break;
		case 'M':
			factor = 1024LL*1024;
			break;
		case 'G':
			factor = 1024LL*1024*1024;
			break;
		case 'T':
			factor = 1024LL*1024*1024*1024;
			break;
		case 'P':
			factor = 1024LL*1024*1024*1024*1024;
			break;
		default:
			factor = 0;
			break;
	}

	return result * factor;
}

int string_time_parse( const char *str )
{
	int value;
	char mod;

	if(sscanf(str, "%d%c", &value, &mod) == 2) {
		switch (mod) {
		case 's':
			return value;
		case 'm':
			return value * 60;
		case 'h':
			return value * 60 * 60;
		case 'd':
			return value * 60 * 60 * 24;
		}
	} else if(sscanf(str, "%d", &value) == 1) {
		return value;
	}

	return 0;
}

/*
Split a string into words, recognizing only spaces.
You probably want to use string_split_quotes instead.
*/

int string_split( char *str, int *argc, char ***argv )
{
	*argc=0;

	*argv = malloc((strlen(str)+1)*sizeof(char*));
	if(!*argv) return 0;

	while(*str) {
		while(isspace((int)*str)) {
			str++;
		}
		(*argv)[(*argc)++] = str;
		while(*str && !isspace((int)*str)) {
			str++;
		}
		if(*str) {
			*str = 0;
			str++;       
		}
	}

	(*argv)[*argc] = 0;

	return 1;
}

/*
Split a string into args, respecting backwhacks and quotes.
This is probably the one you want to use.
*/

int string_split_quotes( char *str, int *argc, char ***argv )
{
	*argc=0;

	*argv = malloc((strlen(str)+1)*sizeof(char*));
	if(!*argv) return 0;

	while(*str) {

		/* Skip over leading whitespace */

		while(isspace((int)*str)) {
			str++;
		}

		if(!*str) break;

		/* The token begins here. */
		(*argv)[(*argc)++] = str;

		/* Start advancing over tokens */
		while(*str) {
			if( *str=='\\' ) {
				/* If we are backwhacked, shift and continue */
				memcpy(str,str+1,strlen(str));
				if(*str) str++;
			} else if(isspace((int)*str)) {
				/* If we have found a delimiter, accept */
				*str = 0;
				str++;
				break;
			} else if( *str=='\'' || *str=='\"' ) {
				/* Upon finding a quote, we enter a new loop */
				char quote = *str;
				memcpy(str,str+1,strlen(str));
				while(*str) {
					if(*str=='\\') {
						/* Skip anything backwhacked */
						memcpy(str,str+1,strlen(str));
						if(*str) str++;
					} else if(*str==quote) {
						/* Shift and stop on a matching quote */
						memcpy(str,str+1,strlen(str));
						break;
					} else {
						/* Otherwise, keep going */
						str++;
					}
				}
			} else if(!*str) {
				/* If we have found the end, accept */
				break;
			} else {
				/* Otherwise, continue on */
				str++;
			}
		}
	}

	(*argv)[*argc] = 0;

	return 1;
}

char * string_pad_right( char *old, int length )
{
	int i;
	char *s = malloc(length+1);
	if(!s) return 0;

	if( strlen(old) <= length ) {
		strcpy(s,old);
		for(i=strlen(old);i<length;i++) {
			s[i] = ' ';
		}
	} else {
		strncpy(s,old,length);
	}
	s[length] = 0;
	return s;
}

char * string_pad_left( char *old, int length )
{
	int i;
	int slength;
	int offset;
	char *s;

	s = malloc(length+1);
	if(!s) return 0;

	slength = strlen(old);
	offset = length-slength;

	for(i=0;i<length;i++) {
		if(i<offset) {
			s[i] = ' ';
		} else {
			s[i] = old[i-offset];
		}
	}

	s[length]=0;
	return s;
}

void string_cookie( char *s, int length )
{
	int i;

	for( i=0; i<length; i++ ) {
		s[i] = rand()%26+'a';
	}

	s[length-1] = 0;
}

char * string_subst( char *value, string_subst_lookup_t lookup, void *arg )
{
	char *subvalue, *newvalue;
	char *dollar, *ldelim, *rdelim;
	char oldrdelim;
	int length;
 
	while(1) {
		dollar = strchr(value,'$');
		if(!dollar) return value;

		while(dollar>value && *(dollar-1)=='\\') {
			dollar = strchr(dollar+1,'$');
			if(!dollar) return value;
		}
 
		ldelim = dollar+1;
		if(*ldelim=='(') {
			rdelim = ldelim+1;
			while(*rdelim!=')') rdelim++;
		} else if(*ldelim=='{') {
			rdelim = ldelim+1;
			while(*rdelim!='}') rdelim++;
		} else {
			ldelim--;
			rdelim = ldelim+1;
			while(isalpha(*rdelim)) rdelim++;
		}

		oldrdelim = *rdelim;
		*rdelim = 0;
 
		subvalue = lookup(ldelim+1,arg);
		if(!subvalue) subvalue = strdup("");

		*rdelim = oldrdelim;

		length = strlen(value) - (rdelim-dollar) + strlen(subvalue) + 1;
		newvalue = malloc(length);
		if(!newvalue) {
			free(subvalue);
			free(value);
			return 0;
		}

		if(ldelim!=dollar) rdelim++;
		*dollar = 0;

		strcpy(newvalue,value);
		strcat(newvalue,subvalue);
		strcat(newvalue,rdelim);

		free(subvalue);
		free(value);

		value = newvalue;
	}
}


/* This definition taken directly from the GNU C library */
 
#undef __strsep
#undef strsep

#ifndef CCTOOLS_OPSYS_DARWIN
 
char * strsep (char **stringp, const char *delim)
{
  char *begin, *end;
 
  begin = *stringp;
  if (begin == NULL)
    return NULL;
 
  /* A frequent case is when the delimiter string contains only one
     character.  Here we don't need to call the expensive `strpbrk'
     function and instead work using `strchr'.  */
  if (delim[0] == '\0' || delim[1] == '\0')
    {
      char ch = delim[0];
 
      if (ch == '\0')
        end = NULL;
      else
        {
          if (*begin == ch)
            end = begin;
          else if (*begin == '\0')
            end = NULL;
          else
            end = strchr (begin + 1, ch);
        }
    }
  else
    /* Find the end of the token.  */
    end = strpbrk (begin, delim);
 
  if (end)
    {
      /* Terminate the token and set *STRINGP past NUL character.  */
      *end++ = '\0';
      *stringp = end;
    }
  else
    /* No more delimiters; this is the last token.  */
    *stringp = NULL;
 
  return begin;
}

#endif

char * string_combine( char *a, char *b )
{
        char *r;
        
        if( a && b ) {
                r = malloc(strlen(a)+strlen(b)+1);
                if(r) {
                        strcpy(r,a);
                        strcat(r,b);
                }
        } else {
                r = 0;
        }

        if(a) free(a);
        if(b) free(b);

        return r;
}



char * string_combine_multi( char *r, ... )
{
	char *n;
	va_list args;
	va_start(args,r);


	while( (n=va_arg(args,char*)) ) {
		r = string_combine(r,n);
	}

	return r;
	
	va_end(args);
}

char * string_signal( int sig )
{
#ifdef HAS_STRSIGNAL
	return strsignal(sig);
#else
	return (char*) _sys_siglist[sig];
#endif
}

void string_split_path( const char *input, char *first, char *rest )
{
	/* skip any leading slashes */
	while(*input=='/') {
		input++;
	}
 
	/* copy the first element up to slash or null */
	while( *input && *input!='/' ) {
		*first++ = *input++;
	}
	*first=0;
 
	/* make sure that rest starts with a slash */
	if( *input!='/' ) {
		*rest++ = '/';
	}
 
	/* copy the rest */
	while( *input ) {
		*rest++ = *input++;
	}
	*rest = 0;
}

/*
Canonicalize a path name by removing duplicate
slashes, dots, and so on.
*/

void string_collapse_path( const char *l, char *s, int remove_dotdot )
{
	char *start = s;

	while(*l) {
		if( (*l)=='/' && (*(l+1))=='.' && (*(l+2))==0  ) {
			l++;
			l++;
		} else if( (*l)=='/' && (*(l+1))=='.' && (*(l+2))=='/'  ) {
			l++;
			l++;
		} else if( (*l)=='/' && (*(l+1))==0 ) {
			l++;
		} else if( remove_dotdot && !strncmp(l,"/..",3) ) {
			if(s>start) s--;
			while( s>start && ((*s) != '/') ) {
				s--;
			}
			*s = 0;
			l++; l++; l++;
		} else {
			*s++ = *l++;
		}
	}

	*s = 0;

	if(s==start) {
		strcpy(s,"/");
	} else {
		string_remove_trailing_slashes(s);
	}
}

void string_tolower( char *s )
{
	while(*s) {
		*s = tolower((*s));
		s++;
	}
}

void string_toupper( char *s )
{
	while(*s) {
		*s = toupper((*s));
		s++;
	}
}

int string_is_integer( const char *s )
{
	while(*s) {
		if(!isdigit(*s)) return 0;
		s++;
	}
	return 1;
}

int strpos(char *str, char c) {

    int i;
    if(str != NULL) {
	for(i = 0; i < strlen(str); i++) {
	    if(str[i] == c)
		return i;
	}
    }
    return -1;
}


int strrpos(char *str, char c) {

    int i;
    if(str != NULL) {
	for(i = strlen(str)-1; i>=0; i--) {
	    if(str[i] == c)
		return i;
	}
    }
    return -1;
}

int getDateString(char* str) {

    int retval;
    char *Month[12] = {"Jan","Feb","Mar","Apr","May","Jun",
		       "Jul","Aug","Sep","Oct","Nov","Dec"};
    
    struct tm *T = NULL;                   
    time_t Tval = 0;
    Tval = time(NULL);
    T = localtime(&Tval);
    if(T->tm_mday < 10)
	retval = sprintf(str,"%s0%d",Month[T->tm_mon],T->tm_mday);
    else
	retval = sprintf(str,"%s%d",Month[T->tm_mon],T->tm_mday);
    if(retval <= 4)
	return 0;
    else
	return 1;
}
