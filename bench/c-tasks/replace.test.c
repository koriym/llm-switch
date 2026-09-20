#include <stdlib.h>
#include <string.h>
#include <stdio.h>
char *str_replace_all(const char *s, const char *from, const char *to);
static int fails=0;
static void c(const char*name,const char*s,const char*f,const char*t,const char*want){
 char in[128]; snprintf(in,sizeof in,"%s",s);
 char *g=str_replace_all(in,f,t);
 int ok = g && strcmp(g,want)==0 && strcmp(in,s)==0;
 if(!ok){fails++;printf("FAIL %-22s got=%s want=%s\n",name,g?g:"(null)",want);}
 else printf("ok   %-22s -> %s\n",name,g);
 free(g);}
int main(void){
 c("simple","hello world","world","there","hello there");
 c("multiple","a.b.c",".","-","a-b-c");
 c("no match","abc","x","y","abc");
 c("grow","aaa","a","xx","xxxxxx");
 c("shrink","xxxxxx","xx","x","xxx");
 c("to empty","abcabc","b","","acac");
 c("whole string","abc","abc","z","z");
 c("empty subject","","a","b","");
 c("empty from","abc","","X","abc");
 c("overlap left","aaaa","aa","b","bb");
 c("prefix","abcd","ab","Z","Zcd");
 c("suffix","abcd","cd","Z","abZ");
 c("adjacent","abab","ab","c","cc");
 printf(fails?"\n%d FAILED\n":"\nALL PASSED\n",fails); return fails!=0;}
