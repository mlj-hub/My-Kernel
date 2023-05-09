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

struct cpu cpus[8];

struct thread * idle = NULL;

static struct thread * thread_get_ready();
static void kernel_thread();
struct cpu * mycpu();


void
thread_init(){
    list_init(&all_list); 
    list_init(&ready_list);
    initlock(&tid_lock,"tid lock");
    initlock(&all_list_lock,"all list lock");
    initlock(&ready_list_lock,"ready list lock");
}

void
idle_thread(void * _){
    for(;;);
}

void 
thread_idle_init(){
    if(r_tp()==0)
        thread_create("hart0_idle",0,idle_thread,NULL);
    else
        thread_create("hart1_idle",0,idle_thread,NULL);
}

tid_t
thread_create(char * name, int priority,thread_func* function,void * arg){
    tid_t ret_tid ;
    acquire(&tid_lock);
    ret_tid = next_tid++;
    release(&tid_lock);

    struct thread * t =(struct thread *) kalloc(); 

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
    
    if(!idle)
        idle = t;
    
    list_push_back(&all_list,&t->all_elem);
    list_push_back(&ready_list,&t->elem);

    return ret_tid;
}

void
thread_exit(){
    struct thread * cur_thread = mycpu()->cur_thread;
    cur_thread->status = DYING;
    acquire(&all_list_lock);
    list_remove(&cur_thread->all_elem);
    release(&all_list_lock);

}

static void
kernel_thread(){
    struct thread * t = mycpu()->cur_thread;
    t->kernel_thread_frame.function(t->kernel_thread_frame.arg);
    thread_exit();
}

static struct thread *
thread_get_ready(){
    acquire(&ready_list_lock);
    if(list_empty(&ready_list)){
        release(&ready_list_lock);
        return idle;
    }
    struct thread * t = list_entry(list_pop_front(&ready_list),struct thread, elem);
    release(&ready_list_lock);
    return t;
}

void
thread_yield(){
    struct thread * next_thread  = thread_get_ready();
    struct thread * cur_thread  = mycpu()->cur_thread;
    
    if(next_thread != cur_thread){
        mycpu()->cur_thread = next_thread;
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

// void
// proc_mapstacks(pagetable_t kpgtbl)
// {
//   struct proc *p;
  
//   for(p = proc; p < &proc[NPROC]; p++) {
//     char *pa = kalloc();
//     if(pa == 0)
//       panic("kalloc");
//     uint64 va = KSTACK((int) (p - proc));
//     kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
//   }
// }