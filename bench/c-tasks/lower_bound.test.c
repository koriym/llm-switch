#include <stddef.h>
#include <stdio.h>
#include <limits.h>
size_t lower_bound(const int *a, size_t n, int key);
static int fails=0;
static void c(const char*name,const int*a,size_t n,int key,size_t want){
 size_t g=lower_bound(a,n,key);
 if(g!=want){fails++;printf("FAIL %-22s key=%d got=%zu want=%zu\n",name,key,g,want);}
 else printf("ok   %-22s key=%d -> %zu\n",name,key,g);}
int main(void){
 const int e[1]={0}; c("empty",e,0,5,0);
 const int a[]={1,3,3,5,7};
 c("below all",a,5,0,0); c("exact first",a,5,1,0);
 c("between",a,5,2,1); c("dup first",a,5,3,1);
 c("after dup",a,5,4,3); c("exact last",a,5,7,4);
 c("above all",a,5,8,5);
 const int s[]={2}; c("single below",s,1,1,0); c("single equal",s,1,2,0);
 c("single above",s,1,3,1);
 const int x[]={INT_MIN,0,INT_MAX};
 c("int_min",x,3,INT_MIN,0); c("int_max",x,3,INT_MAX,2);
 static int big[100000]; for(int i=0;i<100000;i++) big[i]=i/2;
 c("large",big,100000,4999,9998);
 printf(fails?"\n%d FAILED\n":"\nALL PASSED\n",fails); return fails!=0;}
