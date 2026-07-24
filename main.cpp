// #include <iostream>
// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <cstring>
// #include <fcntl.h>

// int main()
// {

// 	int fd[2];
// 	if (pipe(fd) == -1)
// 	{
// 		std::cerr << "Pipe creation failed" << std::endl;
// 		return 1;
// 	}

// 	pid_t pid = fork();
// 	if (pid == -1)
// 	{
// 		std::cerr << "Fork failed" << std::endl;
// 		return 1;
// 	}
// 	else if (pid == 0)
// 	{
// 		dup2(fd[0], STDIN_FILENO);

// 		close(fd[1]);
// 		close(fd[0]);

// 		char cat_path[] = "/bin/cat";
// 		char *args[] = {cat_path, NULL};
// 		execve(args[0], args, NULL);
// 	}
// 	else
// 	{                                           
//     fcntl(fd[1], F_SETFL, O_NONBLOCK);
//     fcntl(fd[0], F_SETFL, O_NONBLOCK);
// 		const char *message = "Hello from parent!";
// 		write(fd[1], message, strlen(message) + 1);
// 		close(fd[0]);
// 		// close(fd[1]);
// 		waitpid(pid, NULL, 0);
// 	}
// 	std::cout << "Hello, World!" << std::endl;
// 	sleep(10);
// 	return 0;
// }

#include <iostream>
int main ()
{
	std::string str = "Hello, World!sdlslkdf;sljoiwe oijerijwe[orjie [cjqir c[qr [rjqe [jq ejr 	ci v v	rweeeex]]]]]";
	// str.~basic_string();
	str = "New String";
	std::cout << str << std::endl;
	return 0;
}
