/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include <interrupts_101260631_101298706.hpp>
const int TIME_QUANTUM = 100;


void round_robin(std::vector<PCB> &ready_queue) {   //basically FCFS
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time > second.arrival_time); 
                } 
            );
}

std::tuple<std::string  > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    PCB running;
    int quantum_remaining = TIME_QUANTUM;
    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;
    // no need to sort firts this time since it's fcfs
    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, assign memory and put the process into the ready queue
                assign_memory(process);

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue
        std::vector <PCB> p_to_remove; //vector to keep track of any processes that are ready and need to be removed 
        for(auto &process : wait_queue) {
            //If the I/O duration is complete, the process is ready 
            if(process.io_duration + process.start_time <= current_time) {
                process.state = READY;
                ready_queue.push_back(process);
                p_to_remove.push_back(process); //keep track of it for later removal
                execution_status += print_exec_status(current_time, process.PID, WAITING, READY);
            }
        }
        //remove the process from the wait queue if it's already been handled
        for(auto &process : p_to_remove) {
            wait_queue.erase(
                std::remove_if(wait_queue.begin(), wait_queue.end(),[&process](const PCB& p) { 
                    return p.PID == process.PID; 
                }),
                wait_queue.end()
            );
        }
        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        round_robin(ready_queue); 
        /////////////////////////////////////////////////////////////////

        //////////////////////////EXCECUTION//////////////////////////////
        //if the cpu is idle and there are processes in the ready queue, run the next process
        if(running.state == NOT_ASSIGNED && !ready_queue.empty()){
            execution_status += print_exec_status(current_time, ready_queue.back().PID, READY, RUNNING);
            run_process(running, job_list, ready_queue, current_time);
            quantum_remaining = TIME_QUANTUM; //reset it if we are starting new process
            if (running.first_run_time == -1) { //record it for later calculations for report
                running.first_run_time = current_time;
                sync_queue(job_list, running);
            }
        }
        //if the cpu is running, move the time forward and then see if it's complete or an I/O needs to happen
        if (running.state == RUNNING) {
            running.remaining_time -= 1; 
            quantum_remaining -= 1;
            //if we finished the task
            if (running.remaining_time == 0) { 
                execution_status += print_exec_status(current_time + 1, running.PID, RUNNING, TERMINATED);
                running.completion_time = current_time + 1; //for report calculations
                terminate_process(running, job_list);
                idle_CPU(running);
            } else if (running.io_freq != 0 && (running.processing_time - running.remaining_time) % running.io_freq == 0) { 
                //is it time for I/O? (depending on io freqency)
                execution_status += print_exec_status(current_time + 1, running.PID, RUNNING, WAITING);
                running.state = WAITING;
                running.start_time = current_time + 1; 
                wait_queue.push_back(running);
                sync_queue(job_list, running);
                idle_CPU(running);
            } else if (quantum_remaining == 0) {
                //if we ran out of the allocated time for the quantum
                //unlike if they finish, dont terminate the process cause we still need to finish it later
                execution_status += print_exec_status(current_time + 1, running.PID, RUNNING, READY);
                running.state = READY;
                ready_queue.push_back(running);
                sync_queue(job_list, running);
                idle_CPU(running);
                quantum_remaining = TIME_QUANTUM; 
            }
        }
        current_time++;
        /////////////////////////////////////////////////////////////////

    }
    
    //Close the output table
    execution_status += print_exec_footer();
    execution_status += resulting_metrics(job_list, current_time);

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}