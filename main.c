#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define COL_SIZE 1001
#define ROW_SIZE 1000
#define SIZE (ROW_SIZE * COL_SIZE)

// Instruction Example user$./source c a.dat b.dat c.dat
// gcc hw2.c -o hw2 -lrt
int main(int argc, char* argv[])
{
	/* File Descriptor was Open */
	int fp_a = open(argv[2], O_RDONLY);
	int fp_b = open(argv[3], O_RDONLY);
	int fp_c = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fp_a == -1 || fp_b == -1 || fp_c == -1) {
		puts("File open fail");
		return -1;
	}

	time_t start, end;
	double duration = 0.0;

	int n = 0; // Array Size
	int i, j, k;
	i = j = k = 0;
	read(fp_a, &n, sizeof(int));
	float X[n];

	/* Variable initialize */
	pid_t pid; // process id

	if (atoi(argv[1]) < 1 || atoi(argv[1]) > 16) {
		puts("Process error");
		return -1;
	}

	/* Shared memory mapping */
	const char *name_a = "shm_a";
	int shm_fd_a = shm_open(name_a, O_CREAT | O_RDWR, 0666); // shm for array create
	ftruncate(shm_fd_a, sizeof(float)*SIZE); // set size
	float(*ptr)[COL_SIZE] = (float(*)[COL_SIZE])mmap(NULL, sizeof(float)*SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_a, 0); // memory mapping
	for (i = 0; i < ROW_SIZE; i++) // shared memory initialize
		for (j = 0; j < COL_SIZE; j++)
			ptr[i][j] = 0;

	const char *name_x = "shm_x";
	int shm_fd_x = shm_open(name_x, O_CREAT | O_RDWR, 0666); // shm for switch create
	ftruncate(shm_fd_x, sizeof(float)*n); // set size
	float *ptr_x = (float *)mmap(NULL, sizeof(float)*ROW_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_x, 0); // memory mapping
	for (i = 0; i < n; i++) // shared memory initialize
		ptr_x[i] = 0;

	const char *name_s = "shm_switch";
	int shm_fd_s = shm_open(name_s, O_CREAT | O_RDWR, 0666); // shm for switch create
	ftruncate(shm_fd_s, sizeof(int)*atoi(argv[1])); // set size
	int *ptr_s = (int *)mmap(NULL, sizeof(int)*(atoi(argv[1]) + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_s, 0); // memory mapping
	for (i = 0; i < atoi(argv[1]) + 1; i++) // shared memory initialize
		ptr_s[i] = 0;

	/* File to Shm Copy */
	for (i = 0; i< n; i++)
		for (j = 0; j< n; j++) {
			lseek(fp_a, sizeof(int) + sizeof(float)*(i*n + j), SEEK_SET);
			read(fp_a, &(ptr[i][j]), sizeof(float));
		}
	for (i = 0; i< n; i++) {
		lseek(fp_b, sizeof(int) + sizeof(float)*i, SEEK_SET);
		read(fp_b, &(ptr[i][n]), sizeof(float));
	}

	/* Process create */
	int rest_child = atoi(argv[1]);
	while (rest_child > 0)
	{
		if ((pid = fork()) < 0) {
			fprintf(stderr, "fork error\n");
			return -1;
		}
		if (pid == 0)
			break;
		rest_child--;
	}

	/* Set child process identifier */
	int c_index;
	int c_value[atoi(argv[1]) + 1]; // cumulative value
	for (i = 0; i < atoi(argv[1]); i++) // c_value initialize
		c_value[i] = 0;
	c_value[atoi(argv[1])] = n;
	if (pid == 0)
	{
		c_index = ptr_s[atoi(argv[1])];
		ptr_s[atoi(argv[1])] += 1;
	}

	/* each process block share */
	int block_share[atoi(argv[1])];
	for (i = 0; i < atoi(argv[1]); i++)
		block_share[i] = (n - 1) / atoi(argv[1]);
	for (i = 0; i < (n - 1) % atoi(argv[1]); i++)
		block_share[i] += 1;

	for (i = 0; i < atoi(argv[1]); i++)
		if (i == 0)
			c_value[i] = 0;
		else
			c_value[i] = block_share[i - 1] + c_value[i - 1];

	/* Parent process stand-by wait() */
	if (pid > 0) {
		int sum = 0;
		start = clock();
		while (1) // step 1
		{
			for (i = 0; i< atoi(argv[1]); i++)
				sum += ptr_s[i];
			if (sum == atoi(argv[1]))
				break;
			else
				sum = 0;
		}
		for (i = 0; i<atoi(argv[1]); i++) // switch reset
			ptr_s[i] = 0;
		sum = 0;

		while (1) // step 2
		{
			for (i = 0; i< atoi(argv[1]); i++)
				sum += ptr_s[i];
			if (sum == atoi(argv[1]))
				break;
			else
				sum = 0;
		}
		end = clock();
	}
	/* ---------------------------------------------------------------- */
	// child process identifier -> c_index
	// child process identifier Cumulative value -> c_value[]
	// child process block -> block_share[]

	/* child process Running */
	float c = 0;
	if (pid == 0)
	{
		/* Parallel gaussian elimination */
		for (i = 0; i<n; i++)
			for (j = c_value[c_index]; j<c_value[c_index + 1]; j++)
			{
				if (j>i)
				{
					c = ptr[j][i] / ptr[i][i];
					for (k = 0; k <= n; k++)
						ptr[j][k] -= c * ptr[i][k];
				}
			}
		ptr_x[n - 1] = ptr[n - 1][n] / ptr[n - 1][n - 1];

		ptr_s[c_index] = 1; // step 1. switch set
		while (1)
			if (ptr_s[c_index] == 0)
				break;

		/* Parallel Backward Substitution */
		for (i = n - 2; i >= c_value[c_index]; i--)
		{
			c = 0;
			for (j = i + 1; j <= n - 1; j++)
				c += ptr[i][j] * ptr_x[j];
			ptr_x[i] = (ptr[i][n] - c) / ptr[i][i];
		}

		ptr_s[c_index] = 1; // step 2. switch set
		while (1)
			if (ptr_s[c_index] == 0)
				break;
		exit(0); // child process kill
	}

	/* ---------------------------------------------------------------- */

	/* File Write -> c.dat */
	lseek(fp_c, 0, SEEK_SET);
	write(fp_c, &n, sizeof(int));
	for (i = 0; i< n; i++) {
		lseek(fp_c, sizeof(int) + sizeof(float)*i, SEEK_SET);
		write(fp_c, &(ptr_x[i]), sizeof(float));
	}

	printf("Operation time : %f sec\n", (double)(end - start) / CLOCKS_PER_SEC);
	close(fp_a);
	close(fp_b);
	close(fp_c);
	shmdt(ptr);
	shmdt(ptr_x);
	shmdt(ptr_s);
	return 0;
}