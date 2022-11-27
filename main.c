#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio_ext.h>
#include "structures.h"
#include <fcntl.h>

#define MAXLINE 1024
#define MAXARGS 128
#define BOOL char
#define TRUE 1
#define FALSE 0

myqueue *history_q;
FILE *saved_history;

int eval(char *cmdline);
tuple parseline(char *buf, char *buf2, char **arg, int *indexpipes);
void to_string(char *arg, char character, int notOne);
void spaces_free(char *buf, char **argv);
BOOL built_in_command(char *token);
BOOL find_spaces(char *cmd);
int solve(char **cmdline, int total);
int solve2(char *filein, char *cmd, char *op, char *fileout,int total);
void my_solve(char **instruction, const int *pipes, int total, int pipescount);
int myexec(char **arg);
void my_pipe_exec(char *filein, char *cmd1, char *cmd2, char *op, char *fileout);
void cd(char *path);
void help(char *cmd);
void history_print();
void history_save(char *cmd);
void history_load();
char *history_again(int index);

int main() {
    char cmdline[MAXLINE];
    history_q = (myqueue *) malloc(MAXARGS * sizeof(myqueue));
    history_q->first = NULL;
    history_q->last = NULL;
    history_q->counter = 0;
    history_load();
    while (1) {
        __fpurge(stdin);
        printf("my_prompt $ ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin)) {
            exit(0);
        }
        //history
        if (cmdline[0] != ' ' && strstr(cmdline, "again") == NULL) {
            history_save(cmdline);
        }
        int temp = eval(cmdline);
        if (temp == -1) break; //exit
    }
    return 0;
}

int eval(char *cmdline) {
    if (cmdline[0] == '>' || cmdline[0] == '<' || cmdline[0] == '|' || cmdline[0] == '#') return 0;
    char *arg[MAXARGS];
    int indexpipes[MAXARGS];
    char buf[MAXLINE];
    char buf2[MAXLINE];
    strcpy(buf, cmdline);
    strcpy(buf2, cmdline);
    tuple count = parseline(buf, buf2, arg, indexpipes);
    if (count.pipecount == 0) {
        return solve(arg, count.total);
    } else {
        my_solve(arg, indexpipes, count.total, count.pipecount);
        return 0;
    }
}

tuple parseline(char *buf, char *buf2, char **arg, int *indexpipes) {
    int idxlen = 0;
    int idxcad = 0;
    int pipescount = 0;
    buf[strlen(buf) - 1] = ' ';
    buf2[strlen(buf2) - 1] = ' ';
    char *token = strtok(buf, "<>|#");
    if (token != NULL) {
        while (token != NULL) {
            char *name = (char*)malloc(MAXARGS*sizeof(char));
            strcpy(name, token);
            strcat(name, " ");
            arg[idxcad] = name;
            idxcad += 1;
            idxlen += (int) strlen(token);
            if (buf2[idxlen] =='#') break;
            if (buf2[idxlen] == '>') {
                if (buf2[idxlen + 1] == '>') {
                    arg[idxcad] = (char *) malloc(sizeof(char));
                    to_string(arg[idxcad], (char) buf2[idxlen], 1);
                    idxcad += 1;
                    idxlen += 2;
                    token = strtok(NULL, "<>|#");
                    continue;
                }
            }
            if (buf2[idxlen] != '\0') {
                if (buf2[idxlen] == '|') {
                    indexpipes[pipescount] = idxcad;
                    pipescount += 1;
                }
                arg[idxcad] = (char *) malloc(sizeof(char));
                to_string(arg[idxcad], (char) buf2[idxlen], 0);
                idxcad += 1;
                idxlen += 1;
            }
            token = strtok(NULL, "<>|#");
        }
    }
    tuple result;
    result.pipecount = pipescount;
    result.total = idxcad;
    return result;
}

void to_string(char *arg, char character, int notOne) {
    if (notOne == 0) {
        char temp[1] = {character};
        strcpy(arg, temp);
    } else {
        char temp[2] = {character, character};
        strcpy(arg, temp);
    }
}

void spaces_free(char *buf, char **argv) {
    char *delim;
    int argc;
    while (*buf && (*buf == ' '))
        buf++;
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) {
            buf++;
        }
    }
}

BOOL built_in_command(char *token) {
    if (strcmp(token, "cd") == 0 || strcmp(token, "exit") == 0 ||
        strcmp(token, "again") == 0 || strcmp(token, "help") == 0 || strcmp(token, "history") == 0)
        return TRUE;
    return FALSE;
}

BOOL find_spaces(char *cmd) {
    for (int i = 0; i < strlen(cmd); ++i) {
        if (cmd[i] == ' ') return TRUE;
    }
    return FALSE;
}

int solve(char **cmdline, int total) {
    char *op = "";
    char *cmd = cmdline[0];
    char *filein = "";
    char *fileout = "";
    for (int i = total - 1; i >= 0; i--) {
        if (strcmp(cmdline[i], ">") == 0 || strcmp(cmdline[i], ">>") == 0) {
            fileout = cmdline[i + 1];
            op = cmdline[i];
            break;
        }
    }
    for (int i = total - 1; i >= 0; i--) {
        if (strcmp(cmdline[i], "<") == 0) {
            filein = cmdline[i + 1];
            break;
        }
    }
    return solve2(filein, cmd, op, fileout, total);
}

int solve2(char *filein, char *cmd, char *op, char *fileout,int total) {
    char *arg1[MAXARGS];
    for (int i = 0; i < MAXARGS; i++) {
        arg1[i] = NULL;
    }
    int i_file = 0;
    int o_file = 0;
    int in_oldfd = 0;
    int out_oldfd = 0;
    if (find_spaces(cmd) == TRUE) {
        spaces_free(cmd, arg1);
    } else {
        arg1[0] = cmd;
    }

    if (total == 1) {
        if (arg1[0] == NULL) return 0;
        return myexec(arg1);
    }
    if (find_spaces((fileout))) {
        char *temp[MAXARGS];
        spaces_free(fileout, temp);
        if (temp[0] == NULL) return 0;
        fileout = temp[0];
    }
    if (find_spaces((filein))) {
        char *temp[MAXARGS];
        spaces_free(filein, temp);
        if (temp[0] == NULL) return 0;
        filein = temp[0];
    }
    if (strcmp(op, ">") == 0 || strcmp(op, ">>") == 0) {
        if (strcmp(op, ">") == 0) {
            o_file = open(fileout, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            out_oldfd = dup(STDOUT_FILENO);
            dup2(o_file, STDOUT_FILENO);
        } else if (strcmp(op, ">>") == 0) {
            o_file = open(fileout, O_CREAT | O_WRONLY | O_APPEND, 0644);
            out_oldfd = dup(STDOUT_FILENO);
            dup2(o_file, STDOUT_FILENO);
        }
    }
    if (strcmp(filein, "") != 0) {
        in_oldfd = dup(STDIN_FILENO);
        i_file = open(filein, O_RDONLY);
        dup2(i_file, STDIN_FILENO);
        close(i_file);
        myexec(arg1);
        dup2(in_oldfd, STDIN_FILENO);
    } else {
        myexec(arg1);
    }
    if (strcmp(fileout, "") != 0) {
        close(o_file);
        dup2(out_oldfd, STDOUT_FILENO);
    }
    return 0;
}

void my_solve(char **instruction, const int *pipes, int total, int pipescount) {
    int last = 0;
    BOOL next = FALSE;
    char *filein = "";
    for (int i = 0; i < pipescount; i++) {
        filein = "";
        next = FALSE;
        for (int j = pipes[i] - 1; j > last; j--) {
            if (strcmp(instruction[j], "<") == 0) {
                filein = instruction[j +1];
                break;
            }
        }

        for (int j = pipes[i] - 1; j > last; j--) {
            if (strcmp(instruction[j], ">") == 0 || strcmp(instruction[j], ">>") == 0) {
                solve2(filein,instruction[last],instruction[j],instruction[j + 1],total-last);
                return;
            }
        }
        int end;
        if (i == pipescount - 1) end = total - 1;
        else end = pipes[i + 1];
        for (int j = pipes[i] + 1; j < end; j++) {
            if (strcmp(instruction[j], "<") == 0)
            {
                next=TRUE;
                break;
            }
        }
        if(next) continue;
        for (int j = end; j > pipes[i] + 1; j--) {
            if (strcmp(instruction[j], ">") == 0 || strcmp(instruction[j], ">>") == 0) {
                my_pipe_exec(filein, instruction[last], instruction[pipes[i] + 1], instruction[j], instruction[j + 1]);
                return;
            }
        }
        my_pipe_exec(filein, instruction[last], instruction[pipes[i] + 1], "", "");
        last = pipes[i] + 1;
    }
    if(next){
        last =pipes[pipescount-1]+1;
        for (int j = total - 1; j > pipes[pipescount-1]; j--) {
            if (strcmp(instruction[j], "<") == 0) {
                filein = instruction[j + 1];
                break;
            }
        }
        for (int j = total - 1; j > pipes[pipescount-1]; j--) {
            if (strcmp(instruction[j], ">") == 0 || strcmp(instruction[j], ">>") == 0) {
                solve2(filein,instruction[last],instruction[j], instruction[j + 1],total-last);
                return;
            }
        }
    }
}

int myexec(char **arg) {
    if (arg[0] != NULL) {
        if (built_in_command(arg[0]) == TRUE) {
            if (strcmp(arg[0], "exit") == 0) {
                return -1;
            } else if (strcmp(arg[0], "cd") == 0) {
                cd(arg[1]);
                printf("pwd now: %s\n", arg[1]);
            } else if (strcmp(arg[0], "help") == 0) {
                help(arg[1]);
            } else if (strcmp(arg[0], "history") == 0) {
                history_print();
            } else if (strcmp(arg[0], "again") == 0) {
                char *cmdline = history_again(atoi(arg[1]));
                history_save(cmdline);
                return eval(cmdline);
            }
        } else {
            pid_t pid;
            int status;
            pid = fork();
            if (pid < 0) printf("Error,not able to create child process\n");
            if (pid == 0) {
                status = execvp(arg[0], arg);
                if (status) {
                    printf("Not found %s\n", arg[0]);
                    exit(1);
                }
            } else wait(NULL);
        }
    }
    return 0;
}

void my_pipe_exec(char *filein, char *cmd1, char *cmd2, char *op, char *fileout) {
    char *arg1[MAXARGS];
    char *arg2[MAXARGS];
    for (int i = 0; i < MAXARGS; i++) {
        arg1[i] = NULL;
        arg2[i] = NULL;
    }
    if (find_spaces(cmd1) == TRUE) {
        spaces_free(cmd1, arg1);
    } else {
        arg1[0] = cmd1;
    }
    if (find_spaces(cmd2) == TRUE) {
        spaces_free(cmd2, arg2);
    } else arg2[0] = cmd2;

    int oldfd;
    int file;
    int oldin = dup(STDIN_FILENO);
    int oldout = dup(STDOUT_FILENO);
    oldfd = dup(STDIN_FILENO);
    file = open(filein, O_RDONLY);
    dup2(file, STDIN_FILENO);
    close(file);

    pid_t pid;
    int df[2];
    pipe(df);
    pid = fork();
    if (pid < 0) printf("Error,not able to create child process\n");
    if (pid == 0) {
        close(df[0]);
        dup2(df[1], STDOUT_FILENO);
        int status = execvp(arg1[0], arg1);
        if (status) {
            printf("Not found %s\n", arg1[0]);
            exit(1);
        }
        close(df[1]);
    } else {
        wait(NULL);
        close(df[1]);
        if (strcmp(op, ">") == 0) {
            file = open(fileout, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            dup2(file, STDOUT_FILENO);
        } else if (strcmp(op, ">>") == 0) {
            file = open(fileout, O_CREAT | O_WRONLY | O_APPEND, 0644);
            dup2(file, STDOUT_FILENO);
        }
        oldfd = dup(STDIN_FILENO);
        pid = fork();
        dup2(df[0], STDIN_FILENO);
        close(df[0]);
        if (pid == 0) {
            int status = execvp(arg2[0], arg2);
            if (status) {
                printf("Not found %s\n", arg2[0]);
                exit(1);
            }
        } else {
            wait(NULL);
            dup2(oldfd, STDIN_FILENO);
        }
        if (strcmp(filein, "") != 0) dup2(oldin, STDIN_FILENO);
        if (strcmp(op, "") != 0) {
            close(file);
            dup2(oldout, STDOUT_FILENO);
        }
    }
}

void cd(char *path) {
    chdir(path);
}

void help(char *cmd) {
    if(cmd != NULL) {
        if (strcmp(cmd, "basic") == 0) {
            printf("Comandos básicos implementados:\n"
                   "·cd <arg1>:\n"
                   "Cambia la ruta del shell para la carpeta especificada en <arg1>\n\n"
                   "· > , < :\n"
                   "Redirecciona las entradas y salidas estándar desde/hacia ficheros\n\n"
                   "· <arg1> >> <arg2> :\n"
                   "Concatena la salida de <arg1> en <arg2>\n\n"
                   "· <cmd1> | <cmd2> (pipe):\n"
                   "Redirecciona la salida estandar del <cmd1>, despues de ejecutarlo, hacia el <cmd2>\n\n"
                   "· # (comment line):\n"
                   "Ignora los caracteres desde la aparición de la almohadilla hasta el final de la línea\n\n"
                   "· exit:\n"
                   "Termina inmediatamente el shell\n");
            return;
        } else if (strcmp(cmd, "spaces") == 0) {
            printf("Permite al usuario ingresar cualquier cantidad de espacios (incluso ninguno) entre los parámentros, comandos o archivos\n"
                   "Ejemplo: <cmd1>           |<cmd2> \n"
                   "Sería igual a hacer: <cmd1> | <cmd2>\n");
            return;
        } else if (strcmp(cmd, "history") == 0) {
            printf("Permite al usuario ver el historial de los 10 últimos comandos usados hasta el momento.\n"
                   "Además permite mediante el comando ¨again <n>¨ ejecutar la n-ésima operación del historial\n"
                   "Ejemplo: again 10\n"
                   "No guarda en el history el comando si se entra un espacio (' ') antes del comando\n");
            return;
        }
    }
    printf("Integrantes (C-211):\n"
               "·Félix Daniel Acosta Rodríguez\n"
               "·Dianelys Cruz Mengana\n"
               "·Jordan Plá González\n\n"
               "Funcionalidades:\n"
               "·basic: funcionalidades básicas (3 puntos)\n"
               "·spaces: permite cualquier cantidad de espacios entre los comandos y parámetros (0.5 puntos)\n"
               "·history: permite imprimir los últimos 10 comandos ejecutados (0.5 puntos)\n"
               "·help: ayuda (1 punto)\n\n"
               "Comandos built-in:\n"
               "·cd: cambia de directorios\n"
               "·exit: termina el shell\n"
               "·help: muestra esta ayuda, o la ayuda específica de algun comando usando ¨help <keyword>¨\n"
               "Total: 5.0 puntos\n");
}

void history_save(char *cmd) {
    char *temp = (char *) malloc(MAXLINE * sizeof(char));
    strcpy(temp, cmd);
    enqueue(history_q, temp);
    if (history_q->counter > 10) {
        dequeue(history_q);
    }
    saved_history = fopen("saved_history.txt", "w+");
    node_t *actualnode = history_q->first;
    while (actualnode != NULL) {
        fprintf(saved_history, "%s", actualnode->cmd);
        actualnode = actualnode->next;
    }
    fclose(saved_history);
}

void history_load() {
    saved_history = fopen("saved_history.txt", "r+");
    if (saved_history != NULL) {
        char *command = NULL;
        size_t len = 0;
        while (getline(&command, &len, saved_history) != -1) {
            char *temp = (char *) malloc(MAXLINE * sizeof(char));
            strcpy(temp, command);
            enqueue(history_q, temp);
            if (history_q->counter > 10)
                dequeue(history_q);
        }
        fclose(saved_history);
    }
}

char *history_again(int index) {
    int i = 1;
    char *cmd;
    node_t *actualnode = history_q->first;
    while (i < 11) {
        if (i == index) {
            cmd = actualnode->cmd;
            return cmd;
        }
        actualnode = actualnode->next;
        i++;
    }
    return NULL;
}

void history_print() {
    node_t *cmd = history_q->first;
    for (int i = 0; i < history_q->counter; ++i) {
        printf("%i --> %s", i + 1, cmd->cmd);
        cmd = cmd->next;
    }
}