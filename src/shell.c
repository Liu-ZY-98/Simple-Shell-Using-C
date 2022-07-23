#include "shell_util.h"
#include "linkedList.h"
#include "helpers.h"

// Library Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define DEF_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH

int flag = 0;
List_t bg_list;


void sigchld_handler(int signum)
{
	flag = 1;
}

void sigusr1_handler(int signum)
{
	int x;
	node_t *node = bg_list.head;

	for (x = 0; x < bg_list.length; x++)
	{
		printBGPEntry((ProcessEntry_t *)(node->value));
		node = node->next;
	}
}


int main(int argc, char *argv[])
{
	int i; //loop counter
	char *args[MAX_TOKENS + 1];
	int exec_result;
	int exit_status;
	pid_t pid;
	pid_t wait_result;

    //Initialize the linked list
    bg_list.head = NULL;
    bg_list.length = 0;
    bg_list.comparator = timeComparator;  // Don't forget to initialize this to your comparator!!!

	// Setup segmentation fault handler
	if(signal(SIGSEGV, sigsegv_handler) == SIG_ERR)
	{
		perror("Failed to set signal handler");
		exit(-1);
	}

	signal(SIGCHLD, sigchld_handler);
	signal(SIGUSR1, sigusr1_handler);

	while(1) {
		// DO NOT MODIFY buffer
		// The buffer is dynamically allocated, we need to free it at the end of the loop
		char * const buffer = NULL;
		size_t buf_size = 0;

		int bg = 0;

		int rd = 0;
		int infd = 0;
		int outfd = 0;
		int errfd = 0;

		int pip = 0;
		int pipefd[2];
		char *pipe1[40];
		char *pipe2[40];

		// Print the shell prompt
		display_shell_prompt();
		
		// Read line from STDIN
		ssize_t nbytes = getline((char **)&buffer, &buf_size, stdin);

		// No more input from STDIN, free buffer and terminate
		if(nbytes == -1) {
			free(buffer);
			break;
		}

		// Remove newline character from buffer, if it's there
		if(buffer[nbytes - 1] == '\n')
			buffer[nbytes- 1] = '\0';

		// Handling empty strings
		if(strcmp(buffer, "") == 0) {
			free(buffer);
			continue;
		}
		
		// Parsing input string into a sequence of tokens
		size_t numTokens;
		*args = NULL;
		char* cmd = malloc(500);
		strcpy(cmd, buffer);
		numTokens = tokenizer(buffer, args);
		
		if(strcmp(args[0],"exit") == 0) {
			// Terminating the shell

			int i;
			node_t *node = bg_list.head;
			for (i = 0; i < bg_list.length; i++)
			{
				printf(BG_TERM, ((ProcessEntry_t *)(node->value))->pid, 
						((ProcessEntry_t *)(node->value))->cmd);
				kill(((ProcessEntry_t *)(node->value))->pid, SIGKILL);
				node = node->next;
			}
			free(buffer);
			return 0;
		}

		if (strcmp(args[0], "cd") == 0)
		{
			char* cDir = malloc(200);
			if (numTokens == 1)
			{
				cDir = getenv("HOME");
				chdir(cDir);
				printf("%s\n", cDir);
				continue;
			}

			else if (chdir(args[1]) != 0)
			{
				fprintf(stderr, "%s", DIR_ERR);
			}
			else
			{
				getcwd(cDir, 200);
				printf("%s\n", cDir);
			}
			free(cDir);
			continue;
		}

		if (strcmp(args[0], "estatus") == 0)
		{
			int estatus = 0;
			wait(&exit_status);
			if (WIFEXITED(exit_status))
				estatus = WEXITSTATUS(exit_status);
			printf("%d\n", estatus);
			continue;
		}

		if ((strcmp(args[0], ">") == 0) || (strcmp(args[0], ">>") == 0) || 
				(strcmp(args[0], "<") == 0) || (strcmp(args[0], "2>") == 0))
		{
			fprintf(stderr, "%s", RD_ERR);
			continue;
		}

		if ((strcmp(args[0], "|") == 0) || (strcmp(args[numTokens-1], "|") == 0) ||
				((strcmp(args[numTokens-1], "&") == 0) && (strcmp(args[numTokens-2], "|") == 0)))
		{
			fprintf(stderr, "%s", PIPE_ERR);
			continue;
		}

		if (numTokens > 1)
		{
			int i;
			int out = 0;
			int in = 0;
			int err = 0;
			int invalid = 0;
			
			int outindex = 0;
			int inindex = 0;
			int errindex = 0;

			for (i = 1; i < numTokens; i++)
			{
				if ((strcmp(args[i], ">") == 0) || (strcmp(args[i], ">>") == 0) || 
							(strcmp(args[i], "<") == 0) || (strcmp(args[i], "2>") == 0))
				{
					if (numTokens == i + 1)
					{
						fprintf(stderr, "%s", RD_ERR);
						invalid = 1;
						break;
					}
					else if ((strcmp(args[i+1], ">") == 0) || (strcmp(args[i+1], ">>") == 0) || 
							(strcmp(args[i+1], "<") == 0) || (strcmp(args[i+1], "2>") == 0))
					{
						fprintf(stderr, "%s", RD_ERR);
						invalid = 1;
						break;
					}
					else if ((numTokens > i + 2) && (strcmp(args[i+2], ">") != 0) && 
							(strcmp(args[i+2], ">>") != 0) && (strcmp(args[i+2], "<") != 0) &&
							(strcmp(args[i+2], "2>") != 0) && (strcmp(args[i+2], "&") != 0))
					{
						fprintf(stderr, "%s", RD_ERR);
						invalid = 1;
						break;
					}

					else
					{
						if (strcmp(args[i], ">") == 0)
						{
							if (out == 0)
							{
								outfd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC,
										DEF_MODE);
								out = 1;
								outindex = i + 1;
							}
							else
							{
								fprintf(stderr, "%s", RD_ERR);
								invalid = 1;
								break;
							}
						}
						else if (strcmp(args[i], ">>") == 0)
						{
							if (out == 0)
							{
								outfd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND,
										DEF_MODE);
								out = 1;
								outindex = i + 1;
							}
							else
							{
								fprintf(stderr, "%s", RD_ERR);
								invalid = 1;
								break;
							}
						}
						else if (strcmp(args[i], "<") == 0)
						{
							if (in == 0)
							{
								infd = open(args[i+1], O_RDONLY, DEF_MODE);
								in = 1;
								inindex = i + 1;
							}
							else
							{
								fprintf(stderr, "%s", RD_ERR);
								invalid = 1;
								break;
							}
						}
						else if (strcmp(args[i], "2>") == 0)
						{
							if (err == 0)
							{
								errfd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC,
										DEF_MODE);
								err = 1;
								errindex = i + 1;
							}
							else
							{
								fprintf(stderr, "%s", RD_ERR);
								invalid = 1;
								break;
							}
						}
						if ((outfd == -1) || (infd == -1) || (errfd == -1))
						{
							fprintf(stderr, "%s", RD_ERR);
							invalid = 1;
							break;
						}
					}
				}
			}
			if (in && out)
			{
				if (strcmp(args[inindex], args[outindex]) == 0)
					invalid = 1;
			}
			if (out && err)
			{
				if (strcmp(args[outindex], args[errindex]) == 0)
					invalid = 1;
			}
			if (in && err)
			{
				if (strcmp(args[inindex], args[errindex]) == 0)
					invalid = 1;
			}
			if (invalid)
			{
				if (outfd > 0)
					close(outfd);
				if (infd > 0)
					close(infd);
				if (errfd > 0)
					close(errfd);
				continue;
			}
			if (in)
			{
				args[inindex-1] = NULL;
				rd = 1;
			}
			if (out)
			{
				args[outindex-1] = NULL;
				rd = 1;
			}
			if (err)
			{
				args[errindex-1] = NULL;
				rd = 1;
			}
		}

		if (flag)
		{
			pid_t term;
			int status;
			while ((term = waitpid(-1, &status, WNOHANG)) > 0)
			{
				int i;
				node_t *node = bg_list.head;
				for (i = 0; i < bg_list.length; i++)
				{
					if (term == ((ProcessEntry_t *)(node->value))->pid)
					{
						int s;
						waitpid(term, &s, 0);
						printf(BG_TERM, term, ((ProcessEntry_t *)(node->value))->cmd);
						removeByPid(&bg_list, term);
						break;
					}
					else
					{
						node = node->next;
					}
				}
			}
			flag = 0;
		}

		if (numTokens > 2 && rd == 0)
		{
			int i;
			int pipeindex = 0;
			int p = 0;

			int invalid = 0;
			for (i = 1; i < numTokens; i++)
			{
				if (strcmp(args[i], "|") == 0)
				{
					if (p == 0)
					{
						pipeindex = i;
						p = 1;
					}
					else
					{
						fprintf(stderr, "%s", PIPE_ERR);
						invalid = 1;
						break;
					}
				}
			}
			if (invalid)
				continue;
			else if (!invalid && p == 1)
			{
				pip = 1;
				pipe(pipefd);
				int a;
				for (a = 0; a < pipeindex; a++)
				{
					pipe1[a] = malloc(strlen(args[a])+1);
					strcpy(pipe1[a], args[a]);
				}
				pipe1[pipeindex] = NULL;
				if (strcmp(args[numTokens-1], "&") != 0)
				{
					for (a = pipeindex+1; a < numTokens; a++)
					{
						pipe2[a-pipeindex-1] = malloc(strlen(args[a])+1);
						strcpy(pipe2[a-pipeindex-1], args[a]);
					}
					pipe2[numTokens-pipeindex-1] = NULL;
				}
				else
				{
					for (a = pipeindex+1; a < numTokens-1; a++)
					{
						pipe2[a-pipeindex-1] = malloc(strlen(args[a])+1);
						strcpy(pipe2[a-pipeindex-1], args[a]);
					}
					pipe2[numTokens-pipeindex-2] = NULL;
				}
			}
		}

		pid = fork();   //In need of error handling......
	
		if (strcmp(args[numTokens-1], "&") == 0)
		{
			args[numTokens-1] = NULL;
			ProcessEntry_t *bpro = malloc(sizeof(ProcessEntry_t));
			bpro->cmd = malloc(500);
			strcpy(bpro->cmd, cmd);
			bpro->pid = pid;
			bpro->seconds = time(NULL);
			insertInOrder(&bg_list, (void *)bpro);
			bg = 1;
		}
	
		if (pid == 0){ //If zero, then it's the child process
			if (infd > 0)
				dup2(infd, STDIN_FILENO);
			if (outfd > 0)
				dup2(outfd, STDOUT_FILENO);
			if (errfd > 0)
				dup2(errfd, STDERR_FILENO);
			if (pip)
			{
				close(pipefd[0]);
				dup2(pipefd[1], STDOUT_FILENO);
				int result = execvp(pipe1[0], &pipe1[0]);
				if (result == -1)
				{
					close(pipefd[1]);
					printf(EXEC_ERR, pipe1[0]);
					exit(EXIT_FAILURE);
				}
				close(pipefd[1]);
			}
			if (!pip)
			{
				exec_result = execvp(args[0], &args[0]);
				if(exec_result == -1){ //Error checking
					printf(EXEC_ERR, args[0]);
					exit(EXIT_FAILURE);
				}
			}
		    exit(EXIT_SUCCESS);
		}

		pid_t pipepid;
		if (pip)
			pipepid = fork();
		if (pip && pipepid == 0)
		{
			close(pipefd[1]);
			dup2(pipefd[0], STDIN_FILENO);
			int r = execvp(pipe2[0], &pipe2[0]);
			if (r == -1)
			{
				close(pipefd[0]);
				printf(EXEC_ERR, pipe2[0]);
				exit(EXIT_FAILURE);
			}
			close(pipefd[0]);
			exit(EXIT_SUCCESS);
		}

		else{ // Parent Process
			if (!bg)
			{
				wait_result = waitpid(pid, &exit_status, 0);
				if(wait_result == -1)
				{
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}
				if (pip)
				{
					close(pipefd[0]);
					close(pipefd[1]);
					int ps;
					pid_t pipe_result = waitpid(pipepid, &ps, 0);
				}
			}
		}

		// Free the buffer allocated from getline
		free(buffer);
	}
	return 0;
}


