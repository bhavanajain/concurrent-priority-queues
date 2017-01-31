#include <iostream>
#include <random>
#include <queue>
#include <mutex>

std::mutex QLock;

template<class T> class coarsePriorityQueue
{
	private:
		std::priority_queue<T> CQueue;

	public:

		void insert(T value)
		{
			QLock.lock();
			CQueue.push(value);
			QLock.unlock();
		}

		T removeMin()
		{
			QLock.lock();
			T ans = CQueue.top();
			CQueue.pop();
			QLock.unlock();
			return ans;	
		}
};
