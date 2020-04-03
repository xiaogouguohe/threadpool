# 一个简陋线程池的实现



## 1 线程池的工作原理

线程池是什么？顾名思义，线程池就是一个放着若干线程的池子。整个线程池的工作原理是这样的，在一个池子里有若干个线程，此外还有任务队列，当任务队列为空的时候，线程池的所有线程也处于休眠（阻塞）状态；当任务队列中有任务的时候，需要唤醒线程来拿走任务队列中的任务；新任务产生的时候，这个任务会进入任务队列，等待线程来执行它。也就是说，我们需要维护两个数据结构，一个是线程池，另一个是任务队列。



## 2 为什么需要线程池？

线程池的优势就是，线程池的线程是一直存在的，也就是说**我们不会像往常的做法那样，一旦有了一个任务就创建一个线程来执行它，执行完成后再把这个线程销毁，这样就节省了创建线程和销毁线程的开销，** 因为虽然线程和进程相比，体量要小得多，但是反复地创建和销毁进程还是需要占用一定的资源。而线程池的出现，就允许线程在没有任务的时候阻塞，有任务的时候再被唤醒。

当然，线程池也不是所有场合都使用的。当任务产生的速度特别快的时候，如果线程池内线程的数量太少，就会使得任务的实时性大大降低。



## 3 怎么实现一个线程池？

这次的线程池是最简陋的实现，没有任务优先级（所有任务都是一样的优先级，遵循先来先执行），线程池大小在初始化之后不再变化，执行的任务都是void()类型。进行了这些简化以后，代码就变得很短。



### 3.1 代码和实现流程

```cpp
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
```

```cpp
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
```

```cpp
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
```

以上就是线程池的实现。简单来说，就是**主线程在线程池中放入若干个子线程，每个子线程都在threadLoop()不断循环，不断尝试take()，也就是在任务队列中拿任务出来执行。而take()做的事情就是，当任务队列为空的时候阻塞，直到有任务进入任务队列再去拿这个任务，然后就返回拿到的任务给上游的线程循环threadLoop()，threadLoop()就可以执行这个任务了。**

这个流程是不难理解的，但是有些细节需要注意。



### 3.2 互斥锁的使用

这里用到了两个互斥锁，一个t_mux，还有一个out_mtx，它们一个是任务队列的锁，还有一个是输出的锁。什么意思呢？**对于任务队列tasks来说，每个时刻只能有最多一个线程来访问它。** 想象一下有两个线程同时向任务队列内增添任务，任务队列的最后一个任务是tail，那么它们可能最后会都在tail后面的一个位置增加这个任务（因为任务队列的大小还没来得及改动），最后的结果就是，只增加了一个任务，这显然不是我们想看到的。因此我们需要给任务队列加锁（不过**假如有两个线程，有一个取走队首元素，有一个在队尾增添元素，这两个线程能同时执行吗？** 不是很清楚）。

还有一个锁是输出锁，这个是很好理解的，假如有多个线程同时输出，那它们最后可能会同时输出在一行，而且可能是一个线程输出一部分，紧接着另一个线程又输出一部分，这样几个句子就杂糅在一起了。因此要给输出加锁。



### 3.3 一些C++语法

这次用到了函数模板function，并发编程中的条件变量condition_variable和互斥锁mutex，两种锁lock_guard和unique_lock（注意它们是一定义就自动上锁），注意这些C++的语法。



### 3.4 如何正确结束线程池的工作？

主线程需要等待子线程执行完再退出，留意stop()的代码，join()等子线程退出，delete释放子线程指针指向的空间。

2020.3.29
