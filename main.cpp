//main.cpp

#include "threadpool.h"

#include <iostream>

mutex out_mtx;    //输出的锁，防止多个线程同时输出互相干扰

void testFunc();

int main()
{
	ThreadPool thread_pool1(5);    //有5个线程的线程池
	for (int i = 0; i < 100; ++i)    //添加100个任务testFunc进入任务队列
	{
		thread_pool1.addTask(testFunc);
	}
	//这里要等加进去的任务全部执行完，因此还欠缺
	//system("pause");    //不能这么写，可能到这里时线程还没执行完
	getchar();    //等待所有任务执行完，输入任意字符，执行stop()
	thread_pool1.stop();
	system("pause");
	return 0;
}

void testFunc()
{
	unique_lock<mutex> lck(out_mtx);
	cout << "tid = " << this_thread::get_id() << endl;
}
