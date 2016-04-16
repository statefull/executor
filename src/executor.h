#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <thread>
#include <condition_variable>
#include <queue>
#include <mutex>
#include "work.h"

namespace EXECUTOR
{
	class Executor {
		friend class ExecutorFactory;
	public:

		std::future<bool> addWork(WORK::WorkInterface *w);

	private:
		Executor(bool ordered);
		~Executor();
		Executor( const Executor& other ) = delete;
		Executor& operator=( const Executor& )  = delete;

		void start();
		void stop(bool wait = false);
		void execute();

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
		Executor * createOrderedExecution();
		Executor * createUnorderedExecution();
		static void removeExecutorWaiting(Executor *executor);
		static void removeExecutorNoWaiting(Executor *executor);
		static ExecutorFactory & instance();

	private:
		ExecutorFactory();
		~ExecutorFactory();
		ExecutorFactory( const ExecutorFactory& other ) = delete;
		ExecutorFactory& operator=( const ExecutorFactory& )  = delete;
	};
}

#endif