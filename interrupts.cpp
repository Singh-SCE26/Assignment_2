/**
 *
 * @file interrupts.cpp
 * @author Aryan Singh [101270896], Joodi Al-Asaad [101200152]
 *
 */
#include <interrupts.hpp>



int PID_COUNTER = 1;

std::string snapshot_line(int time, const std::string& trace, const PCB& process, const std::vector<PCB>& wait_queue) {
    return std::to_string(time) + ", Snapshot placeholder for process " + std::to_string(process.PID) + "\n";
}

std::tuple<std::string, std::string, int> simulate_trace(
    std::vector<std::string> trace_file, int time, std::vector<std::string> vectors,
    std::vector<int> delays, std::vector<external_file> external_files,
    PCB current, std::vector<PCB> wait_queue) {

    if (time > 10000) {
        std::cerr << "Terminating: Simulation timeout reached.\n";
        return {"", "", time};
    }

    std::string execution = "";
    std::string system_status = "";
    int current_time = time;

    for (size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if (activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } 
        else if (activity == "SYSCALL") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } 
        else if (activity == "END_IO") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } 
        else if (activity == "FORK") {
            auto [intr, t] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            PCB child(PID_COUNTER++, current.PID, current.program_name, current.size, -1);
            if (!allocate_memory(&child)) {
                execution += std::to_string(current_time) + ", 0, ERROR: memory allocation failed for child\n";
                continue;
            }

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

            system_status += snapshot_line(current_time, trace, child, wait_queue);

            // Skip recursion for child to avoid infinite loops
            free_memory(&child);
            execution += std::to_string(current_time) + ", 1, switch to parent and IRET\n";
            current_time++;
        } 
        else if (activity == "EXEC") {
            auto [intr, t] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = t;

            unsigned int prog_size = get_size(program_name, external_files);
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) +
                         ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;

            int load_time = 15 * prog_size;
            execution += std::to_string(current_time) + ", " + std::to_string(load_time) + ", loading program into memory\n";
            current_time += load_time;

            free_memory(&current);
            current.program_name = program_name;
            current.size = prog_size;
            allocate_memory(&current);

            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;
            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

            system_status += snapshot_line(current_time, trace, current, wait_queue);

            // Disable recursive EXEC loading
            execution += std::to_string(current_time) + ", 0, Skipped recursive EXEC load\n";
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    print_external_files(external_files);

    PCB current(0, -1, "init", 1, -1);
    if (!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;
    std::vector<std::string> trace_file;
    std::string trace;
    while (std::getline(input_file, trace)) trace_file.push_back(trace);

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);
    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");
    return 0;
}
