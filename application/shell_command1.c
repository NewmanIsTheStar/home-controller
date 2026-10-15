case HC_CMD_BASIC_INTERACTIVE:
                        basic_Interpreter(IM_INTERACTIVE, NULL, NULL, basic_program_buffer, strlen(basic_program_buffer), false);
                        break;
                    case HC_CMD_BASIC_SCRIPT:
                        basic_program[current_buffer_index++] = 0;
                        basic_Interpreter(IM_EXECUTE_IN_PASSED_RAM_BUFFER, NULL, NULL, basic_program, current_buffer_index, false);                
                        break;
                    case HC_CMD_BASIC_FILE:
                        basic_Interpreter(IM_MMAP_FILE, NULL, web.basic_file_to_execute, NULL, 0, false);                
                        break; 

