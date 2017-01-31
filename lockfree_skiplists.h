#include <iostream>
#include <vector>
#include <atomic>
#include <random>
#include <climits>
#include <cstdio>

int maxHeight;

void setMaxHeight(int h)
{
	maxHeight = h;
}

/*
 * Random level numbers should be generated according to geometric distribution.
 */
std::default_random_engine generator(time(NULL));
std::geometric_distribution<int> distribution(0.5);
int getRandomLevel()
{
	int temp = distribution(generator) + 1;
	if(temp < maxHeight)
	{
		return temp;
	}
	return maxHeight;
}

/*
 * Struct for atomic markable reference
 */
template<class T> struct markableReference
{
	T* next;
	bool marked;

	markableReference()
	{
		next = NULL;
		marked = false;
	}

	markableReference(T* node, bool mark)
	{
		next = node;
		marked = mark;
	}

	bool operator==(const markableReference<T>& other)
	{
		return (next == other.next && marked == other.marked);
	}
};

/* 
 * The AtomicMarkableReference class
 */
template<class T> class atomicMarkableReference
{
	private:
		/*
		 * Since a pointer is always a POD type, even if T is of non-pod type
		 * atomicity will be maintained
		 */
		std::atomic<markableReference<T>*> markedNext;
		
	public:
		atomicMarkableReference()
		{
			markedNext.store(new markableReference<T> (NULL, false));
		}

		atomicMarkableReference(T* nextNode, bool mark)
		{
			/* Atomically store the values*/
			markedNext.store(new markableReference<T> (nextNode, mark));
		}

		/*
		* Returns the reference. load() is atomic and hence that will be the linearization point
		*/
		T* getReference()
		{
			return markedNext.load()->next;
		}

		/*
		 * Returns the reference and update bool in the reference passed as the argument
		 * load() is atomic and hence that will be the linearization point.
		 */
		T* get(bool *mark)
		{
			markableReference<T> *temp = markedNext.load();
			*mark = temp->marked;
			return temp->next;
		}
		
		/*
		* Set the variables unconditionally. load() is atomic and hence that will be the linearization point
		*/
		void set(T* newRef, bool newMark)
		{
			markableReference<T> *curr = markedNext.load();
			if(newRef != curr->next || newMark != curr->marked)
			{
				markedNext.store(new markableReference<T>(newRef, newMark));
			}
		}

		/*
		 * CAS with reference and the marked field. load() is atomic and hence that will be the linearization point.
		 * We take advantage of the fact that C++ has short-circuiting hence
		 * if one of the first 2 conditions is false the atomic_compare_exchange_weak will not happen
		 */

		bool CAS(T* expected, T* newValue, bool expectedBool, bool newBool)
		{
			markableReference<T> *curr = markedNext.load();
			return(expected == curr->next && expectedBool == curr->marked && 
					((newValue == curr->next && newBool == curr->marked) ||
					 markedNext.compare_exchange_strong(curr, new markableReference<T>(newValue, newBool))));
		}
};

template<class X> struct skipListNode
{
		X value;
		int priority;	
		int topLevel;
		std::atomic<bool> deleted;
		std::vector<atomicMarkableReference<skipListNode<X>>> next;

		skipListNode()
		{
			priority = 0;
			deleted = false;
			next = std::vector<atomicMarkableReference<skipListNode<X>>>(maxHeight + 1);
			topLevel = maxHeight;
		}

		skipListNode(int priority, X value, int height = maxHeight)
		{
			this->priority = priority;
			this->value = value;
			next = std::vector<atomicMarkableReference<skipListNode<X>>>(height + 1);
			deleted = false;
			topLevel = height;
		}
};

template<class X> class skipListQueue
{
	private:
		skipListNode<X> *head;
		skipListNode<X> *tail;

	public:

		void display()
		{
			skipListNode<X> *temp = head;
			while(temp != NULL)
			{
				std::cout << temp->priority << " " << temp->value << "\n";
				temp = temp->next[0].getReference();
			}
		}

		skipListQueue()
		{
			head = new skipListNode<X>(INT_MIN, 0);
			tail = new skipListNode<X>(INT_MAX, 0);

			/* 
			 * Set next of head to tail and
			 * next of tail will be NULL, false due to default constructors
			 */
			for(int i = 0; i <= maxHeight; i++)
			{
				head->next[i].set(tail, false);
			}
		}
		
		/*
		* Populates the input paramater preds and succs with the 
		* predecessors and successors of the priority value - prio 
		* In addition it resets the links in case of a marked node.
		* Returns if the prio key exists.
		*/
		bool findNode(int prio, std::vector<skipListNode<X>* >& preds, std::vector<skipListNode<X>* >& succs)
		{
			int bottomLevel = 0;
			bool *mark = new bool;
			bool snip;
			skipListNode<X> *pred = new skipListNode<X>();
			skipListNode<X> *curr = new skipListNode<X>();
			skipListNode<X> *succ = new skipListNode<X>();
			RETRY:
			while(true)
			{
				pred = head;
				for(int i = maxHeight; i >= bottomLevel; i--)
				{
					curr = pred->next[i].getReference();
					while(true)
					{
						succ = curr->next[i].get(mark);
						while(mark[0] || curr->deleted.load())
						{
							snip = pred->next[i].CAS(curr, succ, false, false);	
							if(!snip)
							{
								goto RETRY;
							}
							curr = pred->next[i].getReference();
							succ = curr->next[i].get(mark);
						}
						if(curr->priority < prio)
						{
							pred = curr;
							curr = succ;
						}
						else
						{
							break;
						}
					}
					preds[i] = pred;
					succs[i] = curr;
				}
				return (curr->priority == prio);
			}
		}

		/*
		 * The priorities are unique and we don't insert in case of duplicate addition. 
		 * Hence, it returns true in case of successful addition and false otherwise.
		 */
		bool insert(int priority, X element = 0)
		{
			int maxLevel = getRandomLevel();
			int bottomLevel = 0;
			std::vector<skipListNode<X>* > preds(maxHeight + 1);
			std::vector<skipListNode<X>* > succs(maxHeight + 1);
			while(true)
			{
				bool found = findNode(priority, preds, succs);
				if (found) {
					return false;
				}
				
				skipListNode<X> *newNode = new skipListNode<X>(priority, element, maxLevel);
				for(int i = bottomLevel; i <= maxLevel; i++)
				{
					newNode->next[i].set(succs[i], false);
				}

				skipListNode<X> *pred = preds[bottomLevel];
				skipListNode<X> *succ = succs[bottomLevel];

				if(!pred->next[bottomLevel].CAS(succ, newNode, false, false))
				{
					continue;
				}
				bool *mark = new bool;
				newNode->next[bottomLevel].get(mark);
				for(int i = bottomLevel + 1; i <= maxLevel; i++)
				{
					while(true)
					{
						pred = preds[i];
						succ = succs[i];
						if(pred->next[i].CAS(succ, newNode, false, false))
						{
							break;
						}
						findNode(priority, preds, succs);
					}

				}
				return true;
			}
		}

		/*
		* findAndMarkMin logically deletes the first undeleted node.
		* It traverses the bottom level and checks if the deleted field of curr node is true.
		* If so, then move to the next node. Else, atomically TAS the deleted field.
		*/
		skipListNode<X>* findAndMarkMin() 
		{
			skipListNode<X> *curr = NULL;
			skipListNode<X> *succ = NULL;
			curr = head->next[0].getReference();
			while(curr != tail)
			{
				if(!curr->deleted.load())
				{
					if (!curr->deleted.exchange(true))
					{
						return curr;
					}  
					else 
					{
						curr = curr->next[0].getReference();
					}
				}
				else 
				{
					curr = curr->next[0].getReference();
				}
			}
			return NULL;
		}

		/*
		* Remove marks all the predecessors and successors of priority
		* and calls findNode() to relink the nodes.
		*/
		bool remove(int priority) {
			int bottomLevel = 0;
			std::vector<skipListNode<X>* > preds(maxHeight + 1);
			std::vector<skipListNode<X>* > succs(maxHeight + 1);
			skipListNode<X>* succ;
			while(true)
			{
				bool found = findNode(priority, preds, succs);
				if (!found) {
					return false;
				}
				else
				{
					skipListNode<X>* nodeToRemove = succs[bottomLevel];
					for (int level = nodeToRemove->topLevel; level >= bottomLevel+1; level--) 
					{
						bool *mark = new bool(false);
						succ = nodeToRemove->next[level].get(mark);
						while (!mark[0]) 
						{
							nodeToRemove->next[level].CAS(succ, succ, false, true);
							succ = nodeToRemove->next[level].get(mark);
						}
					}
					bool *mark = new bool(false);
					succ = nodeToRemove->next[bottomLevel].get(mark);
					while(true)
					{
						bool iMarkedIt = nodeToRemove->next[bottomLevel].CAS(succ, succ, false, true);
						succ = succs[bottomLevel]->next[bottomLevel].get(mark);
						if (iMarkedIt)
						{
							findNode(priority, preds, succs);
							return true;
						}
						else if (mark[0])
						{
							return false;
						}
					}

				}
			}
		}

		/*
		* removeMin() returns the priority of the node deleted by findAndMarkMin().
		* It calls remove() to physically delete the node. 
		* In case of empty queue, it return INT_MIN.
		*/
		int removeMin() 
		{
			skipListNode<X>* node = findAndMarkMin();
			if (node != NULL) 
			{
				int prio = node->priority;
				remove(prio);
				return prio;
			}
			else
			{
				return INT_MIN;
			}
		}
};
