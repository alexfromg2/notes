#pragma once
#include <thread>
#include <future>

struct ThreadInterrupted {};

class InterruptFlag
{
	std::atomic<bool> flag;
	std::condition_variable* threadCond;
	std::condition_variable_any* threadCondAny;
	std::mutex setClearMutex;
public:
	InterruptFlag():
		flag(false),
		threadCond(nullptr),
		threadCondAny(nullptr)
	{
	}

	void set()
	{
		flag.store(true, std::memory_order_relaxed);
		std::lock_guard<std::mutex> lk(setClearMutex);
		if(threadCond)
		{
			threadCond->notify_all();
		}
		if(threadCondAny)
		{
			threadCondAny->notify_all();
		}
	}

	bool isSet()const
	{
		return flag.load(std::memory_order_relaxed);
	}

	template<typename Lockable> void wait(std::condition_variable_any& cv, Lockable& lk)
	{
		struct CustomLock
		{
			InterruptFlag* m_self;
			Lockable& m_lk;

			CustomLock(InterruptFlag* self, std::condition_variable_any& cond, Lockable& lk_):
				m_self(self),
				m_lk(lk_)
			{
				self->setClearMutex.lock();
				self->threadCondAny = &cond;
			}

			void unlock()
			{
				m_lk.unlock();
				m_self->setClearMutex.unlock();
			}

			void lock()
			{
				std::lock(m_self->setClearMutex, m_lk);
			}

			~CustomLock()
			{
				m_self->threadCondAny = nullptr;
				m_self->setClearMutex.unlock();
			}
		};

		CustomLock cl(this, cv, lk);
		interruptionPoint();
		cv.wait(cl);
		interruptionPoint();
	}

	void setConditionVariable(std::condition_variable& cv)
	{
		std::lock_guard<std::mutex> lk(setClearMutex);
		threadCond = &cv;
	}

	void clearConditionVariable()
	{
		std::lock_guard<std::mutex> lk(setClearMutex);
		threadCond = nullptr;
	}

	struct ClearConditionVariableOnDestroy
	{
		~ClearConditionVariableOnDestroy();
	};
};

thread_local InterruptFlag this_thread_interrupt_flag;

InterruptFlag::ClearConditionVariableOnDestroy::~ClearConditionVariableOnDestroy()
{
	this_thread_interrupt_flag.clearConditionVariable();
}

void interruptionPoint()
{
	if (this_thread_interrupt_flag.isSet())
	{
		throw ThreadInterrupted();
	}
}

class InterruptibleThread
{
public:
	template<typename FunctionType> InterruptibleThread(FunctionType f)
	{
		std::promise<InterruptFlag*> p;
		m_thread = std::thread([f, &p] {p.set_value(&this_thread_interrupt_flag);
		                                try
		{
			f();
		}
		catch (ThreadInterrupted const&)
		{

		}
		                               });
		m_interruptFlag = p.get_future().get();
	}

	void join()
	{
		m_thread.join();
	}

	void detach()
	{
		m_thread.detach();
	}

	bool joinable()const
	{
		return m_thread.joinable();
	}

	void interrupt()
	{
		if (m_interruptFlag)
		{
			m_interruptFlag->set();
		}
	}

	void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk)
	{
		interruptionPoint();
		this_thread_interrupt_flag.setConditionVariable(cv);
		InterruptFlag::ClearConditionVariableOnDestroy guard;
		interruptionPoint();
		cv.wait_for(lk, std::chrono::milliseconds(1));
		interruptionPoint();
	}

	template<typename Predicate> void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk,
	        Predicate pred)
	{
		interruptionPoint();
		this_thread_interrupt_flag.setConditionVariable(cv);
		InterruptFlag::ClearConditionVariableOnDestroy guard;
		while (!this_thread_interrupt_flag.isSet() && !pred())
		{
			cv.wait_for(lk, std::chrono::milliseconds(1));
		}
		interruptionPoint();
	}

	template<typename _Rep, typename _Period> void interruptibleWait(std::condition_variable& cv,
	        std::unique_lock<std::mutex>& lk, const std::chrono::duration<_Rep, _Period>& relTime)
	{
		auto minTime = (relTime < std::chrono::milliseconds(1)) ? relTime : std::chrono::milliseconds(1);
		interruptionPoint();
		this_thread_interrupt_flag.setConditionVariable(cv);
		InterruptFlag::ClearConditionVariableOnDestroy guard;
		interruptionPoint();
		cv.wait_for(lk, minTime);
		interruptionPoint();
	}

	template<typename Predicate, typename _Rep, typename _Period> void interruptibleWait(std::condition_variable& cv,
	        std::unique_lock<std::mutex>& lk, const std::chrono::duration<_Rep, _Period>& relTime,
	        Predicate pred)
	{
		auto minTime = (relTime < std::chrono::milliseconds(1)) ? relTime : std::chrono::milliseconds(1);
		interruptionPoint();
		this_thread_interrupt_flag.setConditionVariable(cv);
		InterruptFlag::ClearConditionVariableOnDestroy guard;
		while (!this_thread_interrupt_flag.isSet() && !pred())
		{
			cv.wait_for(lk, minTime);
		}
		interruptionPoint();
	}

	template<typename Lockable> void interruptibleWait(std::condition_variable_any& cv, Lockable& lk)
	{
		this_thread_interrupt_flag.wait(cv, lk);
	}

	template<typename T> void interruptibleWait(std::future<T>& uf)
	{
		while (!this_thread_interrupt_flag.isSet())
		{
			if (uf.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
			{
				break;
			}
		}
		interruptionPoint();
	}
private:
	std::thread m_thread;
	InterruptFlag* m_interruptFlag;
};