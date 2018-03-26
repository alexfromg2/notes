#pragma once
#include <mutex>

template <typename T> class ListThreadsafe
{
public:
	ListThreadsafe() = default;
	~ListThreadsafe()
	{
		removeIf([](T const&) {return true; });
	}

	ListThreadsafe(ListThreadsafe const&) = delete;
	ListThreadsafe& operator=(ListThreadsafe const&) = delete;

	void pushFront(T* value)
	{
		std::unique_ptr<node> newNode(new node(value));
		std::lock_guard<std::mutex> guard(head.m_mutex);
		newNode->next = std::move(head.next);
		head.next = std::move(newNode);
	}

	template <typename Function> void forEach(Function f)
	{
		node* current = &head;
		std::unique_lock<std::mutex> headLock(head.m_mutex);
		while (node* const next = current->next.get())
		{
			std::unique_lock<std::mutex> nextLock(next->m_mutex);
			headLock.unlock();
			f(*next->data);
			current = next;
			headLock = std::move(nextLock);
		}
	}

	template <typename Predicate> std::shared_ptr<T> findFirstIf(Predicate p)
	{
		node* current = &head;
		std::unique_lock<std::mutex> headLock(head.m_mutex);
		while (node* const next = current->next.get())
		{
			std::unique_lock<std::mutex> nextLock(next->m_mutex);
			headLock.unlock();
			if (p(*next->data))
			{
				return next->data;
			}
			current = next;
			headLock = std::move(nextLock);
		}
		return nullptr;// std::shared_ptr<T>();
	}

	template <typename Predicate> std::shared_ptr<T> removeIf(Predicate p)
	{
		node* current = &head;
		std::unique_lock<std::mutex> headLock(head.m_mutex);
		while (node* const next = current->next.get())
		{
			std::unique_lock<std::mutex> nextLock(next->m_mutex);
			if (p(*(next->data)))
			{
				std::unique_ptr<node> oldNext = std::move(current->next);
				current->next = std::move(next->next);
				nextLock.unlock();
			}
			else
			{
				headLock.unlock();
				current = next;
				headLock = std::move(nextLock);
			}
		}
		return nullptr;//std::shared_ptr<T>();
	}
private:
	struct node
	{
		std::mutex m_mutex;
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;

		node() :
			next()
		{}

		node(T* value) :
			data(value)//TODO: here was make_shared. make initialization by shared_ptr and not by raw pointer
		{}
	};

	node head;
};