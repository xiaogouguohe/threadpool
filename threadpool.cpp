//threadpool.cpp
#include "threadpool.h"

#include <iostream>
#include <assert.h>

using namespace std;

ThreadPool::ThreadPool(int size) : 
	is_started(false), 
	thread_size(size), 
	t_mtx(), 
	cv()
{
	start();    //线程池开始工作
}

ThreadPool::~ThreadPool()
{
	if (is_started)
	{
		stop();
	}
}

void ThreadPool::start()
{
	assert(threads.empty());    
		//线程池为空，才开始工作，不为空说明异常
		//（初始状态下线程池内没有线程）
	is_started = true;    //设置启动位
	for (int i = 0; i < thread_size; ++i)    //向线程池内添加线程
	{
		//thread t1(threadLoop);    //不能这么写
		//threads.push_back(t1);
		threads.push_back(new thread(bind(&ThreadPool::threadLoop, 
			this)));    //子线程执行threadLoop()
	}
}

void ThreadPool::threadLoop()    
	//每个线程都不断地尝试在任务队列中取任务
{
	while (is_started)
	{
		Task task = take();    //获取任务
		if (task)    //获取到任务
		{
			task();    //执行
		}
		//进入下一轮循环，再次尝试获取任务
	}
}

void ThreadPool::stop()
{
	is_started = false;    
		//标志位设为false，令子进程退出threadLoop()的循环
	cv.notify_all();    //会不会出现已经运行完的问题？

	for (int i = 0; i < threads.size(); ++i)
	{
		(threads[i])->join();    //等待子线程 *threads[i] 执行完
		delete threads[i];    //释放子线程指针指向的空间
	}
	threads.clear();
}

ThreadPool::Task ThreadPool::take()
{
	unique_lock<mutex> lck(t_mtx);    
		//这里的lck作用是锁住任务队列tasks，只有当前进程能访问tasks
		//因为要随时释放，所以最好用unique_lock而非lock_guard，
		//lock_guard在作用域结束的时候自动释放
	while (is_started && tasks.empty())
	{
		cv.wait(lck);    
			//没有任务，阻塞，释放锁lck，允许其他线程访问tasks，
			//addTask的时候唤醒
	}

	//该线程被唤醒
	Task task;
	int s = tasks.size();
	if (is_started && !tasks.empty())    //当前有任务
	{
		task = tasks.front();
		tasks.pop_front();
		assert(s - 1 == tasks.size());    
			//检查是否有别的线程访问并改动了tasks（同时加减可以吗？）
	}
	return task;
}

void ThreadPool::addTask(const Task& task)
{
	unique_lock<mutex> lck(t_mtx);    
		//锁住tasks，这个线程向tasks添加任务时别的线程不能访问tasks
		//可能用lock_guard也可以，开销更小
	tasks.push_back(task);    //添加任务
	cv.notify_one();    //释放锁lck，唤醒一个阻塞在take()的线程
}
