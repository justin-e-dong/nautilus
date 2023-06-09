/*
    This is an implemenetation of interrupt playback allowing playback of a set of interrupts and recording
    the time in the interrupt thread and the number of dropped interrupts while also allowing to set
    changes to the interrupt thread period and slice.

    Currently allows only for interrupt playback without interrupt thread parameter changes and without interrupt thread
    timing (to be implemented)

    Pass in the defined inputs TRACE_LENGTH, Number of distinct IRQs, the IRQ numbers, and the trace as INPUT DATA
*/





#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/intersched.h>
#include <nautilus/irq.h>
#include <nautilus/timer.h>
#include <nautilus/idt.h>

// Call nk_sched_thread_change_constraints to modify period and slice

#define TRACE_LENGTH 2
#define NUMBER_OF_DIFFERENT_IRQS 2
#define IRQ_NUMS {1,2}

// INPUT_DATA is defined as the irq number followed by the interrupt start time in the next index and then the interrupt length
#define INPUT_DATA {1, 5, 10, 2, 10000, 15}


// The generic irq handler used for all intersched idt entries
static int intersched_irq_handler (excp_entry_t * et, excp_vec_t ev, void * state);

// setups the interrupt idt entries
static int setup_interrupt();

// runs the entire interrupt schedular test
static void run_intersched_test();

// handler for the inersched shell command
static int handle_intersched (char * buf, void * priv);

// intersched shell command struct implementation
static struct shell_cmd_impl intersched_impl;

// determines the time delay in ns of getting the current clock time
static uint64_t time_delay();

// Contains the address to the first received interrupt vector
// since we acquire them aligned the rest are just first_interrupt_vector + i   
static uint8_t* first_interrupt_vector = 0;

// Contains a list of addreses to each interrupts state, used to set the amount of time to sleep
static uint64_t** interrupt_state;

// maps the input irq's to real vectors
static uint8_t* irq_to_real_vectors;

// counts the total number of interrupts executed
// used to determine if any interrupts were dropped
static uint64_t interrupt_counter;


// setup the interrupt vectors
static int 
intersched_irq_handler (excp_entry_t * et, excp_vec_t ev, void * state) {
    uint64_t time_in_ns = *((uint64_t**)state);
    interrupt_counter++;
    // nk_vc_printf("hanlding interrupt %u of time %u\n", ev, time_in_ns);
    nk_delay(time_in_ns);
    IRQ_HANDLER_END();

    return 0;

}

static int setup_interrupt(){
    // If the first interrupt vector is set, then we already did the setup and we skip the function
    if (first_interrupt_vector ==0){

        // reserve the interrupt vectors
        first_interrupt_vector = malloc(sizeof(ulong_t));
        int ret_val = idt_find_and_reserve_range((ulong_t) NUMBER_OF_DIFFERENT_IRQS, 1, first_interrupt_vector);
        ret_val += 1;

        if (!ret_val) {
            ERROR_PRINT("IDT Entries Could not Be Set\n");

        }
        else{


            irq_to_real_vectors = malloc(sizeof(uint8_t)*NUMBER_OF_DIFFERENT_IRQS);
            uint8_t* tmp_irq[NUMBER_OF_DIFFERENT_IRQS] = IRQ_NUMS;
            interrupt_state = malloc(sizeof(ulong_t)*256);

            for (size_t i=0; i< NUMBER_OF_DIFFERENT_IRQS; i++){
                // set the mapping of input irq to real interrupt vector 
                irq_to_real_vectors[(uint8_t)(tmp_irq[i])] = i + *first_interrupt_vector;

                // allocate space for the interrupt state of the given interrupts
                interrupt_state[(uint8_t)(tmp_irq[i])] = malloc(sizeof(uint64_t));
                ret_val =idt_assign_entry(*(first_interrupt_vector)+i, intersched_irq_handler, interrupt_state[(uint8_t)tmp_irq[i]]);
                ret_val +=1;
                if (!ret_val){
                    ERROR_PRINT("Failed to assign idt entry %u", *first_interrupt_vector + i);
                    return -1;
                }
            }

            nk_vc_printf("IDT Entries Sucesffuly Set \n");
        }
    }
    return 0;

}



static void run_intersched_test(){
    int ret_val = setup_interrupt();

    if (!(ret_val+1)){
        return;
    }



    uint64_t * input_data[TRACE_LENGTH*3] = INPUT_DATA;
    uint64_t avg_time_delay = time_delay();
    long int test_reference_time_ns = (long int)(nk_sched_get_realtime() + 1000U);

    for (size_t i =0; i < TRACE_LENGTH*3; i+=3){
        
        uint8_t irq_num = input_data[i];
        size_t real_vector_num = irq_to_real_vectors[irq_num];
        *(interrupt_state[irq_num])= input_data[i+2];
        
        while (test_reference_time_ns + (long int)input_data[i+1] - (long int)avg_time_delay > (long int)nk_sched_get_realtime()){
        }
        

        // Could also try to time the apic ipi command at some point to get more accurate results
        apic_ipi(
            per_cpu_get(apic),  // "get my apic (sending apic)"
            nk_get_nautilus_info()->sys.cpus[1]->lapic_id, // "get address of destination cpu's apic"
            real_vector_num // send it this vector
        );
    }

    nk_delay(10000000000);
    return;
}

static uint64_t time_delay(){
    uint64_t init_time = nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    nk_sched_get_realtime();
    uint64_t after_time = nk_sched_get_realtime();
    uint64_t avg_time_delay = (after_time-init_time)/10;
    nk_vc_printf("avg time delay is %u\n", avg_time_delay);
    return avg_time_delay;
}


// Registering the shell cmd below

static int handle_intersched (char * buf, void * priv)
{

    nk_vc_printf("intersched shell command processing\n");
    nk_vc_printf("Discard first run as the average time delay changes once paging is complete\n");
    run_intersched_test();
    nk_vc_printf("Test done\n");
    nk_vc_printf("Number of dropped interrupts %u\n", TRACE_LENGTH-interrupt_counter);
    
    return 0;
}

static struct shell_cmd_impl intersched_impl = {
    .cmd = "intersched",
    .help_str = "intersched",
    .handler = handle_intersched,

};


nk_register_shell_cmd(intersched_impl);