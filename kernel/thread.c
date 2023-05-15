#include "thread.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"

static tid_t next_tid = 0;
struct spinlock tid_lock;

static struct list all_list;
static struct spinlock all_list_lock;

static struct list hart0_ready_list;
static struct list hart1_ready_list;
static struct spinlock hart0_ready_list_lock;
static struct spinlock hart1_ready_list_lock;

struct cpu cpus[8];

struct thread * hart0_idle = NULL;
struct thread * hart1_idle = NULL;

static struct thread * thread_get_ready();
static void kernel_thread();
struct cpu * mycpu();
void dump_list(struct list * target_list);


void
thread_init(){
    list_init(&all_list); 
    list_init(&hart0_ready_list);
    list_init(&hart1_ready_list);
    initlock(&tid_lock,"tid lock");
    initlock(&all_list_lock,"all list lock");
    initlock(&hart0_ready_list_lock,"hart0 ready list lock");
    initlock(&hart1_ready_list_lock,"hart1 ready list lock");
}

void
idle_thread(void * _){
    for(;;);
}

void thread_test(void * _){
    printf("name: %s, tid:%d, cpuid:%d\n",thread_current()->name,thread_current()->tid,cpuid());
    for(;;);
}

void 
thread_idle_init(){
    if(cpuid()==0){
        hart0_idle = thread_create("hart0_idle",0,idle_thread,NULL);
        char test[5]="test0";
        for(int i=0;i<8;i++){
            test[4]=(char)(97+i);
            thread_create(test,10,thread_test,NULL);
        }
    }
    else
        hart1_idle = thread_create("hart1_idle",0,idle_thread,NULL);
}

struct thread *
thread_create(char * name, int priority,thread_func* function,void * arg){
    tid_t ret_tid ;
    acquire(&tid_lock);
    ret_tid = next_tid++;
    release(&tid_lock);

    struct thread * t =(struct thread *) kalloc(); 
    printf("thread:%s, start address:0x%p\n",name,(void *)t);

    if(!t)
        return NULL;

    t->tid = ret_tid;
    t->priority = priority;
    strncpy(t->name,name,strlen(name)+1);
    initlock(&t->lock,t->name);
    t->status = READY;
    t->context.ra = (uint64_t)kernel_thread;
    t->context.sp = (uint64_t)t+PGSIZE;
    t->kernel_stack = (uint8_t *)t+PGSIZE;
    t->kernel_thread_frame.function = function;
    t->kernel_thread_frame.arg = arg; 
    
    list_push_back(&all_list,&t->all_elem);
    if(ret_tid%2 == 0)
        list_push_back(&hart0_ready_list,&t->elem);
    if(ret_tid%2 == 1)
        list_push_back(&hart1_ready_list,&t->elem);

    return t;
}

void
thread_exit(){
    struct thread * cur_thread = mycpu()->cur_thread;
    cur_thread->status = DYING;
    acquire(&all_list_lock);
    list_remove(&cur_thread->all_elem);
    release(&all_list_lock);
    thread_yield();
}

static void
kernel_thread(){
    intr_on();
    struct thread * t = thread_current();
    t->kernel_thread_frame.function(t->kernel_thread_frame.arg);
    thread_exit();
}

static struct thread *
thread_get_ready(){
    struct list * ready_list = NULL;
    if(cpuid() == 0){
        acquire(&hart0_ready_list_lock);
        ready_list = &hart0_ready_list;
    }
    else{
        acquire(&hart1_ready_list_lock);
        ready_list = &hart1_ready_list;
    }


    if(list_empty(ready_list)){
        release(cpuid()==0?&hart0_ready_list_lock:&hart1_ready_list_lock);
        return cpuid()==0?hart0_idle:hart1_idle;
    }
    struct thread * t = list_entry(list_pop_front(ready_list),struct thread, elem);
    release(cpuid()==0?&hart0_ready_list_lock:&hart1_ready_list_lock);
    return t;
}

void
thread_yield(){
    struct thread * next_thread  = thread_get_ready();
    struct thread * cur_thread  = mycpu()->cur_thread;
    
    if(next_thread != cur_thread){
        mycpu()->cur_thread = next_thread;
        list_push_back(cpuid()==0?&hart0_ready_list:&hart1_ready_list,&cur_thread->elem);
        swtch(&cur_thread->context,&next_thread->context);
    }
}

struct cpu *
mycpu(){
    int id = r_tp();
    return cpus+id;
}

int
cpuid()
{
  int id = r_tp();
  return id;
}

struct thread *
thread_current(){
    return mycpu()->cur_thread;
}

void
thread_start(){
    struct context temp_comtext;
    struct thread * next_thread = thread_get_ready();
    mycpu()->cur_thread = next_thread;
    swtch(&temp_comtext,&next_thread->context);
}

void dump_list(struct list * target_list){
    for(struct list_elem *i=list_front(target_list);i!=list_end(target_list);i=list_next(i)){
        printf("name :%s\n",list_entry(i,struct thread,elem)->name);
    }
}