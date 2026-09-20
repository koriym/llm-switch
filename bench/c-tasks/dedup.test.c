#include <stddef.h>
#include <stdio.h>
#include <limits.h>
size_t dedup_sorted(int *a, size_t n);
static int fails=0;
static void c(const char*name,int*in,size_t n,const int*want,size_t wn){
 int buf[64]; for(size_t i=0;i<n;i++) buf[i]=in[i];
 size_t g=dedup_sorted(buf,n); int ok=(g==wn);
 for(size_t i=0;ok&&i<wn;i++) if(buf[i]!=want[i]) ok=0;
 if(!ok){fails++;printf("FAIL %-20s got n=%zu want n=%zu [",name,g,wn);
  for(size_t i=0;i<g&&i<12;i++)printf("%d ",buf[i]);printf("]\n");}
 else printf("ok   %-20s -> %zu\n",name,g);}
int main(void){
 int e[1]; c("empty",e,0,e,0);
 int a1[]={5}; int w1[]={5}; c("single",a1,1,w1,1);
 int a2[]={1,2,3}; c("no dups",a2,3,a2,3);
 int a3[]={1,1,1}; int w3[]={1}; c("all same",a3,3,w3,1);
 int a4[]={1,1,2,2,3}; int w4[]={1,2,3}; c("pairs",a4,5,w4,3);
 int a5[]={1,2,2,2,3,4,4}; int w5[]={1,2,3,4}; c("runs",a5,7,w5,4);
 int a6[]={-3,-3,0,0,7}; int w6[]={-3,0,7}; c("negatives",a6,5,w6,3);
 int a7[]={INT_MIN,INT_MIN,INT_MAX}; int w7[]={INT_MIN,INT_MAX};
 c("extremes",a7,3,w7,2);
 int a8[]={1,1}; int w8[]={1}; c("two same",a8,2,w8,1);
 printf(fails?"\n%d FAILED\n":"\nALL PASSED\n",fails); return fails!=0;}
