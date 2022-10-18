/*** 
 * @Author: baisichen
 * @Date: 2022-10-13 20:06:37
 * @LastEditTime: 2022-10-13 20:08:14
 * @LastEditors: baisichen
 * @Description: 大小端
 */
#include<iostream>
#include<stdio.h>
using  namespace  std;
void byteorder() {
	union {
		short value;
		char union_bytes[sizeof (short)];
	} test;
	test.value = 0x0102;
	//大端高位字节(23-31bit)存储在内存低地址，低位字节(0-7bit)存储在内存的高地址
	//从编译角度来看，大端更符合栈的形式
	if (test.union_bytes[0] == 1 && test.union_bytes[1] == 2) {
		printf("big endian\n");
	} else if(test.union_bytes[0] == 2 && test.union_bytes[1] == 1) {
		//小端低位字节(0-7bit)存储在内存低地址，高位字节(23-31bit)存储在内存的高地址
		printf("little endian\n");
	} else {
		printf("unknown");
	}
}
int main()
{
	byteorder();
	return 0;
}