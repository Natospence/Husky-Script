#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//Signal handler for when child process terminates
void set_dead();

//Boolean tracking whether child is dead
int alive = 1;

//Boolean tracking whether each pipe has flushed since child's died
int unflushed = 0b111;

int main (int argc, char** argv, char** envp)
{

	//If we haven't been given at least a program and directory
	if(argc < 3)
	{
		write(1, "Not enough arguments\n", 21);
		return -1;
	}

	//Arguments to pass to the program
	//subtract name of hscript and subtract directory
	int passed_argument_count = argc - 2;

	//Get the name of directory, for ease of use
	char* directory = argv[argc - 1];

	//Make array for arguments, including null termination
	char* arguments[passed_argument_count + 1];

	//Copy each argument to the arguments array
	for(int i = 0; i < passed_argument_count; i++)
	{
		arguments[i] = argv[i + 1];
	}
	arguments[passed_argument_count] = NULL;

	//Establish the output directory
	
	//Attempt to make the directory
	int retval = mkdir(directory, 0777);	

	//If we fail, check the file
	if(retval < 0)
	{

		struct stat stat_buffer;
		stat(directory, &stat_buffer);

		if(!S_ISDIR(stat_buffer.st_mode))
		{
			write(1, "non-directory file exits with directory name. Bailing\n", 54);
			exit(-1);
		}
	}

	//Create the pipes
	
	int std_in_pipe[2];
	int std_out_pipe[2];
	int std_error_pipe[2];
	pipe(std_in_pipe);
	pipe(std_out_pipe);
	pipe(std_error_pipe);

	if(fork() == 0)
	{
		//Child

		//Close stdin pipe write
		close(std_in_pipe[1]);
		//Close stdout pipe read
		close(std_out_pipe[0]);
		//Close stderr pipe read
		close(std_error_pipe[0]);

		//Send stdout to the pipe
		dup2(std_out_pipe[1], 1);

		//Send stderr to the pipe
		dup2(std_error_pipe[1], 2);

		//Get stdin from the pipe
		dup2(std_in_pipe[0], 0);

		execve(arguments[0], arguments, envp);
		write(2, "Exec failed: ", 13);
		write(2, strerror(errno), strlen(strerror(errno)));
		write(2, "\n", 1);
		exit(-1);

	}
	else
	{
		//Parent

		//Handle child exit
		signal(SIGCHLD, set_dead);

		//Create the output directory

		chdir(directory);

		//Whack any old output files
		unlink("0");
		unlink("1");
		unlink("2");

		//Open the new output files
		int stdin_file = open("0", O_CREAT | O_RDWR, 0777);
		int stdout_file = open("1", O_CREAT | O_RDWR, 0777);
		int stderr_file = open("2", O_CREAT | O_RDWR, 0777);

		chdir("..");

		//Close pipe ends

		//Close stdin pipe read
		close(std_in_pipe[0]);
		//Close stdout pipe write
		close(std_out_pipe[1]);
		//Close stderr pipe write
		close(std_error_pipe[1]);

		//Set timeval to 0 to poll
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	

		//Continue until all pipes have been flushed and child has exited
		while(unflushed)
		{
			
			char read_buffer[1024];

			//Zero the read buffer
			memset(read_buffer, 0, sizeof read_buffer);

			fd_set rfds;

			//Reset the pipe set for select
			FD_ZERO(&rfds);

			FD_SET(0, &rfds); //Add stdin to the read set
			FD_SET(std_out_pipe[0], &rfds); //Add stdout pipe to the read set
			FD_SET(std_error_pipe[0], &rfds); //Add stderror pipe to the read set

			//Use select to poll the file descriptors
			select(std_error_pipe[0] + 1, &rfds, NULL, NULL, &tv);

				//Check to see if we've written anything to stdin
				if(FD_ISSET(0, &rfds))
				{
					//Read from stdin
					int readretval = read(0, read_buffer, 1023);
					//Write to the stdin file and the stdin pipe
					write(stdin_file, read_buffer, strlen(read_buffer));
					write(std_in_pipe[1], read_buffer, strlen(read_buffer));

					//If we read nothing and the child is dead, mark done
					if(!readretval && !alive)
						unflushed = unflushed & 0b011;

					//Zero the read buffer
					memset(read_buffer, 0, sizeof read_buffer);

				}
				else if(!alive)
				{
					unflushed = unflushed & 0b011;
				}

				//Check to see if there's data in the stdout pipe
				if(FD_ISSET(std_out_pipe[0], &rfds))
				{
					//Read from the stdout pipe
					int readretval = read(std_out_pipe[0], read_buffer, 1023);
					//Write to stdout and the stdout file
					write(1, read_buffer, strlen(read_buffer));
					write(stdout_file, read_buffer, strlen(read_buffer));

					//If we read nothing and the child is dead, mark done
					if(!readretval && !alive)
						unflushed = unflushed & 0b101;
					

					//Zero the read buffer
					memset(read_buffer, 0, sizeof read_buffer);
				}
				else if(!alive)
				{
					unflushed = unflushed & 0b101;
				}

				//Check to see if there's data in the stderr pipe
				if(FD_ISSET(std_error_pipe[0], &rfds))
				{
					//Read from the stderror pipe
					int readretval = read(std_error_pipe[0], read_buffer, 1023);
					//Write to stderror and the stderror file
					write(2, read_buffer, strlen(read_buffer));
					write(stderr_file, read_buffer, strlen(read_buffer));

					if(!readretval && !alive)
						unflushed = unflushed & 0b110;

					//Zero the read buffer
					memset(read_buffer, 0, sizeof read_buffer);
				}
				else if(!alive)
				{
					unflushed = unflushed & 0b110;
				}

		}

	}


	return 0;
}

//Handler function for child exits. Flip flag
void set_dead()
{
	alive = 0;
}

