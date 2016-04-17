#include <iostream>
#include <unistd.h>
#include <executor.h>



class Test {
public:
	Test(){}
	~Test(){}
	void print(std::string text) { std::cout << text << std::endl; }
};

int main()
{
	EXECUTOR::Executor * ex = EXECUTOR::ExecutorFactory::instance().createUnorderedExecution();
	WORK::Work<void,int,int> w([] (int a,int b) -> void { std::cout << a << b << std::endl; },12,15);

	Test t;
	WORK::Work<void> w1(std::bind(&Test::print,t,"hola"));

	std::future<bool> fut = ex->addWork(&w);
	std::future<bool> fut2 = ex->addWork(&w1);
	std::future<bool> fut3 = ex->addWork(&w);

	std::function<void()> workFinished = [&fut] () {
		if(fut.get()) { std::cout << "work finished" << std::endl; }
	};

	std::function<void()> workFinished2 = [&fut2] () {
		if(fut2.get()) { std::cout << "work finished 2" << std::endl; }
	};

	std::function<void()> workFinished3 = [&fut3] () {
		if(fut3.get()) { std::cout << "work finished 3" << std::endl; }
	};

	std::thread(workFinished).detach();
	std::thread(workFinished2).detach();
	std::thread(workFinished3).detach();

	EXECUTOR::ExecutorFactory::instance().removeExecutorNoWaiting(ex);

	usleep(10000);

}