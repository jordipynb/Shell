#ifndef MYSHELL_STRUCTURES_H
#define MYSHELL_STRUCTURES_H

typedef struct
{
    int pipecount;
    int total;
}tuple;
typedef struct node {
    char *cmd;
    struct node *next;
} node_t;
typedef struct {
    int counter;
    node_t *first;
    node_t *last;
} myqueue;

void enqueue(myqueue *Q, char* cmd);
char *dequeue(myqueue *Q);

void enqueue(myqueue *Q, char* cmd) {
    node_t *newnode = (node_t *) malloc(sizeof (cmd));
    newnode->cmd = cmd;
    newnode->next = NULL;
    if (Q->counter == 0) {
        Q->first = newnode;
        Q->last = newnode;
    } else {
        (Q->last)->next = newnode;
        Q->last = newnode;
    }
    Q->counter++;
}
char *dequeue(myqueue *Q) {
    char *temp = NULL;
    if (Q->counter > 0) {
        temp = Q->first->cmd;
        if (Q->counter == 1)
            free(Q->first);
        else {
            node_t *newfirst = Q->first->next;
            free(Q->first);
            Q->first = newfirst;
        }
        Q->counter -= 1;
    }
    return temp;
}

#endif //MYSHELL_STRUCTURES_H