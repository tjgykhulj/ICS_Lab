/*
name: 汤劲戈
number: 5130309006
*/
#include "cachelab.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "getopt.h"

typedef struct {
	int valid;
	int tag;
	int priority;   每改一次值priority = ++pri_num，表示优先级
}line;

int hit_count, miss_count, eviction_count;
int S,E,B,s,b, flag;
int pri_num = 0;	//用来记录优先级
line **cache = NULL;

void init(int argc, char **argv, char *name)
{
	//用getopt获取指令，并初使化，同时计算出S,B方便计算
	char i;
	while ((i = getopt(argc, argv, "b:E:s:t:v"))!=-1)
	{
		if (i == 't') { strcpy(name, optarg); } else
		if (i == 'v') {	flag = 1; } else
		if (i == 'E') { E = atoi(optarg); } else
		if (i == 's') { s = atoi(optarg); S = 1<<s; } else
		if (i == 'b') { b = atoi(optarg); B = 1<<b; }
	}
}

void init_cache()
{
	// cache的初使化
	cache = (line **) malloc(S * sizeof(line*));
	for (int i=0; i<S; i++) {
		cache[i] = (line *) malloc(E * sizeof(line));
		for (int j=0; j<E; j++)
			cache[i][j].valid = 0;
	}
}

void work(int add, char op, char *result)
{
	//将地址中的组和标记算出
	int set = (add>>b) & ((1<<s)-1);
	int tag = add>>(b+s);
	for (int i=0; i<E; i++) 
	if (cache[set][i].valid && cache[set][i].tag == tag)
	{
		++hit_count;
		cache[set][i].priority = ++pri_num; //若hit中则更新此位置的优先级
		strcat(result, " hit");
		return ;
	}
	++miss_count;
	for (int i=0; i<E; i++) if (!cache[set][i].valid) 
	{
		cache[set][i].valid = 1;
		cache[set][i].tag = tag;
		cache[set][i].priority = ++pri_num; //更新此位置的优先级
		strcat(result, " miss");
		return;
	}
	++eviction_count;
	int p = 0;
	for (int i=1; i<E; i++)
		if (cache[set][i].priority < cache[set][p].priority) p = i;
	cache[set][p].valid = 1;
	cache[set][p].tag = tag;
	cache[set][p].priority = ++pri_num;  //更新此位置的优先级
	strcat(result, " miss eviction");
}

void main_work(char *name)
{
	FILE *file = fopen(name,"r");	   //打开文件
	
	char w[30] = "";
	while ( fgets(w,30,file) != 0) 
	{
		if (w[0] != ' ') continue;
		char op;
		int add, size;
		sscanf(w, " %c %x,%d\n",&op,&add,&size);	//读取这条指令的内容
		char result[100] = "";
		if (op == 'M') {		//若为M则读后再存
			work(add,'L',result); 
			work(add,'S',result); 
		}
		else work(add,op,result);

		w[strlen(w)-1] = '\0';
		if (flag) printf("%s%s\n",w+1,result);
	}
	fclose(file);
}

int main(int argc, char **argv)
{ 
	char name[30] = "";
	
	init(argc,argv,name);	//初使S\E\B等数据
	init_cache();				//初使cache
	main_work(name);		//指令处理过程
	printSummary(hit_count, miss_count, eviction_count);
	return 0;
}