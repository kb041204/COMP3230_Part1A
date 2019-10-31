/* 2019-20 Programming Project
	part0-mandelbrot.c */

//Using SDL2 and standard IO
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"

typedef struct message {
	int row_index;
	float rowdata[IMAGE_WIDTH];
} MSG;

int child_terminated = 0; //global variable for parent
void sigchild_handler(int signum) {
	++child_terminated;
}

int main( int argc, char* args[] )
{
/*	int pfd[2*atoi(args[1])]; //pipe for message
	for(int i=0; i<atoi(args[1]); i++)
		pipe(&pfd[2*i]); //set pipe */
	int pfd[2];
	pipe(pfd);	


	if (argc != 2) { //not enough/too many arguments
		printf("Invalid argument!\n");
		printf("Usage: ./1amandel <number of child>\n");
		exit(0);
	}

	//data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	
	//data structure to store the start and end times of the computation
	struct timespec start_compute, end_compute;
	struct timespec child_start_compute, child_end_compute;

	//generate mandelbrot image and store each pixel for later display
	//each pixel is represented as a value in the range of [0,1]
	
	//store the 2D image as a linear array of pixels (in row-major format)
	float * pixels;
	
	//allocate memory to store the pixels
	pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
	if (pixels == NULL) {
		printf("Out of memory!!\n");
		exit(1);
	}
	
	//compute the mandelbrot image
	//keep track of the execution time - we are going to parallelize this part
	printf("Start the computation ...\n");
	clock_gettime(CLOCK_MONOTONIC, &start_compute);
    	int x, y;
	float difftime, child_difftime;
	
	int rows_to_complete = IMAGE_HEIGHT / atoi(args[1]);
	int first_row_index = rows_to_complete*-1;
	int last_row_index = 0;
	int pid;	
	int i;
	int * children = (int*)malloc(sizeof(int) * atoi(args[1]));

	for (i=0; i<atoi(args[1]); i++) { //create child
		first_row_index += rows_to_complete;
		if(i!=atoi(args[1])-1)
			last_row_index += rows_to_complete;
		else 
			last_row_index = IMAGE_HEIGHT; //handle not divisble
		pid = fork();
		if(pid == 0) //child
			break; 
		else //parent
			children[i] = pid; 
	}
	
	if(pid > 0) { //==============parent==============
		signal(SIGCHLD, sigchild_handler);
		close(pfd[1]); //close write end for parents
		printf("Start collecting the image lines\n");
		
/*		for(int j = 0; j < atoi(args[1]); j++) {
			//waitpid(children[j], NULL, 0);
			MSG * par_msg = malloc(sizeof(*par_msg));
			while(read(pfd[2*j+2], par_msg, sizeof(*par_msg)) != 0) {
				for(int k = 0; k < IMAGE_WIDTH; k++) {
					pixels[par_msg->row_index*IMAGE_WIDTH+k] = par_msg->rowdata[k];
				}
			}
		} */
		MSG * par_msg = malloc(sizeof(*par_msg));

		while(child_terminated != atoi(args[1]) &&
			read(pfd[0], par_msg, sizeof(*par_msg)) != 0) {
			for(int k = 0; k < IMAGE_WIDTH; k++) {
				pixels[par_msg->row_index*IMAGE_WIDTH+k] = par_msg->rowdata[k];
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &end_compute);
		difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
		printf(" ... completed. Elapse time = %.3f ms\n", difftime);

		//Report timing
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
		printf("Total elapse time measured by the process = %.3f ms\n", difftime);
	
		printf("Draw the image\n");
		//Draw the image by using the SDL2 library
		DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
	
		return 0;

	} else if (pid == 0) { //==============child==============
		//close(pfd[2*i]); //close read end for childs
		close(pfd[0]);
		
		// Computation
		printf("Child (%d): Start the computation...\n", (int)getpid());
		clock_gettime(CLOCK_MONOTONIC, &child_start_compute);
		for (y=first_row_index; y<last_row_index; y++) {
			MSG * curr_msg = malloc(sizeof(*curr_msg));
			curr_msg->row_index = y;
    			for (x=0; x<IMAGE_WIDTH; x++) {
				//compute a value for (x, y)
				curr_msg->rowdata[x] = Mandelbrot(x, y);
    			}
			//write(pfd[2*i+1], curr_msg, sizeof(*curr_msg));
			//printf("Child (%d): Send to pipe... (x=%d, y=%d)\n", (int)getpid(), x, y);
			write(pfd[1], curr_msg, sizeof(*curr_msg));
    		}
		clock_gettime(CLOCK_MONOTONIC, &child_end_compute);
		child_difftime = (child_end_compute.tv_nsec - child_start_compute.tv_nsec)/1000000.0 + (child_end_compute.tv_sec - child_start_compute.tv_sec)*1000.0;
		printf("Child (%d): ... completed. Elapsed time = %.3f ms\n", (int)getpid(), child_difftime);
		close(pfd[1]); //close write end of that child
		return 0;
	} else {
		printf("Fail to create child!\n");
	}
}

