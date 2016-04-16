#include "executor.h"
#include <limits>

namespace EXECUTOR
{
	Executor::Executor(bool ordered):m_running(false), m_ordered(ordered), m_waitForDestroy(false), m_promiseIndex(0) { start(); }
	Executor::~Executor() {}

	std::future<bool> Executor::addWork(WORK::WorkInterface *w)
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

	void Executor::start()
	{
		std::lock_guard<std::mutex> guardExecutor(m_mutexExecutor);
		if(!m_running)
		{
			m_running = true;
			m_tEx = std::thread(&EXECUTOR::Executor::execute,this);
		}
	}

	void Executor::stop(bool wait)
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

	void Executor::execute()
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

	ExecutorFactory::ExecutorFactory() {}
	ExecutorFactory::~ExecutorFactory() {}

	Executor * ExecutorFactory::createOrderedExecution()
	{
		return new Executor(true);
	}

	Executor * ExecutorFactory::createUnorderedExecution()
	{
		return new Executor(false);
	}

	void ExecutorFactory::removeExecutorWaiting(Executor *executor)
	{
		executor->stop(true);
	}

	void ExecutorFactory::removeExecutorNoWaiting(Executor *executor)
	{
		executor->stop();
	}

	ExecutorFactory & ExecutorFactory::instance()
	{
		static ExecutorFactory instance;
		return instance;
	}
}