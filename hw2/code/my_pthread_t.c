/* Copyright (C) Wuyang Zhang Feb 17 2017
	Operating System Project 2 
*/

#include <assert.h>
#include <string.h>
#include "my_pthread_t.h"

schedule_t scheduler;
static int counter = 0;
static my_pthread_mutex_t countLock;
static int CONTEXT_INTERRUPTION = 0;

Node* head;
Node* curr;

/*
*	Create a node for a thread
*/
Node* 
createNode(my_pthread_t thread){
	Node* node = (Node*) malloc(sizeof(Node));
	node->thread = thread;
	return node;
}

/*
*	Insert a thread into list
*/
void 
insertNode(Node* node){
	scheduler.total_thread++;
	if(head == NULL){
		node->thread._self_id = 0;
		head = node;
		head->prev = head;
		head->next = head;
	}else{
		node->thread._self_id = head->prev->thread._self_id + 1;
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		head->prev = node;
	}
}

/*
*	Remove a thread from the list based on its thread id
*/
void
removeNode(pid_t thread_id){
	scheduler.total_thread--;
	curr = head;
	while(curr != NULL){
		if(curr->thread._self_id == thread_id){
			free(curr->thread._ucontext_t.uc_stack.ss_sp);
			curr->prev->next = curr->next;
			curr->next->prev = curr->prev;
			free(curr);
		}
	}
}

/*
*	Return a thread based on its thread id
*/
static inline my_pthread_t*
findThread_id(pid_t thread_id){
		curr = head;
	while(curr != NULL){ //TODO:stuck into while loop with double linked list
		if(curr->thread._self_id == thread_id){
			return &curr->thread;
		}
		curr = curr->next;
	}
	return NULL;
}

/*
*	Scheduler test approach. loop all thread in queue accordingly
*/
static inline my_pthread_t*
findThread_robin(){
	if(scheduler.runningThread == scheduler.total_thread-1){
		return &head->next->thread;
	}else{
		return findThread_id(++scheduler.runningThread);
	}
}

/*
*	Scheduler test approach. Select the thread with the highest priority
*/
static inline my_pthread_t*
findThread_priority(){
	curr = head;
	int maxPriority = -1;
	pid_t priorityThread = 0;
	while(curr != NULL){
		if(curr->thread.priority > maxPriority){
			maxPriority = curr->thread.priority;
			curr->thread._self_id == priorityThread;
		}
		curr = curr->next;
	}
	return findThread_id(priorityThread);
}

/*
*	Create Thread
*/
int 
my_pthread_create(my_pthread_t* thread, pthread_attr_t* attr, 
					void*(*function)(void*),void* arg){
	assert(scheduler.total_thread != MAX_THREADS);
	assert(getcontext(&thread->_ucontext_t) != -1);
	thread->func = function;
	thread->arg = arg;
	thread->_ucontext_t.uc_stack.ss_sp = thread->stack;
	thread->_ucontext_t.uc_stack.ss_size = sizeof(MIN_STACK);
	thread->_ucontext_t.uc_stack.ss_flags = 0;
	thread->_ucontext_t.uc_flags = 0;
	thread->_ucontext_t.uc_link = &head->thread._ucontext_t;
	thread->state = READY;

	makecontext(&thread->_ucontext_t, function, 0);

	insertNode(createNode(*thread));

	return thread->_self_id;
}

/*
*	Yield thread 
*	Explicit call to end the pthread that the current context
*	be swapped out and another be scheduled
*/
void
my_pthread_yield(){
	printf("[PTHREAD] thread %d yield!\n", scheduler.runningThread);
	if(scheduler.total_thread > 1){
		my_pthread_t* thread = findThread_id(scheduler.runningThread);
		thread->state = READY;
		my_pthread_t temp;
		memcpy(&temp, thread, sizeof(thread));
		getcontext(&thread->_ucontext_t);
		thread->func = temp.func;
		thread->arg = temp.arg;
		thread->_ucontext_t.uc_stack.ss_sp = temp._ucontext_t.uc_stack.ss_sp;
		thread->_ucontext_t.uc_stack.ss_size = temp._ucontext_t.uc_stack.ss_size;
		thread->_ucontext_t.uc_stack.ss_flags = temp._ucontext_t.uc_flags;
		thread->_ucontext_t.uc_flags = temp._ucontext_t.uc_flags;
		thread->_ucontext_t.uc_link = temp._ucontext_t.uc_link;
		setcontext(&scheduler.schedule_thread._ucontext_t);		
	}
}

/*
*	Exit thread
*/
void
my_pthread_exit(void* value_ptr){
	pid_t thread_id = *((pid_t *) value_ptr);
	removeNode(thread_id);
}

/*
*	Call to the my_pthread_t library ensuring that the 
*	calling thread will not execute until the one it 
*	references exits. If value_ptr is not null, the return
*	value of the existing thread will be passed back
*	@para thread: 
*/
int
my_pthread_join(my_pthread_t thread, void**value_ptr){

	if(*value_ptr != NULL)
		return *((int*) value_ptr);
	return 0;
}

void 
schedule(){
	
	if(scheduler.total_thread > 1){
		my_pthread_t* currThread = findThread_robin();
		scheduler.runningThread = currThread->_self_id;
		currThread->state = RUNNING;
		printf("[Scheduler] switch to thread %d \n", currThread->_self_id);
		setcontext(&currThread->_ucontext_t);
	}
}

/*
*	To handler the signal, switch to schedule function
*	@para sig: signal value
*	@para context: old_context
*/
void 
signal_handler(int sig, siginfo_t *siginfo, void* context){

	if(sig == SIGPROF){
		printf("[SignalHandler] receive scheduler signal!\n");
		//store the context of running thread
		my_pthread_t* currThread = findThread_id(scheduler.runningThread);
		currThread->_ucontext_t.uc_mcontext = (*((ucontext_t*) context)).uc_mcontext;
		currThread->state = READY;
		/*
			check CONTEXT_INTERRUPTION.
			if == true, do not decrease the priority
			otherwise, change the priority
		*/
		if(CONTEXT_INTERRUPTION){
			CONTEXT_INTERRUPTION = 0;
		}else{
			currThread->priority -= 1;
		}
		//go to the context of scheduler
		setcontext(&scheduler.schedule_thread._ucontext_t);
	}
	
}

/*
*	Initiate scheduler, signal and timer
*/

void
start(){
	//init scheduler context;
	assert(getcontext(&scheduler.schedule_thread._ucontext_t) == 0);
	scheduler.schedule_thread._ucontext_t.uc_stack.ss_sp = scheduler.stack;
	scheduler.schedule_thread._ucontext_t.uc_stack.ss_size = sizeof(MIN_STACK);
	scheduler.schedule_thread._ucontext_t.uc_flags = 0;
	scheduler.schedule_thread._ucontext_t.uc_link = NULL;
	//block timer interruption
	sigset_t sysmodel;sysmodel;
	sigemptyset(&sysmodel);
	sigaddset(&sysmodel, SIGPROF);
	scheduler.schedule_thread._ucontext_t.uc_sigmask = sysmodel;
	makecontext(&scheduler.schedule_thread._ucontext_t, schedule, 0);
	insertNode(createNode(scheduler.schedule_thread));
	
	/*
		Init signal
		once timer signal is trigger, it enters into sigal_handler
	*/
	struct sigaction act;
	memset (&act, 0, sizeof(act));
	act.sa_sigaction = signal_handler;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	sigaction(SIGPROF,&act,NULL);
	
	/*
		Init timer
		it_value.tv_sec: first trigger delay
		it_interval.tv_sec: trigger interval
	*/
	struct itimerval tick;
	tick.it_value.tv_sec = 0;
	tick.it_value.tv_usec = TIME_QUANTUM;
	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = TIME_QUANTUM;
	assert(setitimer(ITIMER_PROF,&tick,NULL)!=1);

	//testlock
	my_pthread_mutex_init(&countLock, NULL);
}

/*
*	Init lock
*/
int
my_pthread_mutex_init(my_pthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr){
	mutex->flag = 0;
	return 0;
}

/*
*	Lock  lock
*/
int
my_pthread_mutex_lock(my_pthread_mutex_t* mutex){
	while(__sync_lock_test_and_set(&mutex->flag,1)){
		printf("[PTHREAD] thread %d spins in the lock\n",scheduler.runningThread);
		my_pthread_yield();
	}
	return 0;
}

/*
*	Unlock lock
*/
int 
my_pthread_mutex_unlock(my_pthread_mutex_t* mutex){
	__sync_lock_release(&mutex->flag);
	printf("[PTHREAD] thread %d unlock!\n",scheduler.runningThread);
	return 0;
}

/*
*	Destory lock
*/
int 
my_pthread_mutex_destory(my_pthread_mutex_t* mutex){
	return 0;
}

/*
*	Test function 1: loops permanently	
*/
void*
test(){
	printf("[TEST] thread %d is running\n", scheduler.runningThread);
	int a = scheduler.runningThread*1000000;
	while(1){
		if(a%1000000 == 0)
			printf("%d\n",a/1000000);
		a++;
	}
	return NULL;
}


/*
*	Test function 2: test lock
*/
static int sb = 0;
void*
test2(){
	printf("[TEST] thread %d is running\n", scheduler.runningThread);
	static int enterTime = 0;
	enterTime++;
	printf("enter time %d\n", enterTime);

	my_pthread_mutex_lock(&countLock); 
	while(enterTime < 3){
		//sb++;
		//printf("%d\n",sb);
	}
	my_pthread_mutex_unlock(&countLock); 

	return NULL;
}

void*
test3(){
	while(1){
		printf("33333\n");
	}
	return NULL;
}

int
main(){
	start();

	my_pthread_t thread;
	my_pthread_t thread2;
	my_pthread_t thread3;

	my_pthread_create(&thread,NULL,&test2,NULL);
	my_pthread_create(&thread2,NULL,&test2,NULL);
	my_pthread_create(&thread3,NULL,&test2,NULL);

	while(1){
		//
	}

	printf("exit program\n");
	return 0;
}
