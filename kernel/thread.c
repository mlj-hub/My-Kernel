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

static struct list ready_list;
static struct spinlock ready_list_lock;

struct list sleep_list;
struct spinlock sleep_list_lock;

struct cpu cpus[8];

struct thread * hart0_idle = NULL;
struct thread * hart1_idle = NULL;

static struct thread * thread_get_ready();
static void kernel_thread();
struct cpu * mycpu();
void dump_list();


void
thread_init(){
    list_init(&all_list); 
    list_init(&ready_list);
    list_init(&sleep_list);
    initlock(&tid_lock,"tid lock");
    initlock(&all_list_lock,"all list lock");
    initlock(&ready_list_lock,"ready list lock");
    initlock(&sleep_list_lock,"sleep list lock");
}

void thread_sleep(int sleep_tick){
    if(sleep_tick <=0)
        return;
    thread_current()->sleep_clock = sleep_tick;
    acquire(&sleep_list_lock);
    list_push_back(&sleep_list,&thread_current()->sleep_elem);
    release(&sleep_list_lock);
    thread_block();
}

void
idle_thread(void * _){
    for(;;);
}

void thread_test(void * _){
    extern uint ticks;
    printf("name: %s, tid:%d, cpuid:%d\n",thread_current()->name,thread_current()->tid,cpuid());
    thread_sleep(thread_current()->tid+1);
    printf("%s wake up after %d ticks, current ticks:%d, cpuid:%d\n",thread_current()->name,thread_current()->tid+1,ticks,cpuid());
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
    // printf("thread:%s, start address:0x%p\n",name,(void *)t);

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
    
    acquire(&all_list_lock);
    list_push_back(&all_list,&t->all_elem);
    release(&all_list_lock);

    acquire(&ready_list_lock);
    list_push_back(&ready_list,&t->elem);
    release(&ready_list_lock);

    return t;
}

void
thread_exit(){
    struct thread * cur_thread = mycpu()->cur_thread;
    cur_thread->status = DYING;
    intr_off();
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
    acquire(&ready_list_lock);
    if(list_empty(&ready_list)){
        release(&ready_list_lock);
        return cpuid()==0?hart0_idle:hart1_idle;
    }
    struct thread * t = list_entry(list_pop_front(&ready_list),struct thread, elem);
    t->status = RUNNING;
    release(&ready_list_lock);
    return t;
}

/*This function should be called when interrupt is disabled*/
void
thread_yield(){
    ASSERT(intr_get() == 0);
    struct thread * next_thread  = thread_get_ready();
    struct thread * cur_thread  = mycpu()->cur_thread;
    struct thread * prev_thread = NULL;
    
    if(next_thread != cur_thread){
        ASSERT(next_thread->status == RUNNING);
        mycpu()->cur_thread = next_thread;

        acquire(&ready_list_lock);
        if(cur_thread->status == RUNNING){
            cur_thread->status = READY;
            list_push_back(&ready_list,&cur_thread->elem);
        }
        release(&ready_list_lock);
        
        prev_thread = swtch(&cur_thread->context,&next_thread->context);
        if(prev_thread==NULL)
            for(;;);
        // swtch(&cur_thread->context,&next_thread->context);
    }
    
    // ASSERT(prev_thread !=NULL);
    
    if(prev_thread->status == DYING){
        printf("thread %s exit\n",prev_thread->name);
        kfree(prev_thread);
    }
}

/*This fucntion will disable the interrupt and block current thread. After the thread is putting back, 
  it will restore the interrupt state.*/
void thread_block(void){
    int old_level = intr_get();
    intr_off();

    thread_current()->status = BLOCKED;
    thread_yield();
    
    intr_set(old_level);
}

/*This function will put the thread into ready state. This function is atomic*/
void thread_unblock(struct thread * t){
    ASSERT(t->status == BLOCKED);
    int old_level = intr_get();
    intr_off();

    acquire(&ready_list_lock);
    t->status = READY;
    list_push_back(&ready_list,&t->elem);
    release(&ready_list_lock);
    
    intr_set(old_level);
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

void dump_list(){
    int cnt=0;
    printf("\n==========ready list==============\n");
    for(struct list_elem *i=list_begin(&ready_list);i!=list_end(&ready_list);i=list_next(i)){
        printf("name :%s\n",list_entry(i,struct thread,elem)->name);
        cnt++;
    }
    printf("ready thread number:%d\n",cnt);
    cnt = 0;
    printf("\n==========all list==============\n");
    for(struct list_elem *i=list_begin(&all_list);i!=list_end(&all_list);i=list_next(i)){
        printf("name :%s\n",list_entry(i,struct thread,all_elem)->name);
        cnt++;
    }
    printf("all thread number:%d\n",cnt);

    cnt=0;
    printf("\n==========sleep list==============\n");
    for(struct list_elem *i=list_begin(&sleep_list);i!=list_end(&sleep_list);i=list_next(i)){
        printf("name :%s\n",list_entry(i,struct thread,sleep_elem)->name);
        cnt++;
    }
    printf("sleep thread number:%d\n",cnt);

    printf("\nhart0 current thread: %s\n",cpus[0].cur_thread->name);
    printf("hart1 current thread: %s\n",cpus[1].cur_thread->name);


}