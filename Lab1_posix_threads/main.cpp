#include <iostream>
#include <fstream>
#include <pthread.h>
#include <queue>
#include <csignal>
#include <cassert>

const char* path_to_input_file = "in.txt";

struct buffer_t {
  std::queue<int> data;
  int is_alive = 1;               
} buf; // Buffer 

int total = 0; // Total sum

pthread_mutex_t mutex;
pthread_barrier_t barrier; // Barrier for producer, consumers and interruptor
pthread_cond_t cond_data_rdy; // Producer has prepared data for consumers
pthread_cond_t cond_data_processed; // Consumer has processed the data
int can_produce = 0; // Prevent random OS unsleeping of the producer

int interruption_flag = 0; // SIGINT flag
int debug_flag = 0; // Show debug information

int PRODUCERS = 1;
int CONSUMERS = 10;
int INTERRUPTORS = 1;
int MAX_TIME = 1000; // Max possible time for sleep

// Read data, loop through each value and update the value, notify consumer, wait for consumer to process
void* producer_routine(void* arg) {
	buffer_t* buffer = (buffer_t*)arg;

	std::ifstream inp_file(path_to_input_file);
	int number;

	if (!inp_file) {
        std::cout << "Failed to open file: " << path_to_input_file << std::endl;
        exit(1);
    }

	pthread_barrier_wait(&barrier); // Wait for consumers and interruptor

   while ((inp_file.peek() != '\n') && (inp_file >> number)) {
    	pthread_mutex_lock(&mutex);

		if (debug_flag)
        	printf("\nProducer %li wants to produce number %i : Total contains %i", pthread_self(), number, total);

		buffer->data.push(number);

		if (debug_flag)
			printf("\nProducer %li produced number %i : Total contains %i", pthread_self(), buffer->data.front(), total);

        pthread_cond_signal(&cond_data_rdy); // Signal random waiting consumer

		if (interruption_flag){
			buffer->is_alive = 0;
			pthread_cond_broadcast(&cond_data_rdy);
			pthread_mutex_unlock(&mutex);
			if (debug_flag)
				std::cout << "Producer stopped" << std::endl;
			pthread_exit(NULL);
		} 

		while (!can_produce)
			pthread_cond_wait(&cond_data_processed, &mutex); // Wait for consumer to process

		if (interruption_flag){
			buffer->is_alive = 0;
			pthread_cond_broadcast(&cond_data_rdy);
			pthread_mutex_unlock(&mutex);
			if (debug_flag)
				std::cout << "Producer stopped" << std::endl;
			pthread_exit(NULL);
		}

		can_produce = 0;

        pthread_mutex_unlock(&mutex);
    }

	pthread_mutex_lock(&mutex);
	buffer->is_alive = 0;
	pthread_mutex_unlock(&mutex);

	pthread_cond_broadcast(&cond_data_rdy); // Notify all consumers of the termination.

    return nullptr;
}

// Cross-platform sleep function
void sleepcp(int milliseconds) {
    clock_t time_end;
    time_end = clock() + milliseconds * CLOCKS_PER_SEC/1000;
    while (clock() < time_end){
		if (interruption_flag)
			break;
    }
}

void* consumer_routine(void* arg) {
    // for every update issued by producer, read the value and add to sum
    // return pointer to result (for particular consumer)

	int result;	// Error detection

	result = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (result != 0)
	{
		perror("\nConsumer interruption protection is not set");
		exit(EXIT_FAILURE);
	}

	buffer_t* buffer = (buffer_t*)arg;
	int* partial_sum_ptr = new int(0); // Partial sum of thread

	pthread_barrier_wait(&barrier); // Wait for producer, other consumers and interruptor

	while (true){
		pthread_mutex_lock(&mutex); 

		if (interruption_flag){
			can_produce = 1;
			pthread_cond_broadcast(&cond_data_processed);
			pthread_mutex_unlock(&mutex);
			if (debug_flag)
				std::cout << "Consumer stopped" << std::endl;
			delete partial_sum_ptr;
			pthread_exit(NULL);
		} 

		while (buffer->data.empty() && buffer->is_alive)
	        pthread_cond_wait(&cond_data_rdy, &mutex); // Wait for producer to prepare data if queue hasn't over already

		if (interruption_flag){
			can_produce = 1;
			pthread_cond_broadcast(&cond_data_processed);
			pthread_mutex_unlock(&mutex);
			if (debug_flag)
				std::cout << "Consumer stopped" << std::endl;
			delete partial_sum_ptr;
			pthread_exit(NULL);
		} 

		if (!buffer->is_alive){
			pthread_mutex_unlock(&mutex);
			break;
		}

		if (debug_flag)
			printf("\nConsumer %li wants to sum number %i: Total contains %i", pthread_self(), buffer->data.front(), total);

		*partial_sum_ptr += buffer->data.front();
		total += buffer->data.front(); // Add data to total sum

		if (debug_flag){
			printf("\nConsumer %li summed number %i : Current partial sum %i : Total contains %i", pthread_self(), buffer->data.front(), *partial_sum_ptr, total);
			printf("\n");
		}

		buffer->data.pop();
		can_produce = 1;
		pthread_cond_signal(&cond_data_processed); // Signal random waiting producer

		pthread_mutex_unlock(&mutex);
		
		if (MAX_TIME > 0)
			sleepcp(rand()%MAX_TIME+1);
	}

	//pthread_exit(NULL);
    return (void*)partial_sum_ptr;
}

void* consumer_interruptor_routine(void* arg) {
	pthread_barrier_wait(&barrier); // Wait for producer and consumers

	pthread_t* consumer_arr = (pthread_t*)arg;

	while (buf.is_alive){
		if (interruption_flag){
			if (debug_flag)
				std::cout << "Interruptor stopped" << std::endl;
			pthread_exit(NULL);
		}
		pthread_cancel(consumer_arr[rand()%CONSUMERS]); // Try interrupt random consumer while producer is running 
	}                

    return nullptr;
}

// Enable flag if TERMINT is received
void intsig_handler(int signum) {
	if (debug_flag)
		std::cout << "Interrupt signal (" << signum << ") received.\n";
	interruption_flag = 1;
}

double run_threads() {
    // start N threads and wait until they're done
    // return aggregated sum of values

	int result;	// Error detection
	void* t_return;	// Thread return value
	pthread_t* producers = new pthread_t[PRODUCERS]; // Producer threads
	pthread_t* consumers = new pthread_t[CONSUMERS]; // Consumer threads
	pthread_t* interruptors = new pthread_t[INTERRUPTORS]; // Interruptor threads

    // Initialize mutex, barrier and condition variables
	result = pthread_mutex_init(&mutex, NULL);
	if (result != 0)
	{
		perror("\nMutex initialization failed");
		exit(EXIT_FAILURE);
	}

	result = pthread_barrier_init(&barrier, NULL, PRODUCERS + CONSUMERS + INTERRUPTORS);
	if (result != 0)
	{
		perror("\nBarrier initialization failed");
		exit(EXIT_FAILURE);
	}

	result = pthread_cond_init(&cond_data_rdy, NULL);
	if (result != 0)
	{
		perror("\nCondition variable initialization failed");
		exit(EXIT_FAILURE);
	}
	result = pthread_cond_init(&cond_data_processed, NULL);
	if (result != 0)
	{
		perror("\nCondition variable initialization failed");
		exit(EXIT_FAILURE);
	}

    // Create Producer and Consumers threads
	for (int i = 0; i < PRODUCERS; i++)
	{
	    result = pthread_create(&producers[i], NULL, producer_routine, (void*)&buf);
		if (result != 0)
		{
			perror("\nThread creation failed");
			exit(EXIT_FAILURE);
		}
		if (debug_flag)
			printf("\nProducer %i ready to produce", i);
	}

	for (int i = 0; i < CONSUMERS; i++)
	{
		result = pthread_create(&consumers[i], NULL, consumer_routine, (void*)&buf);
		if (result != 0)
		{
			perror("\nThread creation failed");
			exit(EXIT_FAILURE);
		}
		if (debug_flag)
			printf("\nConsumer %i ready to consume", i);
	}

	for (int i = 0; i < INTERRUPTORS; i++)
	{
		result = pthread_create(&interruptors[i], NULL, consumer_interruptor_routine, (void*)consumers);
		if (result != 0)
		{
			perror("\nThread creation failed");
			exit(EXIT_FAILURE);
		}
		if (debug_flag)
			printf("\nInterruptor %i ready to interrupt", i);
	}

	if (debug_flag)
		printf("\n");
	

    // Wait for threads to finish execution
	for (int i = 0; i < PRODUCERS; i++)
	{
		result = pthread_join(producers[i], &t_return);
		if (result != 0)
		{
			perror("\nProducer join failed");
			exit(EXIT_FAILURE);
		}
		if (debug_flag)
			std::cout << "\nProducer ended";
	}

	for (int i = 0; i < INTERRUPTORS; i++)
	{
		result = pthread_join(interruptors[i], &t_return);
		if (result != 0)
		{
			perror("\nInterruptor join failed");
			exit(EXIT_FAILURE);
		}
		if (debug_flag)
			std::cout << "\nInterruptor ended";
	}

	for (int i = 0; i < CONSUMERS; i++)
	{
		result = pthread_join(consumers[i], &t_return);
		if (result != 0)
		{
			perror("\nConsumer join failed");
			exit(EXIT_FAILURE);
		}
		if ((debug_flag) && (!interruption_flag))
			std::cout << "\nConsumer ended, final partial sum is: " << *((int*)(t_return));
		if (!interruption_flag)
			delete (int*)t_return;
	}

    // Clean-up mutex, barrier and condition variables
	pthread_mutex_destroy(&mutex);
	pthread_barrier_destroy(&barrier);
	pthread_cond_destroy(&cond_data_rdy);
	pthread_cond_destroy(&cond_data_processed);
	delete [] producers;
	delete [] consumers;
	delete [] interruptors;
	return total;
}

int main(int argc, char* argv[]) {
    //std::cout << run_threads() << std::endl;

	if (argc == 3){
		CONSUMERS = std::stoi(argv[1]);
		MAX_TIME = std::stoi(argv[2]);
	}
	else if (argc == 4){
		CONSUMERS = std::stoi(argv[1]);
		MAX_TIME = std::stoi(argv[2]);
		debug_flag = std::stoi(argv[3]);
	}
	else{
		std::cout << "Incorrect number of input arguments" << std::endl;
		assert(argc == 3 || argc == 4);
	}

	signal(SIGINT, intsig_handler); // Enable flag if interrupt signal is received
    run_threads();
	if (debug_flag){
		std::cout << "\nProgram ended";
		std::cout << "\nTotal: " << total << std::endl;
	}
	std::cout << total << std::endl;
    return 0;
}
