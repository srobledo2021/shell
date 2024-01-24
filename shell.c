#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>

enum {
	PATH = 1024,
	LINE_SIZE = 1024,
};

struct datos {
	char *out;		//fd out
	char *in;		//fd in
	int bg;			//background
	int n_jobs;
	int here_doc;		//{HERE
};
typedef struct datos datos;
datos *data;

/*used for failure*/
void
print_error(char *message)
{
	fprintf(stderr, "%s\n", message);
	exit(EXIT_FAILURE);
}

// Function to print Current Directory.
void
printPrompt()
{
	char cwd[PATH];

	getcwd(cwd, sizeof(cwd));
	printf("%s $ ", cwd);
}

char *
create_path_bin(char *cmd)
{
	char *path = malloc(100);

	strcpy(path, "/bin/");
	strcat(path, cmd);
	return path;
}

char *
create_path_usr_bin(char *cmd)
{
	char *path = malloc(100);

	strcpy(path, "/usr/bin/");
	strcat(path, cmd);
	return path;
}

int
new_pipe(int *fd)
{
	if (pipe(fd) < 0) {
		perror("Error al crear el pipe");
		exit(1);
	}
	return 0;
}

int
execute_bin_usrbin(char *path_bin, char *path_usr_bin, char **cmd)
{
	execv(path_bin, cmd);
	execv(path_usr_bin, cmd);
	perror("Error al ejecutar el comando");
	return (EXIT_FAILURE);
}

// Ejecución de comandos con pipes
int
execute_cmd(char ***cmds)
{

	// numero de comandos
	int n_commands = 0;

	while (cmds[n_commands] != NULL) {
		n_commands++;
	}
	// Definir pipes
	int pipes[n_commands - 1][2];

	for (int i = 0; i < n_commands - 1; i++) {
		new_pipe(pipes[i]);
	}

	// Hijos
	int pids[n_commands];

	for (int i = 0; i < n_commands; i++) {
		pids[i] = fork();
		if (pids[i] < 0) {
			perror("No se pudo crear proceso hijo");
			exit(1);
		} else if (pids[i] == 0) {
			if (n_commands == 1) {
				if (data->in != NULL) {
					int fd = open(data->in, O_RDONLY);

					if (fd < 0) {
						perror
						    ("No se puede abrir el archivo de entrada");
						exit(1);
					}
					dup2(fd, STDIN_FILENO);
					close(fd);
				}
				// En background, se redirecciona la entrada a /dev/null
				else if (data->bg == 1) {
					int fd = open("/dev/null", O_RDONLY);

					if (fd < 0) {
						perror
						    ("No se puede abrir el archivo de entrada");
						exit(1);
					}
					dup2(fd, STDIN_FILENO);
					close(fd);
				}
				if (data->out != NULL) {
					int fd = open(data->out,
						      O_WRONLY | O_CREAT |
						      O_TRUNC,
						      0666);

					if (fd < 0) {
						perror
						    ("No se puede abrir el archivo de salida");
						exit(1);
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
				}
				// Si data->here_doc es 1, seguir leyendo de la entrada estandar 
				//hasta que se ingrese }, guardar en un pipe y redireccionar la entrada a ese pipe
				else if (data->here_doc == 1) {
					char *line = NULL;
					size_t len = 0;
					ssize_t nread;
					int fd[2];

					new_pipe(fd);
					while ((nread =
						getline(&line, &len,
							stdin)) != -1) {
						if (strcmp(line, "}\n") == 0) {
							break;
						}
						write(fd[1], line,
						      strlen(line));
					}
					dup2(fd[0], STDIN_FILENO);
					close(fd[0]);
					close(fd[1]);
				}
			} else {
				// Si data->here_doc es 1, seguir leyendo de la entrada estandar hasta que se ingrese }, guardar en un pipe y redireccionar la entrada a ese pipe
				if (data->here_doc == 1 && i == 0) {
					char *line = NULL;
					size_t len = 0;
					ssize_t nread;
					int fd[2];

					new_pipe(fd);
					while ((nread =
						getline(&line, &len,
							stdin)) != -1) {
						if (strcmp(line, "}\n") == 0) {
							break;
						}
						write(fd[1], line,
						      strlen(line));
					}
					dup2(fd[0], STDIN_FILENO);
					close(fd[0]);
					close(fd[1]);
					dup2(pipes[i][1], STDOUT_FILENO);
					close(pipes[i][0]);
					close(pipes[i][1]);
				} else if (i == 0 && n_commands > 2) {
					dup2(pipes[i][1], STDOUT_FILENO);
					close(pipes[i][0]);
					close(pipes[i][1]);
					// Si se ejecuta en background se redirecciona a /dev/null
					if (data->bg == 1) {
						int fd =
						    open("/dev/null", O_RDONLY);
						if (fd < 0) {
							perror
							    ("No se puede abrir el archivo de entrada");
							exit(1);
						}
						dup2(fd, STDIN_FILENO);
						close(fd);
					}
				} else if (i == 0 && n_commands == 2) {
					if (data->in != NULL) {
						int fd =
						    open(data->in, O_RDONLY);
						if (fd < 0) {
							perror
							    ("No se puede abrir el archivo de entrada");
							exit(1);
						}
						dup2(fd, STDIN_FILENO);
						close(fd);
					}
					// Si se ejecuta en background se redirecciona a /dev/null
					else if (data->bg == 1) {
						int fd =
						    open("/dev/null", O_RDONLY);
						if (fd < 0) {
							perror
							    ("No se puede abrir el archivo de entrada");
							exit(1);
						}
						dup2(fd, STDIN_FILENO);
						close(fd);
					}
					dup2(pipes[i][1], STDOUT_FILENO);
					close(pipes[i][0]);
					close(pipes[i][1]);
				} else if (i == n_commands - 2) {
					if (data->in != NULL) {
						int fd =
						    open(data->in, O_RDONLY);
						if (fd < 0) {
							perror
							    ("No se puede abrir el archivo de entrada");
							exit(1);
						}
						dup2(fd, STDIN_FILENO);
						close(fd);
					} else {
						dup2(pipes[i - 1][0],
						     STDIN_FILENO);
					}
					close(pipes[i - 1][0]);
					close(pipes[i - 1][1]);
					dup2(pipes[i][1], STDOUT_FILENO);
					close(pipes[i][0]);
					close(pipes[i][1]);
				} else if (i == n_commands - 1) {
					if (data->out != NULL) {
						int fd = open(data->out,
							      O_WRONLY | O_TRUNC
							      | O_CREAT, 0666);
						if (fd < 0) {
							perror
							    ("No se puede abrir el archivo de salida");
							exit(1);
						}
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}
					dup2(pipes[i - 1][0], STDIN_FILENO);
					close(pipes[i - 1][0]);
					close(pipes[i - 1][1]);
				} else {
					dup2(pipes[i - 1][0], STDIN_FILENO);
					close(pipes[i - 1][0]);
					close(pipes[i - 1][1]);
					dup2(pipes[i][1], STDOUT_FILENO);
					close(pipes[i][0]);
					close(pipes[i][1]);
				}
			}

			// Cerrar los descriptores de archivo no utilizados por el proceso hijo
			for (int j = 0; j < n_commands - 1; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			char *result = getenv("result");

			// Si el primer argumento es ifok, ejecutar el comando solo si el resultado de la linea anterior fue 0
			if (strcmp(cmds[i][0], "ifok") == 0) {
				if (result != NULL && strcmp(result, "0") == 0) {
					char *path_bin =
					    create_path_bin(cmds[i][1]);
					char *path_usr_bin =
					    create_path_usr_bin(cmds[i][1]);

					if (execute_bin_usrbin
					    (path_bin, path_usr_bin,
					     cmds[i] + 1) == EXIT_FAILURE) {
						return EXIT_FAILURE;
					}
				}
			}
			// Si el primer argumento es ifnot, ejecutar el comando solo si el resultado de la linea anterior fue distinto de 0
			else if (strcmp(cmds[i][0], "ifnot") == 0) {
				if (result == NULL || strcmp(result, "0") != 0) {
					char *path_bin =
					    create_path_bin(cmds[i][1]);
					char *path_usr_bin =
					    create_path_usr_bin(cmds[i][1]);

					if (execute_bin_usrbin
					    (path_bin, path_usr_bin,
					     cmds[i] + 1) == EXIT_FAILURE) {
						return EXIT_FAILURE;
					}
				}
			}
			// Sino ejecutar normalmente
			else {
				char *path_bin = create_path_bin(cmds[i][0]);
				char *path_usr_bin =
				    create_path_usr_bin(cmds[i][0]);

				int st =
				    execute_bin_usrbin(path_bin, path_usr_bin,
						       cmds[i]);

				// Liberar memoria
				free(path_bin);
				free(path_usr_bin);
				if (st == EXIT_FAILURE) {
					return EXIT_FAILURE;
				}
			}
		}
	}
	// Cerrar pipes en el padre
	for (int i = 0; i < n_commands - 1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	// Esperar a que termine el último proceso
	if (data->bg == 0) {
		waitpid(pids[n_commands - 1], NULL, 0);
	} else {
		// Si es en background, sumar 1 al número de jobs y no esperar a que termine el ultimo proceso
		data->n_jobs++;
		waitpid(pids[n_commands - 1], NULL, WNOHANG);	// WNOHANG(=1) sirve para que el proceso padre no se bloquee y pueda seguir ejecutando comandos
		// Imprimir mensaje en background
		printf("[%d] %d\n", data->n_jobs, pids[n_commands - 1]);
	}
	return 0;
}

// Eliminar espacios y tabulaciones
char *
cut(char *str)
{
	char *end;

	//Eliminar al inicio
	while (*str == ' ' || *str == '\t') {
		str++;
	}
	if (*str == 0) {
		return str;
	}
	end = str + strlen(str) - 1;

	//Eliminar al final
	while (end > str && (*end == ' ' || *end == '\t')) {
		end--;
	}
	*(end + 1) = 0;
	return str;
}

char ***
tokenize(char *line)
{
	data = malloc(sizeof(datos));
	if (line == NULL) {
		return NULL;
	}
	// Comprobar si se escribe "HERE{" al final
	if (strstr(line, "HERE{") != NULL) {
		//here_doc a 1 y cabiamos "HERE{" por espacio
		data->here_doc = 1;
		*strstr(line, "HERE{") = '\0';
	} else {
		data->here_doc = 0;
	}
	// Comprobar si se tiene que ejecutar en background
	if (strrchr(line, '&') != NULL) {
		//bg a 1 y cambiamos el caracter por un espacio
		data->bg = 1;
		*strrchr(line, '&') = '\0';
	} else {
		data->bg = 0;
	}

	// Comprobar cualquier tipo de redirección
	char *out = strrchr(line, '>');
	char *in = strrchr(line, '<');

	if (out != NULL) {
		data->out = out + 1;
		*out = '\0';
	} else {
		data->out = NULL;
	}
	if (in != NULL) {
		data->in = in + 1;
		*in = '\0';
	} else {
		data->in = NULL;
	}

	data->n_jobs = 0;
	// resto de linea tokenizada(PIPES)
	char ***matrix = malloc(LINE_SIZE * sizeof(char **));	// Matriz de comandos(pipes)
	char **tokens = malloc(LINE_SIZE * sizeof(char *));	// Array de tokens(espacios en blanco y tabulaciones)
	char *token = strtok(line, "|");

	// Guardar comandos en la matriz que hemos creado antes
	int i = 0;

	while (token != NULL) {
		tokens[i] = token;
		token = strtok(NULL, "|");
		i++;
	}
	tokens[i] = NULL;

	// suprimir espacios y tabulaciones
	if (data->out != NULL) {
		data->out = cut(data->out);
	}
	if (data->in != NULL) {
		data->in = cut(data->in);
	}
	for (int a = 0; tokens[a] != NULL; a++) {
		tokens[a] = cut(tokens[a]);
	}

	// Tokenizar la cadena
	for (int k = 0; tokens[k] != NULL; k++) {
		char **args = malloc(LINE_SIZE * sizeof(char *));	//Array tokenizado
		char *arg = strtok(tokens[k], " \t");
		int j = 0;
		int in_quotes = 0;	// ¿entre comillas?
		char quoted_arg[LINE_SIZE] = { 0 };	//argumento entre comillas
		while (arg != NULL) {
			if (arg[0] == '"') {
				in_quotes = 1;
				strcpy(quoted_arg, arg + 1);
			} else if (arg[strlen(arg) - 1] == '"') {	// Termina por comillas
				strcat(quoted_arg, " ");
				strcat(quoted_arg, arg);
				quoted_arg[strlen(quoted_arg) - 1] = '\0';
				args[j] = strdup(quoted_arg);	// añadir argumento al array
				j++;
				in_quotes = 0;
			} else if (in_quotes) {	// argumento entre comillas
				strcat(quoted_arg, " ");
				strcat(quoted_arg, arg);
			} else {
				args[j] = arg;
				j++;
			}
			arg = strtok(NULL, " \t");
		}
		args[j] = NULL;
		matrix[k] = args;
	}
	matrix[i] = NULL;
	free(tokens);
	return matrix;
}

// Establecer variables de entorno
void
setenv_var(char *args, char *value)
{
	if (args == NULL || value == NULL) {
		printf("Error: se esperaban dos argumentos\n");
		return;
	}
	if (setenv(args, value, 1) == -1) {
		perror("No se pudo crear la variable de entorno");
	}
}

// Expandir variables de entorno
int
expand_var(char ***tokens_args)
{
	for (int i = 0; tokens_args[i] != NULL; i++) {
		for (int j = 0; tokens_args[i][j] != NULL; j++) {
			if (tokens_args[i][j][0] == '$') {
				char *env_var = getenv(tokens_args[i][j] + 1);

				if (env_var == NULL) {
					printf("Error: var %s does not exist\n",
					       tokens_args[i][j] + 1);
					return -1;
				} else {
					tokens_args[i][j] = env_var;
				}
			}
		}
	}
	return 0;
}

void
free_all(char ***args, char *line)
{
	int i = 0;

	while (args[i] != NULL) {
		free(args[i]);
		i++;
	}
	free(args);
	free(line);
	free(data);
}

int
main(int argc, char *argv[])
{
	//input para nuestra shell
	char *line;

	//matriz de argumentos
	char ***args;

	//variable de entorno "result" que contiene el 
	//estatus devuelto por el último comando
	setenv_var("result", "0");

	while (!feof(stdin)) {
		//imprimir prompt
		printPrompt();
		//reservamos memoria y leemos la entrada(1024)
		line = malloc(LINE_SIZE * sizeof(char));
		if (fgets(line, LINE_SIZE, stdin) == NULL) {
			fprintf(stderr, "Error de lectura\n");
			continue;
		}
		if (line[strlen(line) - 1] != '\n') {
			printf("No pueden ingresarse mas de 1024 caracteres\n");
			while (fgetc(stdin) != '\n') ;
			continue;
		}
		// Quitamos salto de línea
		line[strcspn(line, "\n")] = '\0';

		// Tokeniza la linea y comprueba si hay '&', pipes, '>',...
		// devuelve una matriz de arrays de strings
		args = tokenize(line);

		//Si no se introduce nada, se pregunta de nuevo
		if (args[0] == NULL) {
			continue;
		}
		//-------------------------------------------------------------------
		//exit
		if (strcmp(args[0][0], "exit") == 0) {
			free_all(args, line);
			break;
		}
		//cd
		else if (strcmp(args[0][0], "cd") == 0) {
			if (args[0][1] == NULL) {
				chdir(getenv("HOME"));
			} else {
				if (chdir(args[0][1]) == -1) {
					perror
					    ("No se pudo cambiar de directorio");
				}
			}
			free_all(args, line);
			continue;
		}
		//-------------------------------------------------------------------

		// Crear una variable de entorno al encontrar "="
		else if (strchr(args[0][0], '=') != NULL) {
			char *args_copy = malloc(strlen(args[0][0]) + 1);

			strcpy(args_copy, args[0][0]);
			char *arg = strtok(args_copy, "=");
			char *value = strtok(NULL, "=");

			setenv_var(arg, value);
			free(args_copy);
			free_all(args, line);
			continue;
		}
		// Expandir variables de entorno
		if (expand_var(args) == -1) {
			printf
			    ("No se pudieron expandir variables de entorno\n");
			// Liberar memoria
			free_all(args, line);
			continue;
		}
		// Ejecución
		int execution = execute_cmd(args);

		if (execution != 0) {
			printf("Error de ejcución\n");
		}
		// Actualizar variable de entorno "result" con status de salida
		char result_str[10];

		sprintf(result_str, "%d", execution);
		setenv_var("result", result_str);

		// Liberar memoria
		free_all(args, line);
	}
	exit(EXIT_SUCCESS);

}
