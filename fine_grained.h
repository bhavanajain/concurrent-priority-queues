#include <atomic>
#include <mutex>
#include <iostream>
#include <vector>
#include <assert.h>

template<class T> struct Node
{
	static const int EMPTY = -1;
	static const int AVAILABLE = -2;
		
	int priority;
	T item;
	int statusTag;
	std::shared_ptr<std::recursive_mutex> nodeLock;

	Node()
	{
		nodeLock = std::make_shared<std::recursive_mutex>();
		statusTag = EMPTY;
	}

	Node(T myItem, int myPriority, int threadID)
	{
		nodeLock = std::make_shared<std::recursive_mutex>();
		item = myItem;
		priority = myPriority;
		statusTag = threadID;
	}

	void swap(Node<T>& a)
	{
		std::swap(a.priority, this->priority);
		std::swap(a.item, this->item);
		std::swap(a.statusTag, this->statusTag);
	}

	void lock()
	{
		nodeLock -> lock();
	}

	void unlock()
	{
		nodeLock -> unlock();
	}
};

template<class T> class priorityQueue
{
	private:
		const int ROOT = 1;
		int maxLimit;
		int next;
		std::vector<Node<T>> heapArray;
		std::atomic_flag heapLock = ATOMIC_FLAG_INIT;

		void lock()
		{
			while(heapLock.test_and_set())
				;
		}

		void unlock()
		{
			heapLock.clear();
		}
		
		bool hasLeftChild(int i)
		{
			return ((2 * i) < maxLimit);
		}

		bool hasRightChild(int i)
		{
			return ((2 * i + 1) < maxLimit);
		}

	public:
		
		priorityQueue(int n)
		{
			assert(n > 0);
			next = 1;
			maxLimit = n + 1;
			heapArray.resize(maxLimit);
		}

		void insert(int priority, int threadID, T item = 0)
		{
			int currID = threadID;

			lock();	/*This is heapLock's lock()*/
			
			int child = next++;
			if(child >= maxLimit)
			{
				fprintf(stderr, "Out of bounded memory, element will not be inserted\n");
				next--;
				unlock();
				return;
			}
			
			heapArray[child].nodeLock -> lock();

			heapArray[child].item = item;
			heapArray[child].priority = priority;
			heapArray[child].statusTag = threadID;
			unlock();	/*This is heapLock's unlock()*/
			heapArray[child].nodeLock -> unlock();

			while(child > ROOT)
			{
				int parent = child / 2;
				heapArray[parent].nodeLock -> lock();
				heapArray[child].nodeLock -> lock();
				int oldChild = child;

				/*Checking if parent is available and current thread still owns the child node*/
				if(heapArray[parent].statusTag == Node<T>::AVAILABLE && heapArray[child].statusTag == currID)
				{
					if(heapArray[child].priority < heapArray[parent].priority)
					{
						heapArray[child].swap(heapArray[parent]);
						child = parent;
					}
					else
					{
						heapArray[child].statusTag = Node<T>::AVAILABLE;
						heapArray[parent].nodeLock -> unlock();
						heapArray[child].nodeLock -> unlock();
						return;
					}
				}
				/*current thread is not the owner of the child node*/
				else if(heapArray[child].statusTag != currID)
				{
					child = parent;
				}
				
				heapArray[oldChild].nodeLock -> unlock();
				heapArray[parent].nodeLock -> unlock();
			}

			if(child == ROOT)
			{
				heapArray[ROOT].nodeLock -> lock();
				if(heapArray[ROOT].statusTag == currID)
				{
					heapArray[ROOT].statusTag = Node<T>::AVAILABLE;
				}
				heapArray[ROOT].nodeLock -> unlock();
			}
		}

		int removeMin(int threadID)
		{
			int currID = threadID;
			
			lock();	/*This is heapLock's lock()*/
			
			int bottom = --next;
			if(bottom < 0)
			{
				fprintf(stderr, "No element available for removal\n");
				next++;
				unlock();
				return INT_MIN;
			}
			heapArray[ROOT].nodeLock -> lock();
			heapArray[bottom].nodeLock -> lock();
			
			unlock();	/*This is heapLock's unlock()*/

			int minItem = heapArray[ROOT].priority;
			heapArray[ROOT].statusTag = Node<T>::EMPTY;
			heapArray[ROOT].swap(heapArray[bottom]);
			heapArray[bottom].nodeLock -> unlock();

			if(heapArray[ROOT].statusTag == Node<T>::EMPTY)
			{
				heapArray[ROOT].nodeLock -> unlock();
				return minItem;
			}

			heapArray[ROOT].statusTag = Node<T>::AVAILABLE;
			int child = 0;
			int parent = ROOT;

			while(parent < heapArray.size() / 2)
			{
				int left = parent * 2;
				heapArray[left].nodeLock -> lock();
				int right;

				if(hasRightChild(parent))
				{
					right = (parent * 2) + 1;
					heapArray[right].nodeLock -> lock();
				}
				else
				{
					right = -1;
				}

				if(heapArray[left].statusTag == Node<T>::EMPTY)
				{
					heapArray[left].nodeLock -> unlock();
					if(hasRightChild(parent))
					{
						heapArray[right].nodeLock -> unlock();
					}
					break;
				}
				else if(!hasRightChild(parent))
				{
					child = left;
				}
				else if(heapArray[right].statusTag == Node<T>::EMPTY || heapArray[left].priority < heapArray[right].priority)
				{
					heapArray[right].nodeLock -> unlock();
					child = left;
				}
				else
				{
					heapArray[left].nodeLock -> unlock();
					child = right;
				}

				if(heapArray[child].priority < heapArray[parent].priority && heapArray[child].statusTag != Node<T>::EMPTY)
				{
					heapArray[parent].swap(heapArray[child]);
					heapArray[parent].nodeLock -> unlock();
					parent = child;
				}
				else
				{
					heapArray[child].nodeLock -> unlock();
					break;
				}
			}
			heapArray[parent].nodeLock -> unlock();
			return minItem;
		}
};
