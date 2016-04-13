#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>
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
		virtual void execute() = 0;
	};

	template <typename R, typename... Args>
	class Work: public WorkInterface {
	public:
		Work(std::function<R (Args... params)> work, Args... params, std::promise<bool> *workFinished = nullptr): m_work(work), args(std::forward<Args>(params)...),m_workFinished(workFinished)  {}
		~Work() {}
		void execute() {
			func(args);
		}
	private:
		template <typename... RArgs, int... Is>
		void func(std::tuple<RArgs...>& tup, HELPER::index<Is...>)
		{
			m_work(std::get<Is>(tup)...);
		}

	    template <typename... RArgs>
		void func(std::tuple<RArgs...>& tup)
		{
			func(tup, HELPER::gen_seq<sizeof...(RArgs)>{});
		}
		std::function<R (Args... params)> m_work;
		std::tuple<Args...> args;
		std::promise<bool> *m_workFinished;
	};
}

namespace EXECUTOR
{
	class Executor {
		friend class ExecutorFactory;
	public:

		void addWork(WORK::WorkInterface *w)
		{
			{
				std::lock_guard<std::mutex> guard(m_mutexWorks);
				m_works.push_back(w);
			}
			noData.notify_one();
		}

	private:
		Executor(bool ordered):m_ordered(ordered) { start(); }
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

		void stop()
		{
			{
				std::lock_guard<std::mutex> guardExecutor(m_mutexExecutor);
				m_running = false;
			}
			noData.notify_one(); //to wake up if the thread is waiting
			m_tEx.detach();
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

				WORK::WorkInterface * w = nullptr;

				{
					std::lock_guard<std::mutex> guard(m_mutexWorks);
					if(!m_works.empty()) w = m_works.front();
				}

				if(w)
				{
					std::thread t(&WORK::WorkInterface::execute,w);
					m_ordered ? t.join() : t.detach();

					{
						std::lock_guard<std::mutex> guard(m_mutexWorks);
						m_works.erase(m_works.begin());
					}
				}
			}while(m_running || !m_works.empty());

			delete this;
		}

		std::vector<WORK::WorkInterface *> m_works;
		mutable std::mutex m_mutexWorks;
		mutable std::mutex m_mutexData;
		mutable std::mutex m_mutexExecutor;
		std::condition_variable noData;
		std::thread m_tEx;
		bool m_running = false;
		bool m_ordered = true;
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

		static void removeExecutor(Executor *executor)
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
	void print(std::string text) { std::cout << text; }
};

int main()
{
	EXECUTOR::Executor * ex = EXECUTOR::ExecutorFactory::instance().createUnorderedExecution();

	std::promise<bool> promise;

	std::future<bool> fut = promise.get_future();

	std::function<void()> workFinished = [&fut] () {
		if(fut.get()) { std::cout << "work finished" << std::endl; }
	};

	std::thread waitWork(workFinished);
	waitWork.detach();
  	
	WORK::Work<void,int,int> w([] (int a,int b) -> void { std::cout << a << b; },12,15,&promise);
	
	Test t;
	WORK::Work<void> w1(std::bind(&Test::print,t,"hola"));

	ex->addWork(&w);

	EXECUTOR::ExecutorFactory::instance().removeExecutor(ex);
	usleep(10000);
}