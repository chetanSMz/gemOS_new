#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>

///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{	
	if(count == 0){
		return -EINVAL;	// because the buffer is invalid, there can't be a buffer of length 0
	}

	struct exec_context *ctx = get_current_ctx();
	if(ctx == NULL){
		return -EINVAL;
	}

	int found = 0;
	int allowed = -1;

	// checking if buffer is present in virtual memory area
	for(int i = 0; i < (MAX_MM_SEGS - 1); i++){
		if((buff >= ctx -> mms[i].start) && ((buff + count - 1) <= ((ctx -> mms[i].next_free) - 1))){
			found = 1;
			unsigned int mms_access_flags = ctx -> mms[i].access_flags;

			if(mms_access_flags & (1 << access_bit)){
				allowed = 1;
			}
		}
	}

	if(!found){
		if((buff >= ctx -> mms[MM_SEG_STACK].start) && ((buff + count - 1) <= ((ctx -> mms[MM_SEG_STACK].end) - 1))){
			found = 1;
			unsigned int mms_access_flags = ctx -> mms[MM_SEG_STACK].access_flags;

			if(mms_access_flags & (1 << access_bit)){
				allowed = 1;
			}
		}
	}

	// checking if buffer is present in vm_area
	if(!found){
		struct vm_area *Vm_area = ctx -> vm_area;
		while(found == 0 && Vm_area != NULL){
			if((buff >= Vm_area -> vm_start) && ((buff + count - 1) <= ((Vm_area -> vm_end) - 1))){
				found = 1;
				unsigned int Vm_access_flags = Vm_area -> access_flags;

				if(Vm_access_flags & (1 << access_bit)){
					allowed = 1;
				}
			}

			Vm_area = Vm_area -> vm_next;
		}
	}

	if(allowed != 1){
		return -EINVAL;
	}

	else if(allowed == 1){
		return 1;
	}

	return -EINVAL;
}

long trace_buffer_close(struct file *filep)
{	
	if(filep == NULL){
		return -EINVAL;
	}

	if(filep -> trace_buffer == NULL || filep -> type != TRACE_BUFFER){
		return -EINVAL;
	}

	if(filep -> fops == NULL){
		return -EINVAL;
	}

	os_page_free(USER_REG, filep -> trace_buffer -> data_filled);
	os_page_free(USER_REG, filep -> trace_buffer -> buffer);

	os_page_free(USER_REG, filep -> fops);
	os_page_free(USER_REG, filep -> trace_buffer);
	os_page_free(USER_REG, filep);

	return 0;	
}

int trace_buffer_read(struct file *filep, char *buff, u32 count)
{	
	if(buff == NULL){
		return -EBADMEM;
	}

	unsigned long buff_addr = (unsigned long)buff;
	
	if(is_valid_mem_range(buff_addr, count, 1) == -EINVAL){
		return -EBADMEM;
	}

	if(filep == NULL){
		return -EINVAL;
	}

	if(filep -> trace_buffer == NULL || filep -> type != TRACE_BUFFER){
		return -EINVAL;
	}

	if(filep -> mode != O_READ && filep -> mode != O_RDWR){
		return -EINVAL;
	}

	if(filep -> fops == NULL || filep -> fops -> read == NULL){			// need to check whether to add or not
		return -EINVAL;
	}

	if(count == 0){
		return 0;
	}

	int bytes_read = 0;

	struct trace_buffer_info *trace_bufferp = filep -> trace_buffer;
	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	unsigned int Count = count;
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;

	while(Count > 0){
		if(Data_filled[Read_offset] == '1'){
			Data_filled[Read_offset] = '0';
			buff[bytes_read] = Buffer[Read_offset];
			Read_offset = ((Read_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			Count--;
			bytes_read++;
		}

		else{
			break;
		}
	}

	trace_bufferp -> read_offset = Read_offset;

	return bytes_read;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{	
	if(buff == NULL){
		return -EBADMEM;
	}

	unsigned long buff_addr = (unsigned long)buff;

	if(is_valid_mem_range(buff_addr, count, 0) == -EINVAL){
		return -EBADMEM;
	}
	
	if(filep == NULL){
		return -EINVAL;
	}

	if(filep -> trace_buffer == NULL || filep -> type != TRACE_BUFFER){
		return -EINVAL;
	}

	if(filep -> mode != O_WRITE && filep -> mode != O_RDWR){
		return -EINVAL;
	}

	if(filep -> fops == NULL || filep -> fops -> write == NULL){			// need to check whether to add or not
		return -EINVAL;
	}

	if(count == 0){
		return 0;
	}

	int bytes_written = 0;

	struct trace_buffer_info *trace_bufferp = filep -> trace_buffer;
	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	unsigned int Count = count;
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;

	int ctr = 0;
	while(Count > 0){
		if(Data_filled[Write_offset] == '0'){
			Data_filled[Write_offset] = '1';
			Buffer[Write_offset] = buff[ctr];
			Write_offset = ((Write_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			ctr++;
			Count--;
			bytes_written++;
		}

		else{
			break;
		}
	}

	trace_bufferp -> write_offset = Write_offset;

	return bytes_written;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{	
	if (mode != O_READ && mode != O_WRITE && mode != O_RDWR){
        return -EINVAL;
    }
	
	// finding the first empty file descriptor
	int fileDescriptor = -EINVAL;

	for(int i = 0; i < MAX_OPEN_FILES; i++){
		if(current->files[i] == NULL){
			fileDescriptor = i;
			break;
		}
	}

	if(fileDescriptor == -EINVAL){
		return -EINVAL;
	}

	// allocating and initialising file object
	struct file *filep = (struct file *)os_page_alloc(USER_REG);
	if(filep == NULL){
		return -ENOMEM;
	}
	filep -> type = TRACE_BUFFER;
	filep -> mode = mode;
	filep -> offp = 0;
	filep -> ref_count = 1;
	filep -> inode = NULL;
	filep -> trace_buffer = NULL;
	filep -> fops = NULL;

	// allocating and intialising trace_buffer
	struct trace_buffer_info *trace_bufferp = (struct trace_buffer_info *)os_page_alloc(USER_REG);
	if(trace_bufferp == NULL){
		os_page_free(USER_REG, filep);
		return -ENOMEM;
	}
	trace_bufferp -> read_offset = 0;
	trace_bufferp -> write_offset = 0;
	
	char *BUFFER = (char *)os_page_alloc(USER_REG);
	trace_bufferp -> buffer = BUFFER;
	char *DATA_FILLED = (char *)os_page_alloc(USER_REG);
	trace_bufferp -> data_filled = DATA_FILLED;

	for(int i = 0; i < TRACE_BUFFER_MAX_SIZE; i++){
		DATA_FILLED[i] = '0';
	}

	// modifying filep to point to trace_bufferp
	filep -> trace_buffer = trace_bufferp;

	// allocating fileops object and modifying filep to point to fopsp
	struct fileops * fops_pointer = (struct fileops *)os_page_alloc(USER_REG);
	if(fops_pointer == NULL){
		os_page_free(USER_REG, filep);
		os_page_free(USER_REG, trace_bufferp);
		return -ENOMEM;
	}
	filep -> fops = fops_pointer;

	// Initialize read, write, and close function pointers in fileops
    filep->fops->read = trace_buffer_read;
    filep->fops->write = trace_buffer_write;
    filep->fops->lseek = NULL;
    filep->fops->close = trace_buffer_close;

	// modifying files array in current exec_context such that the fileDescriptor index in files array points to
	// filep
	current -> files[fileDescriptor] = filep;

	return fileDescriptor;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int no_of_args(u64 syscall_num){
	switch(syscall_num){
		case SYSCALL_EXIT:
			return 1;
			break;

		case SYSCALL_GETPID:
			return 0;
			break;

		case SYSCALL_EXPAND:
			return 2;
			break;

		case SYSCALL_SHRINK:	//
			return 0;		
			break;

		case SYSCALL_ALARM:		
			return 0;
			break;

		case SYSCALL_SLEEP:
			return 1;
			break;

		case SYSCALL_SIGNAL:
			return 2;
			break;

		case SYSCALL_CLONE:
			return 2;
			break;

		case SYSCALL_FORK:
			return 0;
			break;

		case SYSCALL_STATS:		
			return 0;
			break;

		case SYSCALL_CONFIGURE:
			return 1;
			break;

		case SYSCALL_PHYS_INFO:	
			return 0;
			break;

		case SYSCALL_DUMP_PTT:		
			return 1;
			break;

		case SYSCALL_CFORK:
			return 0;
			break;

		case SYSCALL_MMAP:
			return 4;
			break;

		case SYSCALL_MUNMAP:
			return 2;
			break;

		case SYSCALL_MPROTECT:
			return 3;
			break;

		case SYSCALL_PMAP:
			return 1;
			break;

		case SYSCALL_VFORK:
			return 0;
			break;

		case SYSCALL_GET_USER_P:		
			return 0;
			break;

		case SYSCALL_GET_COW_F:		
			return 0;
			break;

		case SYSCALL_OPEN:
			return 2;
			break;

		case SYSCALL_READ:
			return 3;
			break;

		case SYSCALL_WRITE:
			return 3;
			break;

		case SYSCALL_DUP:
			return 1;
			break;

		case SYSCALL_DUP2:
			return 2;
			break;

		case SYSCALL_CLOSE:
			return 1;
			break;

		case SYSCALL_LSEEK:
			return 3;
			break;

		case SYSCALL_FTRACE:
			return 4;
			break;

		case SYSCALL_TRACE_BUFFER:		
			return 1;
			break;

		case SYSCALL_START_STRACE:
			return 2;
			break;

		case SYSCALL_END_STRACE:
			return 0;
			break;

		case SYSCALL_READ_STRACE:
			return 3;
			break;

		case SYSCALL_STRACE:		
			return 2;
			break;

		case SYSCALL_READ_FTRACE:
			return 3;
			break;

		case SYSCALL_GETPPID:		
			return 0;
			break;

		default:
			return -1;
			break;
	}

return 0;
}

int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{	
	struct exec_context *current = get_current_ctx();
	if(current == NULL){
		return 0;
	}

	if(current -> st_md_base == NULL || current -> st_md_base -> is_traced == 0){
		return 0;
	}

	if(current -> st_md_base -> tracing_mode != FILTERED_TRACING && current -> st_md_base -> tracing_mode != FULL_TRACING){
		return 0;
	}

	if(current -> st_md_base -> tracing_mode == FILTERED_TRACING){
		struct strace_info *temp = current -> st_md_base -> next;
		int found = 0;
		while(temp != NULL){
			if(temp -> syscall_num == syscall_num){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 0){
			return 0;
		}
	}

	if(current -> st_md_base -> tracing_mode == FULL_TRACING){
		if(syscall_num == SYSCALL_START_STRACE || syscall_num == SYSCALL_END_STRACE){
			return 0;
		}
	}

	int args = no_of_args(syscall_num);

	if(args != 0 && args != 1 && args != 2 && args != 3 && args != 4){
		return 0;
	}
	
	u64 *data = os_alloc(sizeof(u64)*(args+2));		// 1 syscall + 1 delimiter along with args
	if(data == NULL){
		return 0;
	}

	data[0] = syscall_num;

	if(args == 1){
		data[1] = param1;
	}

	else if(args == 2){
		data[1] = param1;
		data[2] = param2;
	}

	else if(args == 3){
		data[1] = param1;
		data[2] = param2;
		data[3] = param3;
	}

	else if(args == 4){
		data[1] = param1;
		data[2] = param2;
		data[3] = param3;
		data[4] = param4;
	}

	data[args+1] = 8821828609932355683;		// delimiter

	char *syscall_info = (char *)data;

	int fd = current -> st_md_base -> strace_fd;
	struct trace_buffer_info *trace_bufferp = current -> files[fd] -> trace_buffer;

	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	unsigned int Count = (sizeof(u64)*(args+2));
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;

	int ctr = 0;
	int bytes_written = 0;
	while(Count > 0){
		if(Data_filled[Write_offset] == '0'){
			Data_filled[Write_offset] = '1';
			Buffer[Write_offset] = syscall_info[ctr];
			bytes_written++;
			Write_offset = ((Write_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			ctr++;
			Count--;			
		}

		else{
			break;
		}
	}

	trace_bufferp -> write_offset = Write_offset;
	os_free(data, sizeof(u64)*(args+2));

    return 0;
}

int sys_strace(struct exec_context *current, int syscall_num, int action)
{	
	if(current -> st_md_base == NULL){
		struct strace_head *strace_headP = os_alloc(sizeof(struct strace_head));
		if(strace_headP == NULL){
			return -EINVAL;
		}
		current -> st_md_base = strace_headP;
		strace_headP -> count = 0;
		strace_headP -> is_traced = 0;
		strace_headP -> strace_fd = -1;
		strace_headP -> tracing_mode = -1;
		strace_headP -> next = NULL;
		strace_headP -> last = NULL;
	}

	int found = 0;

	if(action == REMOVE_STRACE){
		struct strace_info *temp = current -> st_md_base -> next;
		if(temp == NULL){
			return -EINVAL;
		}

		if(temp -> syscall_num == syscall_num){
			found = 1;
			current -> st_md_base -> next = temp -> next;
			temp -> next = NULL;
			os_free(temp, sizeof(struct strace_info));
			current -> st_md_base -> count--;
		}

		else{
			while(temp -> next != NULL){
				if(temp -> next -> syscall_num == syscall_num){
					found = 1;
					struct strace_info *chunk = temp -> next;
					temp -> next = chunk -> next;
					chunk -> next = NULL;
					os_free(chunk, sizeof(struct strace_info));
					current -> st_md_base -> count--;
					break;
				}
				temp = temp -> next;
			}
		}

		if(!found){
			return -EINVAL;
		}
	}

	else if(action == ADD_STRACE){

		struct strace_info *temp = current -> st_md_base -> next;
		int count = 0;
		if(temp != NULL){
			while(temp != NULL){
				if(temp == current -> st_md_base -> last){
					count++;
					break;
				}
				count++;
				temp = temp -> next;
			}

			if(count >= STRACE_MAX){
				return -EINVAL;
			}
		}

		found = 0;
		temp = current -> st_md_base -> next;
		while(temp != NULL){
			if(temp -> syscall_num == syscall_num){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 1){
			return -EINVAL;
		}

		struct strace_info *strace_infoChunk = os_alloc(sizeof(struct strace_info));
		if(strace_infoChunk == NULL){
			return -EINVAL;
		}
		strace_infoChunk -> syscall_num = syscall_num;
		strace_infoChunk -> next = NULL;

		struct strace_head *strace_hP = current -> st_md_base;

		if(strace_hP -> last == NULL){
			strace_hP -> next = strace_infoChunk;
			strace_hP -> last = strace_infoChunk;
			current -> st_md_base -> count++;
		}

		else if(strace_hP -> last != NULL){
			strace_hP -> last -> next = strace_infoChunk;
			strace_hP -> last = strace_hP -> last -> next;	
			current -> st_md_base -> count++;
		}
	}
	
	else{
		return -EINVAL;
	}

	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{	
	if(count < 0){
		return -EINVAL;
	}

	if(filep == NULL){
		return -EINVAL;
	}

	if(filep -> trace_buffer == NULL || filep -> type != TRACE_BUFFER){
		return -EINVAL;
	}

	if(filep -> mode != O_READ && filep -> mode != O_RDWR){
		return -EINVAL;
	}

	if(filep -> fops == NULL || filep -> fops -> read == NULL){	
		return -EINVAL;
	}

	if(count == 0){
		return 0;
	}

	int bytes_read = 0;

	struct trace_buffer_info *trace_bufferp = filep -> trace_buffer;
	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	unsigned int Count = count;
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;

	u64 *nums_present = (u64 *)Buffer;

	while(Count > 0){
		if(Data_filled[Read_offset] == '1'){
			if(bytes_read % 8 == 0){
				if(*(nums_present) == 8821828609932355683){
					Count--;
					for(int i = 0; i < 8; i++){
						Data_filled[Read_offset + i] = '0';
					}
					Read_offset = ((Read_offset + 8) % TRACE_BUFFER_MAX_SIZE);
					nums_present = nums_present + 1;
					continue;
				}
				nums_present = nums_present + 1;
			}

			Data_filled[Read_offset] = '0';
			buff[bytes_read] = Buffer[Read_offset];
			Read_offset = ((Read_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			bytes_read++;

		}

		else{
			break;
		}
	}

	trace_bufferp -> read_offset = Read_offset;

	return bytes_read;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{	
	if(current -> st_md_base == NULL){
		struct strace_head *strace_head_pointer = os_alloc(sizeof(struct strace_head));
		if(strace_head_pointer == NULL){
			return -EINVAL;
		}
		current -> st_md_base = strace_head_pointer;
		strace_head_pointer -> count = 0;
		strace_head_pointer -> next = NULL;
		strace_head_pointer -> last = NULL;
	}

	current -> st_md_base -> is_traced = 1;
	current -> st_md_base -> strace_fd = fd;
	current -> st_md_base -> tracing_mode = tracing_mode;

	return 0;
}

int sys_end_strace(struct exec_context *current)
{	
	struct strace_head *strace_head_pointer = current -> st_md_base;
	if(strace_head_pointer == NULL){
		return -EINVAL;
	}

	struct strace_info *NEXT = strace_head_pointer -> next;
	strace_head_pointer -> next = NULL;
	strace_head_pointer -> last = NULL;

	while(NEXT != NULL){
		struct strace_info *temp = NEXT;
		NEXT = NEXT -> next;
		os_free(temp, sizeof(struct strace_info));
	}

	os_free(strace_head_pointer, sizeof(struct strace_head));
	current -> st_md_base = NULL;

	return 0;
}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{	
	if(ctx == NULL){
		return -EINVAL;
	}

	if(ctx -> ft_md_base == NULL){
		struct ftrace_head *ftrace_head_pointer = os_alloc(sizeof(struct ftrace_head));
		if(ftrace_head_pointer == NULL){
			return -EINVAL;
		}

		ctx -> ft_md_base = ftrace_head_pointer;
		ftrace_head_pointer -> count = 0;
		ftrace_head_pointer -> next = NULL;
		ftrace_head_pointer -> last = NULL;	
	}

	if(action == ADD_FTRACE){
		struct ftrace_info *temp = ctx -> ft_md_base -> next;

		int count = 0;
		if(temp != NULL){
			while(temp != NULL){
				if(temp == ctx -> ft_md_base -> last){
					count++;
					break;
				}
				count++;
				temp = temp -> next;
			}

			if(count >= FTRACE_MAX){
				return -EINVAL;
			}
		}

		int found = 0;
		temp = ctx -> ft_md_base -> next;
		while(temp != NULL){
			if(temp -> faddr == faddr){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 1){
			return -EINVAL;
		}

		struct ftrace_info *ftrace_infoChunk = os_alloc(sizeof(struct ftrace_info));
		if(ftrace_infoChunk == NULL){
			return -EINVAL;
		}
		ftrace_infoChunk -> faddr = faddr;
		ftrace_infoChunk -> num_args = nargs;
		ftrace_infoChunk -> next = NULL;
		ftrace_infoChunk -> fd = fd_trace_buffer;
		ftrace_infoChunk -> capture_backtrace = 0;

		struct ftrace_head *ftrace_head_pointer = ctx -> ft_md_base;

		if(ftrace_head_pointer -> last == NULL){
			ftrace_head_pointer -> next = ftrace_infoChunk;
			ftrace_head_pointer -> last = ftrace_infoChunk;
			ctx -> ft_md_base -> count++;
		}

		else if(ftrace_head_pointer -> last != NULL){
			ftrace_head_pointer -> last -> next = ftrace_infoChunk;
			ftrace_head_pointer -> last = ftrace_head_pointer -> last -> next;
			ctx -> ft_md_base -> count++;
		}
	}

	else if(action == REMOVE_FTRACE){
		//If tracing is enabled on this function, then, disable the tracing on the 
		// function before removing its information from the list of functions

		int found = 0;
		struct ftrace_info *temp = ctx -> ft_md_base -> next;

		if(temp == NULL){
			return -EINVAL;
		}

		if(temp -> faddr == faddr){
			found = 1;
			ctx -> ft_md_base -> next = temp -> next;
			temp -> next = NULL;

			// disable ftrace and backtrace if enabled
			u8 *mem = (u8 *)faddr;						// check
			if(*mem == INV_OPCODE && *(mem + 1) == INV_OPCODE && *(mem + 2) == INV_OPCODE && *(mem + 3) == INV_OPCODE){
				*mem = temp -> code_backup[0];
				*(mem + 1) = temp -> code_backup[1];
				*(mem + 2) = temp -> code_backup[2];
				*(mem + 3) = temp -> code_backup[3];
			}

			temp -> capture_backtrace = 0;

			os_free(temp, sizeof(struct ftrace_info));
			ctx -> ft_md_base -> count--;
		}

		else{
			while(temp -> next != NULL){
				if(temp -> next -> faddr == faddr){
					found = 1;
					struct ftrace_info *chunk = temp -> next;
					temp -> next = chunk -> next;
					chunk -> next = NULL;
					
					// disable ftrace and backtrace if enabled
					u8 *mem = (u8 *)faddr;						// check
					if(*mem == INV_OPCODE && *(mem + 1) == INV_OPCODE && *(mem + 2) == INV_OPCODE && *(mem + 3) == INV_OPCODE){
						*mem = temp -> code_backup[0];
						*(mem + 1) = temp -> code_backup[1];
						*(mem + 2) = temp -> code_backup[2];
						*(mem + 3) = temp -> code_backup[3];
					}

					chunk -> capture_backtrace = 0;

					os_free(chunk, sizeof(struct ftrace_info));
					ctx -> ft_md_base -> count--;
					break;
				}
				temp = temp -> next;
			}
		}

		if(!found){
			return -EINVAL;
		}
	}

	else if(action == ENABLE_FTRACE){
		struct ftrace_info *temp = ctx -> ft_md_base -> next;
		int found = 0;
		while(temp != NULL){
			if(temp -> faddr == faddr){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 0){
			return -EINVAL;
		}

		u8 *mem = (u8 *)faddr;							// check

		// enable ftrace if not enabled
		if(*mem != INV_OPCODE || *(mem + 1) != INV_OPCODE || *(mem + 2) != INV_OPCODE || *(mem + 3) != INV_OPCODE){		// check if the condition should be && or ||
			temp -> code_backup[0] = *mem;
			temp -> code_backup[1] = *(mem + 1);
			temp -> code_backup[2] = *(mem + 2);
			temp -> code_backup[3] = *(mem + 3);
			*mem = INV_OPCODE;
			*(mem + 1) = INV_OPCODE;
			*(mem + 2) = INV_OPCODE;
			*(mem + 3) = INV_OPCODE;
		}

	return 0;
	}

	else if(action == DISABLE_FTRACE){
		struct ftrace_info *temp = ctx -> ft_md_base -> next;
		int found = 0;
		while(temp != NULL){
			if(temp -> faddr == faddr){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 0){
			return -EINVAL;
		}

		// disable ftrace if enabled
		u8 *mem = (u8 *)faddr;						// check
		if(*mem == INV_OPCODE && *(mem + 1) == INV_OPCODE && *(mem + 2) == INV_OPCODE && *(mem + 3) == INV_OPCODE){
			*mem = temp -> code_backup[0];
			*(mem + 1) = temp -> code_backup[1];
			*(mem + 2) = temp -> code_backup[2];
			*(mem + 3) = temp -> code_backup[3];
		}

		return 0;
	}

	else if(action == ENABLE_BACKTRACE){
		struct ftrace_info *temp = ctx -> ft_md_base -> next;
		int found = 0;

		while(temp != NULL){
			if(temp -> faddr == faddr){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 0){
			return -EINVAL;
		}

		// enable ftrace if not enabled
		u8 *mem = (u8 *)faddr;						// check
		if(*mem != INV_OPCODE || *(mem + 1) != INV_OPCODE || *(mem + 2) != INV_OPCODE || *(mem + 3) != INV_OPCODE){		// check if the condition should be && or ||
			temp -> code_backup[0] = *mem;
			temp -> code_backup[1] = *(mem + 1);
			temp -> code_backup[2] = *(mem + 2);
			temp -> code_backup[3] = *(mem + 3);
			*mem = INV_OPCODE;
			*(mem + 1) = INV_OPCODE;
			*(mem + 2) = INV_OPCODE;
			*(mem + 3) = INV_OPCODE;
		}

		// enable backtrace if not enabled
		temp -> capture_backtrace = 1;

		return 0;
	}

	else if(action == DISABLE_BACKTRACE){
		struct ftrace_info *temp = ctx -> ft_md_base -> next;
		int found = 0;

		while(temp != NULL){
			if(temp -> faddr == faddr){
				found = 1;
				break;
			}
			temp = temp -> next;
		}

		if(found == 0){
			return -EINVAL;
		}

		// disable ftrace if enabled
		u8 *mem = (u8 *)faddr;						// check
		if(*mem == INV_OPCODE && *(mem + 1) == INV_OPCODE && *(mem + 2) == INV_OPCODE && *(mem + 3) == INV_OPCODE){
			*mem = temp -> code_backup[0];
			*(mem + 1) = temp -> code_backup[1];
			*(mem + 2) = temp -> code_backup[2];
			*(mem + 3) = temp -> code_backup[3];
		}

		// disable backtrace if enabled
		temp -> capture_backtrace = 0;

		return 0;
	}

	else{
		return -EINVAL;
	}

    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{	
	struct exec_context *ctx = get_current_ctx();
	if(ctx == NULL){
		return -EINVAL;
	}

	if(ctx -> ft_md_base == NULL || ctx -> ft_md_base -> next == NULL){
		return -EINVAL;
	}

	struct ftrace_info *temp = ctx -> ft_md_base -> next;
	while(temp != NULL && temp -> faddr != regs -> entry_rip){			// check
		temp = temp -> next;
	}

	if(temp == NULL){
		return -EINVAL;
	}

	u32 args = temp -> num_args;
	unsigned long addr = temp -> faddr;
	int fd = temp -> fd;

	u64 *data = os_alloc(sizeof(u64)*(args+1));		// 1 faddr along with args
	if(data == NULL){
		return -EINVAL;
	}
	
	data[0] = addr;

	if(args == 1){
		data[1] = regs -> rdi;
	}

	else if(args == 2){
		data[1] = regs -> rdi;
		data[2] = regs -> rsi;
	}

	else if(args == 3){
		data[1] = regs -> rdi;
		data[2] = regs -> rsi;
		data[3] = regs -> rdx;
	}

	else if(args == 4){
		data[1] = regs -> rdi;
		data[2] = regs -> rsi;
		data[3] = regs -> rdx;
		data[4] = regs -> rcx;
	}

	else if(args == 5){
		data[1] = regs -> rdi;
		data[2] = regs -> rsi;
		data[3] = regs -> rdx;
		data[4] = regs -> rcx;
		data[5] = regs -> r8;
	}

	// add backtrace information if enabled

	u64 count = 0;
	u64 *backtrace_data = NULL;

	if(temp -> capture_backtrace == 1){

		if(*((u64 *)regs -> entry_rsp) != END_ADDR){
			count++;	// for *((u64 *)regs -> entry_rsp)
		}
		u64 rbp = regs -> rbp;
		u64 *rbp_pointer = (u64 *)rbp;
		u64 ret = *(rbp_pointer + 1);
		if(ret != END_ADDR){
			count++;
		}

		 while (ret != END_ADDR) {
			rbp = *(rbp_pointer);
			rbp_pointer = (u64 *)rbp;
			ret = *(rbp_pointer + 1); 
			if(ret == END_ADDR){
				break;
			}
			count++;
		}

		backtrace_data = os_alloc(sizeof(u64)*(count+1));		// extra 1 to store the address of the first instruction of the function from which the trace function is called
		if(backtrace_data == NULL){
			return -EINVAL;
		}

		backtrace_data[0] = addr;
		int i = 1;

		u64 rsp = regs -> entry_rsp;
		ret = *((u64 *)rsp);

		if(ret != END_ADDR){
			backtrace_data[i] = ret;
			i++;
		}

		rbp = regs -> rbp;
		rbp_pointer = (u64 *)rbp;
		ret = *(rbp_pointer + 1);

		if(ret != END_ADDR){
			backtrace_data[i] = ret;
			i++;
		}

		 while (i <= count && ret != END_ADDR) {
			rbp = *(rbp_pointer); 
			rbp_pointer = (u64 *)rbp;
			ret = *(rbp_pointer + 1);
			if(ret == END_ADDR){
				break;
			}
			backtrace_data[i] = ret;
			i++;
		}
	}

	if(temp -> capture_backtrace == 0){
		count--;
	}

	u64 *complete_data = os_alloc(sizeof(u64)*(1 + args + count + 1 + 1));
	if(complete_data == NULL){
		return -EINVAL;
	}

	for(int i = 0; i < 1 + args; i++){
		complete_data[i] = data[i];
	}

	for(int i = 0; i < 1 + count; i++){
		complete_data[i+1+args] = backtrace_data[i];
	}

	complete_data[1+args+count+1] = 8821828609932355683;		// delimiter

	char *function_info = (char *)complete_data;

	struct trace_buffer_info *trace_bufferp = ctx -> files[fd] -> trace_buffer;

	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;
	unsigned int Count = (sizeof(u64)*(1 + args + count + 1 + 1));


	int ctr = 0;
	int bytes_written = 0;
	while(Count > 0){
		if(Data_filled[Write_offset] == '0'){
			Data_filled[Write_offset] = '1';
			Buffer[Write_offset] = function_info[ctr];
			bytes_written++;
			Write_offset = ((Write_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			ctr++;
			Count--;			
		}

		else{
			break;
		}
	}

	trace_bufferp -> write_offset = Write_offset;

	// correct the function

	u8 *mem = (u8 *)addr;								// check
	*mem = temp -> code_backup[0];
	*(mem + 1) = temp -> code_backup[1];
	*(mem + 2) = temp -> code_backup[2];
	*(mem + 3) = temp -> code_backup[3];

	os_free(data, sizeof(u64)*(args+1));
	os_free(complete_data, sizeof(u64)*(1 + args + count + 1 + 1));
	if(count > 0){
		os_free(backtrace_data, sizeof(u64)*(count+1));
	}

	return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{	
	if(count < 0){
		return -EINVAL;
	}

	if(filep == NULL){
		return -EINVAL;
	}

	if(filep -> trace_buffer == NULL || filep -> type != TRACE_BUFFER){
		return -EINVAL;
	}

	if(filep -> mode != O_READ && filep -> mode != O_RDWR){
		return -EINVAL;
	}

	if(filep -> fops == NULL || filep -> fops -> read == NULL){	
		return -EINVAL;
	}

	if(count == 0){
		return 0;
	}

	int bytes_read = 0;

	struct trace_buffer_info *trace_bufferp = filep -> trace_buffer;
	int Write_offset = trace_bufferp -> write_offset;
	int Read_offset = trace_bufferp -> read_offset;
	unsigned int Count = count;
	char *Data_filled = trace_bufferp -> data_filled;
	char *Buffer = trace_bufferp -> buffer;

	u64 *nums_present = (u64 *)Buffer;

	while(Count > 0){
		if(Data_filled[Read_offset] == '1'){
			if(bytes_read % 8 == 0){
				if(*(nums_present) == 8821828609932355683){
					Count--;
					for(int i = 0; i < 8; i++){
						Data_filled[Read_offset + i] = '0';
					}
					Read_offset = ((Read_offset + 8) % TRACE_BUFFER_MAX_SIZE);
					nums_present = nums_present + 1;
					continue;
				}
				nums_present = nums_present + 1;
			}

			Data_filled[Read_offset] = '0';
			buff[bytes_read] = Buffer[Read_offset];
			Read_offset = ((Read_offset + 1) % TRACE_BUFFER_MAX_SIZE);
			bytes_read++;

		}

		else{
			break;
		}
	}

	trace_bufferp -> read_offset = Read_offset;

	return bytes_read;    
}




