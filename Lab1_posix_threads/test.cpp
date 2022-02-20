#include <pthread.h>
#include <iostream>
#include <fstream>
#include <unistd.h>

int data;
int counter=0;
bool flag_end = false; //data over
bool can_produce = false;
bool can_consume = false;

int PRODUCERS=1;
int CONSUMERS=3;
int INTERRUPTORS=1;

int TIME_SLEEP=100;

pthread_mutex_t m;
pthread_cond_t cond_can_produce;
pthread_cond_t cond_can_consume;
pthread_barrier_t barrier;

pthread_t* producers;
pthread_t* consumers;	
pthread_t* interruptors;

void* producer_routine(void* arg) {
  // Wait for consumer to start
 
  // Read data, loop through each value and update the value, notify consumer, wait for consumer to process
 	std::ifstream in_file("in.txt");
	pthread_barrier_wait(&barrier);

  	int tmp;
  	while ((in_file.peek()!='\n') && (in_file >> tmp)){
  		pthread_mutex_lock(&m);

  		data = tmp;
		can_consume = true;
  		pthread_cond_signal(&cond_can_consume);

		can_produce = false;
		while (!can_produce)
  			pthread_cond_wait(&cond_can_produce, &m);

  		pthread_mutex_unlock(&m);
  	}

	flag_end = true;
	pthread_cond_broadcast(&cond_can_consume);
	
	return nullptr;
 }
 
void* consumer_routine(void* arg) {
  // notify about start
  // for every update issued by producer, read the value and add to sum
  // return pointer to result (for particular consumer)
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	pthread_barrier_wait(&barrier);

	while (true){
		pthread_mutex_lock(&m);

		while (!can_consume && !flag_end)
			pthread_cond_wait(&cond_can_consume,&m);

		if (flag_end){
			pthread_mutex_unlock(&m);
		break;
		}

		counter += data;
		can_produce = true;
		pthread_cond_signal(&cond_can_produce);
		can_consume = false;

		pthread_mutex_unlock(&m);

		if (TIME_SLEEP>0)
			usleep((rand()%TIME_SLEEP+1)*1000);

	}

	pthread_cond_broadcast(&cond_can_produce);

	return nullptr;
}
 
void* consumer_interruptor_routine(void* arg) {
  // wait for consumers to start
	pthread_barrier_wait(&barrier);

	while (!flag_end){
		pthread_cancel(consumers[rand()%CONSUMERS]);
	}
 
  // interrupt random consumer while producer is running    

	return nullptr;
}
 
int run_threads() {
  // start N threads and wait until they're done
  // return aggregated sum of values

 	int i;		
	void * t_return;	
	
	producers = new pthread_t[PRODUCERS];
	consumers = new pthread_t[CONSUMERS];	
	interruptors = new pthread_t[INTERRUPTORS];	

	pthread_mutex_init(&m,NULL);
	pthread_cond_init(&cond_can_consume,NULL);
	pthread_cond_init(&cond_can_produce,NULL);
	pthread_barrier_init(&barrier, NULL, PRODUCERS + CONSUMERS + INTERRUPTORS);

	for(i=0;i<PRODUCERS;i++)
	{
		pthread_create(&producers[i],NULL,producer_routine,nullptr);
	}
	for(i=0;i<CONSUMERS;i++)
	{
		pthread_create(&consumers[i],NULL,consumer_routine,nullptr);
	}
	for(i=0;i<INTERRUPTORS;i++)
	{
		pthread_create(&interruptors[i],NULL,consumer_interruptor_routine,nullptr);
	}

	for(i=0;i<PRODUCERS;i++)
	{
		pthread_join(producers[i],&t_return);
	}
	for(i=0;i<CONSUMERS;i++)
	{
		pthread_join(consumers[i],&t_return);
	}
	for(i=0;i<INTERRUPTORS;i++)
	{
		pthread_join(interruptors[i],&t_return);
	}

	pthread_mutex_destroy(&m);
	pthread_cond_destroy(&cond_can_produce);
	pthread_cond_destroy(&cond_can_consume);
	pthread_barrier_destroy(&barrier);

	delete [] producers;
	delete [] consumers;
	delete [] interruptors;

	return counter;
}
 
int main(int argc, char* argv[]) {
	CONSUMERS = std::stoi(argv[1]);
	TIME_SLEEP = std::stoi(argv[2]);

    std::cout << run_threads() << std::endl;
    return 0;
}