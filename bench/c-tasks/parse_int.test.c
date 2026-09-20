#include <stddef.h>
#include <limits.h>
#include <stdio.h>
int parse_int_strict(const char *s, long *out);
static int fails=0;
static void ok_case(const char*in,long want){long v=12345;int r=parse_int_strict(in,&v);
 if(!(r==1&&v==want)){fails++;printf("FAIL accept %-22s r=%d v=%ld want=%ld\n",in,r,v,want);}
 else printf("ok   accept %-22s -> %ld\n",in,v);}
static void bad_case(const char*name,const char*in){long v=12345;int r=parse_int_strict(in,&v);
 if(!(r==0&&v==12345)){fails++;printf("FAIL reject %-22s r=%d v=%ld (must not touch *out)\n",name,r,v);}
 else printf("ok   reject %-22s\n",name);}
int main(void){
 ok_case("0",0); ok_case("7",7); ok_case("-7",-7); ok_case("+7",7);
 ok_case("000123",123); ok_case("-0",0);
 char bufmax[32],bufmin[32]; snprintf(bufmax,sizeof bufmax,"%ld",LONG_MAX);
 snprintf(bufmin,sizeof bufmin,"%ld",LONG_MIN);
 ok_case(bufmax,LONG_MAX); ok_case(bufmin,LONG_MIN);
 bad_case("empty","");  bad_case("sign only","-"); bad_case("plus only","+");
 bad_case("leading space"," 1"); bad_case("trailing space","1 ");
 bad_case("trailing junk","1x"); bad_case("leading junk","x1");
 bad_case("inner space","1 2"); bad_case("double sign","--1");
 bad_case("hex","0x10"); bad_case("dot","1.0");
 bad_case("overflow","99999999999999999999");
 bad_case("underflow","-99999999999999999999");
 printf(fails?"\n%d FAILED\n":"\nALL PASSED\n",fails); return fails!=0;}
