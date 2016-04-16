#ifndef WORK_H
#define WORK_H

#include <functional>
#include <future>
#include <map>

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
		virtual void execute(unsigned int index = 0) = 0;
		virtual void setExecutionConfirmation(std::promise<bool> *promise, unsigned int index = 0) = 0;
	};

	template <class R, class... Args>
	class Work: public WorkInterface {
	public:
		Work(std::function<R (Args... params)> work, Args... params): m_work(work), args(std::forward<Args>(params)...) {}
		~Work() {}
		void execute(unsigned int index = 0) {
			func(args, index);
		}
	private:
		template <typename... RArgs, int... Is, typename Index>
		void func(std::tuple<RArgs...>& tup, HELPER::index<Is...>, Index index)
		{
			m_work(std::get<Is>(tup)...);
			if(m_workFinished.at(index))
			{
				std::promise<bool> *promise = m_workFinished.at(index);
				promise->set_value(true);
				delete promise;
				m_workFinished.erase(index);
			}
		}

		template <typename... RArgs, typename Index>
		void func(std::tuple<RArgs...>& tup, Index index)
		{
			func(tup, HELPER::gen_seq<sizeof...(RArgs)>{}, index);
		}

		void setExecutionConfirmation(std::promise<bool> *promise, unsigned int index = 0)
		{
			m_workFinished[index] = promise;
		}

		std::function<R (Args... params)> m_work;
		std::tuple<Args...> args;
		std::map<unsigned int,std::promise<bool>*> m_workFinished;
	};
}
#endif