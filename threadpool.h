//threadpool.h
#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

using namespace std;

class ThreadPool
{
public:
	int thread_size;    //线程池大小（线程个数）

	typedef function<void()> Task;    //函数模板
	typedef deque<Task> Tasks;    
	//deque是类似于队列的数据结构，先进先出，而且在队尾添加元素和取出队首
	//元素的时间复杂度都是常数
	typedef vector<thread*> Threads;    //线程向量类

	ThreadPool(int size);
	~ThreadPool();

	void addTask(const Task&);

	void start();
	void stop();
	void threadLoop();
	Task take();

private:
	bool is_started;    //判断线程池是否开始工作
	Threads threads;    //线程向量，存放线程
	Tasks tasks;    //任务队列
	condition_variable cv;    //条件变量
	mutex t_mtx;    
		//任务队列的互斥锁，每个时刻最多只能有一个线程访问任务队列（要是
		//同时访问，比如同时添加任务，会出现并发问题）
};
