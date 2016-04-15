#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>
#include <map>
#include <limits>
#include <unistd.h>

namespace WORK
{
	namespace HELPER
	{
	    template <int... Is>
		struct index {};

	    template <int N, int... Is>
		struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};

	    template <int... Is>
		struct gen_seq<0, Is...> : index<Is...> {};
	}

	class WorkInterface {
	public:
		virtual void execute(int index = 0) = 0;
		virtual void setExecutionConfirmation(std::promise<bool> *promise, int index = 0) = 0;
	};

	template <typename R, typename... Args>
	class Work: public WorkInterface {
	public:
		Work(std::function<R (Args... params)> work, Args... params): m_work(work), args(std::forward<Args>(params)...) {}
		~Work() {}
		void execute(int index = 0) {
			func(args, index);			
		}
	private:
		template <typename... RArgs, int... Is, typename Index>
		void func(std::tuple<RArgs...>& tup, HELPER::index<Is...>, Index index)
		{
			m_work(std::get<Is>(tup)...);
			if(m_workFinished.at(index)) { m_workFinished.at(index)->set_value(true); m_workFinished.erase(index); }
		}

	    template <typename... RArgs, typename Index>
		void func(std::tuple<RArgs...>& tup, Index index)
		{
			func(tup, HELPER::gen_seq<sizeof...(RArgs)>{}, index);
		}

		void setExecutionConfirmation(std::promise<bool> *promise, int index = 0)
		{
			m_workFinished[index] = promise;
		}

		std::function<R (Args... params)> m_work;
		std::tuple<Args...> args;
		std::map<int,std::promise<bool>*> m_workFinished;
	};
}

namespace EXECUTOR
{
	class Executor {
		friend class ExecutorFactory;
	public:

		std::future<bool> addWork(WORK::WorkInterface *w)
		{
			std::lock_guard<std::mutex> guard(m_mutexWorks);
			
			unsigned int nextIndex = 0;
			nextIndex = (m_promiseIndex % std::numeric_limits<unsigned int>::max());
			m_works.push(std::make_pair(nextIndex,w));			
			
			std::promise<bool> *promise = new std::promise<bool>();			
			w->setExecutionConfirmation(promise,nextIndex);
			m_promiseIndex++;
			noData.notify_one();
			return promise->get_future();
		}

	private:
		Executor(bool ordered):m_ordered(ordered), m_waitForDestroy(false), m_running(false), m_promiseIndex(0) { start(); }
		~Executor() {}
		Executor( const Executor& other ) = delete;
		Executor& operator=( const Executor& )  = delete;

		void start()
		{
			std::lock_guard<std::mutex> guardExecutor(m_mutexExecutor);
			if(!m_running)
			{
				m_running = true;
				m_tEx = std::thread(&EXECUTOR::Executor::execute,this);
			}
		}

		void stop(bool wait = false)
		{
			{
				std::lock_guard<std::mutex> guardExecutor(m_mutexExecutor);
				m_running = false;
				m_waitForDestroy = wait;
			}
			noData.notify_one(); //to wake up if the thread is waiting
			if(wait) { m_tEx.join(); delete this; } //wait until all works have been executed
			else { m_tEx.detach(); }
		}

		void execute()
		{
			do
			{
				std::unique_lock<std::mutex> uniqueLock(m_mutexData);
				noData.wait(uniqueLock,[this] () -> bool {

					bool running = false;
					{
						std::lock_guard<std::mutex> guardExecutor(m_mutexExecutor);
						running = m_running;
					}

					return (!m_works.empty() || !running);

				});

				std::pair<int,WORK::WorkInterface *> w;
				thread_local bool runThread = false;

				{
					std::lock_guard<std::mutex> guard(m_mutexWorks);
					
					if(!m_works.empty())
					{ 
						w = m_works.front(); 
						m_works.pop();
						runThread = true;						
					}
				}

				if(runThread)
				{
					runThread = false;
					std::thread t(&WORK::WorkInterface::execute,std::get<1>(w),std::get<0>(w));
					m_ordered ? t.join() : t.detach();
				}
				
				
			}while(m_running || !m_works.empty());

			if(!m_waitForDestroy) { delete this; }
		}

		std::queue<std::pair<unsigned int,WORK::WorkInterface *>> m_works;
		mutable std::mutex m_mutexWorks;
		mutable std::mutex m_mutexData;
		mutable std::mutex m_mutexExecutor;
		std::condition_variable noData;
		std::thread m_tEx;
		bool m_running;
		bool m_ordered;
		bool m_waitForDestroy;
		unsigned int m_promiseIndex;
	};


	class ExecutorFactory {
	public:
		Executor * createOrderedExecution()
		{
			return new Executor(true);
		}

		Executor * createUnorderedExecution()
		{
			return new Executor(false);
		}

		static void removeExecutorWaiting(Executor *executor)
		{
			executor->stop(true);
		}

		static void removeExecutorNoWaiting(Executor *executor)
		{
			executor->stop();
		}

		static ExecutorFactory & instance()
		{
			static ExecutorFactory instance;
			return instance;
		}

	private:
		ExecutorFactory() {}
		~ExecutorFactory() {}
		ExecutorFactory( const ExecutorFactory& other ) = delete;
		ExecutorFactory& operator=( const ExecutorFactory& )  = delete;
	};
}

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